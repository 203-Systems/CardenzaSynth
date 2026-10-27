#include "Library/Synth/Audio/SynthAudioRuntime.h"
#include "Library/Synth/Core/Engines/Dx7/Dx7FactoryPresets.h"
#include "Library/Synth/Core/Engines/Juno/JunoFactoryPresets.h"
#include "Library/Synth/Core/Engines/Drums/DrumsSynthEngine.h"
#include "Library/Synth/Audio/Amy/FactoryParameters.h"
#include "Library/Synth/Audio/Fx/SynthFxDsp.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#include <unistd.h>
#endif

#if !defined(_WIN32)
#include <pthread.h>
#endif

#if defined(ESP_PLATFORM)
#include <esp_heap_caps.h>
#endif

#if defined(ESP_PLATFORM)
#include <driver/uart.h>
#endif

#if defined(ESP_PLATFORM)
#include <soc/uart_reg.h>
#endif

#if defined(ESP_PLATFORM)
#include <esp_task.h>
#endif

namespace amy_embed {
extern "C" {
#include "amy.h"
// AMY's voice table (patches.c): the first oscillator of every allocated voice.
extern uint16_t* voice_to_base_osc;
}
}  // namespace amy_embed

using amy_embed::amy_add_event;
using amy_embed::amy_config_t;
using amy_embed::amy_default_config;
using amy_embed::amy_default_event;
using amy_embed::amy_event;
using amy_embed::amy_simple_fill_buffer;
using amy_embed::amy_start;
using amy_embed::amy_stop;
using amy_embed::amy_update;
using amy_embed::output_sample_type;

namespace synth::audio {

namespace {

constexpr std::size_t kAmyFramesPerBlock = AMY_BLOCK_SIZE;
constexpr std::size_t kAmyChannelCount = AMY_NCHANS;
constexpr std::size_t kSamplerVoiceCount = 4;
constexpr int kAmyMaxSynths = 8;
constexpr int kAmyMaxVoices = 24;
constexpr int kAmyMaxOscs = 160;
#if defined(ESP_PLATFORM) && defined(CARDENZA_AMY_EXPERIMENTAL_MULTICORE)
constexpr bool kAmyUseExperimentalMulticore = true;
#else
constexpr bool kAmyUseExperimentalMulticore = false;
#endif
constexpr float kOutputGain = 0.75f;
constexpr std::int32_t kAmyOnlyOutputGainNumerator = 3;
constexpr std::int32_t kAmyOnlyOutputGainDenominator = 8;
constexpr float kDx7MaxAmyVelocity = 88.0f / 127.0f;
constexpr float kTwoPi = 6.28318530717958647692f;

struct SamplerParams {
  int sampleStart = 0;
  int sampleEnd = 127;
  int sampleLoop = 0;
  int sampleTune = 0;
  int sampleRoot = 60;
  int sampleGain = 96;
  int attack = 4;
  int release = 18;
  int cutoff = 96;
  int resonance = 18;
  int drive = 0;
  int level = 96;
  float filterG = 0.5f;
  float filterDamping = 1.0f;
  SharedSampleBuffer sampleFrames;
  std::uint32_t sampleRate = kSampleRate;
};

struct SamplerVoice {
  bool active = false;
  bool releasing = false;
  std::uint8_t note = 0;
  float velocity = 0.0f;
  float envelope = 0.0f;
  double sampleCursor = 0.0;
  float filterLow = 0.0f;
  float filterBand = 0.0f;
  std::uint64_t serial = 0;
};

struct EngineState {
  SynthEngineKind kind = SynthEngineKind::None;
  bool configured = false;
  int synthId = 0;
  int patchNumber = -1;
  int noteOffset = 0;
  std::array<std::uint8_t, 156> dx7{};
  std::array<std::uint8_t, 18> juno{};
  std::array<bool, 128> activeNotes{};
  std::array<int, 8> drumLevels{{100, 100, 100, 100, 100, 100, 100, 100}};
  int drumMaster = 100;
  SamplerParams sampler{};
  std::array<SamplerVoice, kSamplerVoiceCount> samplerVoices{};
  std::uint64_t nextSamplerVoiceSerial = 1;
};

enum class PendingEventType {
  EnsureEngine,
  SetPreset,
  SetParameter,
  SetSamplerFrames,
  NoteOn,
  NoteOff,
  AllNotesOff,
  SetTrackGain,
  SetTrackMuted,
  SetMasterFx,
};

struct PendingEvent {
  PendingEventType type = PendingEventType::EnsureEngine;
  std::size_t engineIndex = 0;
  SynthEngineKind kind = SynthEngineKind::None;
  std::uint8_t note = 0;
  std::uint8_t velocity = 0;
  int value = 0;
  std::string text;
  SharedSampleBuffer frames;
  std::uint32_t sampleRate = kSampleRate;
  float gain = 1.0f;
};

// Track mixer. AMY renders every voice's root oscillator (with its chained or FM-operator
// oscillators) into a private buffer, decides from that buffer whether the voice has fallen
// silent, and then offers it to amy_external_render_hook before mixing it into the shared output.
// The hook scales each buffer by its track's gain there: post-filter and post-envelope, so the
// gain composes with every engine level (patch levels, Juno vca.level, Drums master.level) and
// survives preset loads, and it never feeds AMY's silence detection, so notes held through a
// mute or a closed fader are still playing when the gain comes back. At unity the hook leaves
// the buffer untouched, which keeps the output bit-identical to rendering without the mixer.
constexpr std::int32_t kTrackGainUnityQ16 = 1 << 16;
// Applied gain moves by at most 0.25 per AMY block, interpolated per frame: a full 0 <-> 1
// swing takes four blocks (~23 ms).
constexpr float kTrackGainStepPerBlock = 0.25f;
constexpr float kTrackGainStepPerFrame = kTrackGainStepPerBlock / static_cast<float>(kAmyFramesPerBlock);
constexpr float kMeterDecaySeconds = 0.3f;
constexpr float kMeterFloor = 0.0001f;
constexpr float kActivityHeldFraction = 0.5f;

// Render-thread mixer state, applied while draining events and rendering.
struct TrackMixState {
  float gain = 1.0f;
  bool muted = false;
  float amyGain = 1.0f;     // Gain ramping towards the target on the AMY voices.
  std::int32_t amyBlockStartQ16 = kTrackGainUnityQ16;  // Gain at the first and last frame of
  std::int32_t amyBlockEndQ16 = kTrackGainUnityQ16;    // the AMY block being rendered.
  float samplerGain = 1.0f; // Gain ramping towards the target on the sampler voices.
  float activity = 0.0f;
  float heldLevel = 0.0f;   // Velocity-normalized level of the latest note-on.
};

// UI-visible mixer state: requested values plus the published meters.
struct PublishedTrackMix {
  std::atomic<float> gain{1.0f};
  std::atomic<bool> muted{false};
  std::atomic<float> activity{0.0f};
};

std::once_flag g_amyInitFlag;
bool g_amyInitialized = false;
std::array<EngineState, kSynthTrackCount> g_engines{};
std::array<std::int16_t, kAmyFramesPerBlock * kAmyChannelCount> g_amyBlock{};
std::size_t g_amyBlockCursor = kAmyFramesPerBlock;
std::deque<PendingEvent> g_pendingEvents;
std::mutex g_pendingEventMutex;
std::mutex g_renderMutex;
std::array<TrackMixState, kSynthTrackCount> g_trackMix{};
// Owning track + 1 of every AMY oscillator (0: none), rebuilt whenever AMY allocates or
// releases voices.
std::array<std::uint8_t, kAmyMaxOscs> g_amyOscTrack{};
std::array<PublishedTrackMix, kSynthTrackCount> g_publishedMix;
float g_outputPeakState = 0.0f;
std::atomic<float> g_outputPeak{0.0f};

// Effects (SynthFx.h, DSP in Fx/SynthFxDsp.h). An AMY track with an active insert or send leaves
// AMY's shared mix: the render hook adds its voices, after the track gain ramp, into a private
// float bus (cheaper on the ESP32-S3 than AMY's own stereo mix) that runs through the track's
// insert chain and sends right after each AMY block. Without memory for its bus, a track that
// only sends stays in AMY's mix and the hook feeds the sends from it directly. Sampler tracks with
// effects render into their bus per chunk. The unrouted AMY output, routed tracks and the bus
// returns then meet in a float mix, go through the master chain and the output soft clipper.
// Until effects are used, and again once nothing is routed, the master chain is neutral and the
// bus tails have died out, rendering takes exactly the paths above, bit for bit.
//
// Memory is allocated on the render thread when a setting first needs it and is then kept:
// FxCore (~4.6 KB) on first use, 1 KB per track with effects, 1 KB per chorus, 16 KB for the delay
// line on the first delay send, 4.75 KB for the reverb on the first reverb send.
#if defined(ESP_PLATFORM)
constexpr float kAmyOutputShift = 0.5f;  // amy_fill_buffer drops one bit on ESP32.
#else
constexpr float kAmyOutputShift = 1.0f;
#endif
static_assert(kAmyFramesPerBlock <= fx::dsp::kMaxBlockFrames, "effects process whole AMY blocks");

struct TrackFxParams {
  fx::dsp::TrackInsertSettings inserts{};
  int delaySend = 0;
  int reverbSend = 0;
};

struct FxCore {
  std::array<float, kAmyFramesPerBlock> mix{};  // Float mix of the current AMY block (or chunk).
  std::array<float, kAmyFramesPerBlock> delaySend{};
  std::array<float, kAmyFramesPerBlock> reverbSend{};
  std::array<fx::dsp::TrackInserts, kSynthTrackCount> inserts{};
  std::array<float, kSynthTrackCount> delayGain{};  // Send gains as applied, ramping per block.
  std::array<float, kSynthTrackCount> reverbGain{};
  fx::dsp::SendBuses buses;
  fx::dsp::MasterChain master;
  fx::dsp::TanhTable tanh;
};

// What the render hook does with one track's AMY voices during the block being rendered.
struct FxHookTrack {
  float* bus = nullptr;         // Inserts: the voices join this bus (mix units), not AMY's mix.
  float* delaySend = nullptr;   // Sends only: the voices stay in AMY's mix and also feed the sends.
  float* reverbSend = nullptr;
  float delayGain = 0.0f, delayStep = 0.0f;    // Per-frame send gains, AMY-to-mix scale included.
  float reverbGain = 0.0f, reverbStep = 0.0f;
};

constexpr std::array<int, fx::kMasterParamCount> DefaultMasterFx()
{
  std::array<int, fx::kMasterParamCount> values{};
  for (std::size_t index = 0; index < values.size(); ++index) values[index] = fx::kMasterParams[index].defaultValue;
  return values;
}

// Render-thread effects state.
struct FxState {
  std::array<TrackFxParams, kSynthTrackCount> tracks{};
  std::array<int, fx::kMasterParamCount> master = DefaultMasterFx();
  std::unique_ptr<FxCore> core;
  std::array<std::unique_ptr<float[]>, kSynthTrackCount> bus;  // Tracks with inserts (or sampler tracks with effects).
  std::array<bool, kSynthTrackCount> routed{};        // Any effect: the float path is needed.
  std::array<bool, kSynthTrackCount> insertRouted{};  // Through the track's bus and inserts.
  bool floatPath = false;
  // Output stage of the effects path: 0 = the AMY-only path's linear 0.75 gain, 1 = the soft clipper
  // of the sampler mix. It crossfades over one AMY block when the path starts or ends next to the
  // AMY-only path, so switching paths does not click.
  float outputBlend = 1.0f;
  bool lastPathLinear = false;  // The plain path last used was the AMY-only (linear) one.
  bool blockPrepared = false;   // core->mix holds the AMY block at g_amyBlockCursor.
  bool dirty = false;          // Settings changed: allocations may be due.
  std::array<float, fx::kBusCount> returnMeter{};
};

FxState g_fx;
// Render hook plan for the AMY block being rendered (all null outside the effects path).
std::array<FxHookTrack, kSynthTrackCount> g_fxHook{};
float g_fxVoiceScale = 0.0f;  // AMY voice sample (S8.23) to mix units, for the block being rendered.

template <std::size_t... Index>
std::array<std::atomic<int>, sizeof...(Index)> MakePublishedMasterFx(std::index_sequence<Index...>)
{
  return {{std::atomic<int>{fx::kMasterParams[Index].defaultValue}...}};
}

// UI-visible effects state.
std::array<std::atomic<int>, fx::kMasterParamCount> g_publishedMasterFx = MakePublishedMasterFx(std::make_index_sequence<fx::kMasterParamCount>{});
std::atomic<float> g_fxTempo{120.0f};
std::array<std::atomic<float>, fx::kBusCount> g_fxReturnLevel{};
std::array<std::atomic<bool>, fx::kBusCount> g_fxBusFailed{};

struct AmyShutdown {
  ~AmyShutdown()
  {
    if (g_amyInitialized)
    {
      amy_stop();
    }
  }
};

AmyShutdown g_amyShutdown;

float Clamp01(float value)
{
  return std::clamp(value, 0.0f, 1.0f);
}

std::int16_t ClampToInt16(std::int32_t value)
{
  return static_cast<std::int16_t>(std::clamp<std::int32_t>(value, INT16_MIN, INT16_MAX));
}

std::int16_t FloatToInt16(float sample)
{
  return static_cast<std::int16_t>(std::clamp(sample, -1.0f, 1.0f) * 32767.0f);
}

float Normalize(int value, int minimum, int maximum)
{
  if (maximum <= minimum)
  {
    return 0.0f;
  }
  return Clamp01(static_cast<float>(value - minimum) / static_cast<float>(maximum - minimum));
}

std::size_t SamplerStartFrame(const SamplerParams& params)
{
  if (!params.sampleFrames || params.sampleFrames->frameCount == 0)
  {
    return 0;
  }

  const std::size_t lastFrame = params.sampleFrames->frameCount - 1;
  return (std::min)(lastFrame,
                    static_cast<std::size_t>((static_cast<std::uint64_t>(lastFrame) * static_cast<std::uint64_t>(std::clamp(params.sampleStart, 0, 127))) /
                                             127u));
}

std::size_t SamplerEndFrame(const SamplerParams& params, std::size_t startFrame)
{
  if (!params.sampleFrames || params.sampleFrames->frameCount == 0)
  {
    return 0;
  }

  const std::size_t frameCount = params.sampleFrames->frameCount;
  const std::size_t endFrame = static_cast<std::size_t>((static_cast<std::uint64_t>(frameCount) * static_cast<std::uint64_t>(std::clamp(params.sampleEnd, 0, 127))) /
                                                        127u);
  return std::clamp(endFrame, startFrame + 1, frameCount);
}

float ReadSamplerFrame(const SamplerParams& params, double frameCursor, std::size_t endFrame)
{
  if (!params.sampleFrames || params.sampleFrames->frameCount == 0)
  {
    return 0.0f;
  }

  const std::size_t clampedEnd = std::clamp<std::size_t>(endFrame, 1, params.sampleFrames->frameCount);
  const std::size_t index0 = (std::min)(clampedEnd - 1, static_cast<std::size_t>((std::max)(0.0, std::floor(frameCursor))));
  const std::size_t index1 = (std::min)(clampedEnd - 1, index0 + 1);
  const float fraction = static_cast<float>(std::clamp(frameCursor - static_cast<double>(index0), 0.0, 1.0));
  const float sample0 = static_cast<float>(params.sampleFrames->frames[index0]) / 32768.0f;
  const float sample1 = static_cast<float>(params.sampleFrames->frames[index1]) / 32768.0f;
  return sample0 + (sample1 - sample0) * fraction;
}

float SamplerLowpass(float input, const SamplerParams& params, SamplerVoice& voice)
{
  // Two-integrator state-variable lowpass, with bounded Q for predictable headroom.
  const float g = params.filterG;
  const float damping = params.filterDamping;
  const float band = (voice.filterBand + g * (input - voice.filterLow)) / (1.0f + g * (g + damping));
  const float low = voice.filterLow + g * band;
  voice.filterBand = 2.0f * band - voice.filterBand;
  voice.filterLow = 2.0f * low - voice.filterLow;
  return low;
}

void UpdateSamplerFilter(SamplerParams& params)
{
  const float cutoff = 80.0f + std::pow(Normalize(params.cutoff, 0, 127), 2.2f) * 12000.0f;
  params.filterG = std::tan(0.5f * kTwoPi * cutoff / static_cast<float>(kSampleRate));
  params.filterDamping = 1.0f / (0.7071f + Normalize(params.resonance, 0, 127) * 7.0f);
}

bool UsesAmy(SynthEngineKind kind)
{
  return kind == SynthEngineKind::Dx7 || kind == SynthEngineKind::Juno || kind == SynthEngineKind::Drums;
}

int VoiceCountFor(SynthEngineKind kind)
{
  switch (kind)
  {
    case SynthEngineKind::Dx7:
      return 4;
    case SynthEngineKind::Juno:
    case SynthEngineKind::Drums:
      return 6;
    default:
      return 0;
  }
}

int DefaultPatchFor(SynthEngineKind kind)
{
  switch (kind)
  {
    case SynthEngineKind::Dx7:
      return 138;
    case SynthEngineKind::Juno:
      return 32;
    default:
      return -1;
  }
}

int MapDx7Preset(std::string_view presetId)
{
  if (presetId == "dx7.piano1") return 135;
  if (presetId == "dx7.piano3") return 137;
  if (presetId == "dx7.epiano1") return 138;
  if (presetId == "dx7.brass1") return 128;
  if (presetId == "dx7.organ1") return 150;
  if (presetId == "dx7.bell") return 219;
  return 138;
}

int MapJunoPreset(std::string_view presetId)
{
  if (const JunoFactoryPresetDefinition* preset = FindJunoFactoryPreset(presetId)) return preset->patchNumber;
  if (presetId == "juno.saw") return 32;
  if (presetId == "juno.pwm") return 47;
  if (presetId == "juno.hoover") return 34;
  if (presetId == "juno.strings") return 64;
  if (presetId == "juno.brass") return 80;
  if (presetId == "juno.bass") return 31;
  if (presetId == "juno.pad") return 74;
  return 32;
}

// Called by AMY (on the rendering core) for each sounding voice block, before it is mixed.
SYNTH_FX_HOT std::uint8_t ApplyTrackGainToAmyVoice(std::uint16_t osc, std::int32_t* buffer, std::uint16_t length)
{
  if (osc >= g_amyOscTrack.size() || g_amyOscTrack[osc] == 0)
  {
    return 0;
  }

  const std::size_t track = g_amyOscTrack[osc] - 1u;
  const TrackMixState& mix = g_trackMix[track];
  const std::int32_t startQ16 = mix.amyBlockStartQ16;
  const std::int32_t endQ16 = mix.amyBlockEndQ16;
  if (startQ16 == 0 && endQ16 == 0)
  {
    return 1;  // "Handled": AMY skips mixing this voice block.
  }
  const FxHookTrack& fx = g_fxHook[track];
  if (float* bus = fx.bus)
  {
    // Through the track's inserts: the voice joins the track bus instead of AMY's mix, with the
    // gain ramp applied in float.
    const float inverseLength = length == kAmyFramesPerBlock ? 1.0f / static_cast<float>(kAmyFramesPerBlock) : 1.0f / static_cast<float>(length);
    const float scale = g_fxVoiceScale * (1.0f / static_cast<float>(kTrackGainUnityQ16));
    float gain = static_cast<float>(startQ16) * scale;
    if (startQ16 == endQ16)
    {
      for (std::uint16_t index = 0; index < length; ++index) bus[index] += static_cast<float>(buffer[index]) * gain;
      return 1;
    }
    const float step = static_cast<float>(endQ16 - startQ16) * scale * inverseLength;
    for (std::uint16_t index = 0; index < length; ++index)
    {
      bus[index] += static_cast<float>(buffer[index]) * gain;
      gain += step;
    }
    return 1;
  }

  if (startQ16 != kTrackGainUnityQ16 || endQ16 != kTrackGainUnityQ16)
  {
    if (startQ16 == endQ16)
    {
      // A settled fader needs only the Q16 multiply. Keep the signed multiply/shift identical
      // to the ramp path, without doing a division by the block length for every sample.
      for (std::uint16_t index = 0; index < length; ++index)
      {
        buffer[index] = static_cast<std::int32_t>((static_cast<std::int64_t>(buffer[index]) * startQ16) >> 16);
      }
    }
    else
    {
      const std::int32_t deltaQ16 = endQ16 - startQ16;
      for (std::uint16_t index = 0; index < length; ++index)
      {
        const std::int64_t gainQ16 = startQ16 + (deltaQ16 * static_cast<std::int32_t>(index)) / static_cast<std::int32_t>(length);
        buffer[index] = static_cast<std::int32_t>((static_cast<std::int64_t>(buffer[index]) * gainQ16) >> 16);
      }
    }
  }
  // Sends only: the (post-fader) voice also feeds the delay and reverb sends.
  if (fx.delaySend != nullptr && fx.reverbSend != nullptr)
  {
    float delayGain = fx.delayGain, reverbGain = fx.reverbGain;
    if (fx.delayStep == 0.0f && fx.reverbStep == 0.0f)
    {
      for (std::uint16_t index = 0; index < length; ++index)
      {
        const float sample = static_cast<float>(buffer[index]);
        fx.delaySend[index] += sample * delayGain;
        fx.reverbSend[index] += sample * reverbGain;
      }
      return 0;
    }
    for (std::uint16_t index = 0; index < length; ++index)
    {
      const float sample = static_cast<float>(buffer[index]);
      fx.delaySend[index] += sample * delayGain;
      fx.reverbSend[index] += sample * reverbGain;
      delayGain += fx.delayStep;
      reverbGain += fx.reverbStep;
    }
  }
  else if (float* send = fx.delaySend != nullptr ? fx.delaySend : fx.reverbSend)
  {
    const bool delay = fx.delaySend != nullptr;
    float gain = delay ? fx.delayGain : fx.reverbGain;
    const float step = delay ? fx.delayStep : fx.reverbStep;
    if (step == 0.0f)
    {
      for (std::uint16_t index = 0; index < length; ++index) send[index] += static_cast<float>(buffer[index]) * gain;
      return 0;
    }
    for (std::uint16_t index = 0; index < length; ++index)
    {
      send[index] += static_cast<float>(buffer[index]) * gain;
      gain += step;
    }
  }
  return 0;
}

void EnsureAmyInitialized()
{
  std::call_once(g_amyInitFlag, []() {
    amy_config_t config = amy_default_config();
    config.audio = AMY_AUDIO_IS_NONE;
    config.midi = AMY_MIDI_IS_NONE;
    config.features.default_synths = 0;
    config.features.startup_bleep = 0;
    config.features.audio_in = 0;
    config.features.custom = 0;
    config.features.partials = 0;
    config.features.reverb = 0;
    config.features.echo = 0;
    config.features.chorus = 0;
  #if defined(ESP_PLATFORM)
    config.platform.multicore = kAmyUseExperimentalMulticore ? 1 : 0;
    config.platform.multithread = 0;
  #else
    config.platform.multicore = 0;
    config.platform.multithread = 0;
  #endif
    // CARDENZA sequences in SynthSequencer; AMY's own tag table would only cost internal RAM.
    config.max_sequencer_tags = 1;
    config.max_synths = kAmyMaxSynths;
    config.max_voices = kAmyMaxVoices;
    config.max_oscs = kAmyMaxOscs;
    // Cardenza owns its track effects and mixes AMY into one shared stereo bus.
    config.max_buses = 1;
    config.amy_external_render_hook = ApplyTrackGainToAmyVoice;
    amy_start(config);
    g_amyInitialized = true;
  });
}

void SendAmyNoteEventNoLock(const EngineState& engine, std::uint8_t note, float velocity)
{
  if (!g_amyInitialized)
  {
    return;
  }

  amy_event event = amy_default_event();
  event.synth = static_cast<uint16_t>(engine.synthId);
  event.midi_note = static_cast<uint16_t>(std::clamp(static_cast<int>(note) + engine.noteOffset, 0, 127));
  event.velocity = velocity;
  if (engine.kind == SynthEngineKind::Drums && note != 0)
  {
    const auto pad = std::find(kDrumNotes.begin(), kDrumNotes.end(), note);
    if (pad == kDrumNotes.end()) return;
    // AMY no longer has its fixed GM-to-tiny-PCM drum flag. Preserve this
    // kit's eight sounds explicitly without depending on MIDI-channel mappings.
    constexpr std::array<std::uint16_t, 8> presets{{1, 2, 6, 7, 9, 8, 10, 0}};
    event.preset = presets[pad - kDrumNotes.begin()];
    event.midi_note = amy_embed::pcm_map[event.preset].midinote;
    event.wave = PCM;
    event.feedback = 0;
  }
  amy_add_event(&event);
}

void ReleaseAllAmyNotesNoLock(EngineState& engine)
{
  if (engine.kind == SynthEngineKind::Drums)
  {
    // Drum key releases preserve one-shot tails. Explicit stop must still reach
    // every allocated voice, including sounds whose performance key was released.
    if (std::any_of(engine.activeNotes.begin(), engine.activeNotes.end(), [](bool active) { return active; }))
    {
      auto event = amy_default_event();
      event.synth = static_cast<std::uint8_t>(engine.synthId);
      event.velocity = 0;
      amy_add_event(&event);
    }
    engine.activeNotes.fill(false);
    return;
  }
  for (std::size_t note = 0; note < engine.activeNotes.size(); ++note)
  {
    if (!engine.activeNotes[note])
    {
      continue;
    }

    SendAmyNoteEventNoLock(engine, static_cast<std::uint8_t>(note), 0.0f);
    engine.activeNotes[note] = false;
  }
}

void ReleaseSamplerNoteNoLock(EngineState& engine, std::uint8_t note)
{
  for (SamplerVoice& voice : engine.samplerVoices)
  {
    if (voice.active && voice.note == note)
    {
      voice.releasing = true;
    }
  }
}

amy_event SynthEvent(const EngineState& engine, int oscillator)
{
  amy_event event = amy_default_event();
  event.synth = static_cast<std::uint8_t>(engine.synthId);
  event.osc = static_cast<std::uint16_t>(oscillator);
  return event;
}

void ApplyJunoParameterNoLock(EngineState& engine, std::string_view id, int value)
{
  const int offset = factory::JunoOffset(id);
  const auto setFlag = [&](std::size_t byte, unsigned flag) {
    engine.juno[byte] = static_cast<std::uint8_t>((engine.juno[byte] & ~flag) | (value ? flag : 0u));
  };
  if (offset >= 0) engine.juno[offset] = static_cast<std::uint8_t>(std::clamp(value, 0, 127));
  else if (id == "dco.saw") setFlag(16, 16u);
  else if (id == "dco.pulse") setFlag(16, 8u);
  else if (id == "dco.range") engine.juno[16] = (engine.juno[16] & ~7u) | (1u << std::clamp(value, 0, 2));
  else if (id == "dco.pwm_mode") setFlag(17, 1u);
  else if (id == "vcf.polarity") setFlag(17, 2u);
  else if (id == "vca.mode") setFlag(17, 4u);
  else return;
  const auto& p = engine.juno;
  const auto normalized = [&](int i) { return p[i] / 127.0f; };
  const std::string_view group = id.substr(0, 4);
  // vca.mode swaps the envelope that drives the VCF (gate mode: ENV -> VCF, gate -> VCA).
  const bool vcaMode = id == "vca.mode";
  if (group == "lfo.")
  {
    auto event = SynthEvent(engine, 1);
    event.freq_coefs[amy_embed::COEF_CONST] = 0.6f * std::exp2(0.04f * p[0]) - 0.1f;
    event.eg0_times[0] = static_cast<std::uint32_t>(18.0f * std::exp2(0.066f * p[1]) - 13.0f);
    event.eg0_values[0] = 1;
    event.eg0_times[1] = 10000;
    event.eg0_values[1] = 0;
    amy_add_event(&event);
    return;
  }
  if (group == "dco.")
  {
    const float baseFrequency = static_cast<float>(ZERO_LOGFREQ_IN_HZ) * ((p[16] & 1) ? 0.5f : ((p[16] & 4) ? 2.0f : 1.0f));
    for (int osc = 2; osc <= 5; ++osc)
    {
      auto event = SynthEvent(engine, osc);
      event.amp_coefs[amy_embed::COEF_CONST] = osc == 2 ? float((p[16] & 8) != 0) :
          osc == 3 ? float((p[16] & 16) != 0) : normalized(osc == 4 ? 15 : 4);
      if (osc < 5)
      {
        event.freq_coefs[amy_embed::COEF_CONST] = baseFrequency * (osc == 4 ? 0.5f : 1.0f);
        // DCO LFO depth (amy/juno.py _freq_coef_string).
        event.freq_coefs[amy_embed::COEF_MOD] = 0.03f * normalized(2);
      }
      if (osc == 2)
      {
        const bool manual = (p[17] & 1) != 0;
        event.duty_coefs[amy_embed::COEF_CONST] = 0.5f + (manual ? 0.5f * normalized(3) : 0.0f);
        event.duty_coefs[amy_embed::COEF_MOD] = manual ? 0.0f : 0.5f * normalized(3);
      }
      amy_add_event(&event);
    }
    return;
  }
  if (group == "vcf." || vcaMode)
  {
    auto event = SynthEvent(engine, 0);
    event.resonance = 0.7f * std::exp2(4.0f * normalized(6));
    event.filter_freq_coefs[amy_embed::COEF_CONST] = 13.0f * std::exp2(0.0938f * p[5] + 0.75f * normalized(9));
    event.filter_freq_coefs[amy_embed::COEF_NOTE] = normalized(9);
    const float amount = 11.0f * ((p[17] & 2) ? -1.0f : 1.0f) * normalized(7);
    event.filter_freq_coefs[amy_embed::COEF_EG0] = (p[17] & 4) ? 0.0f : amount;
    event.filter_freq_coefs[amy_embed::COEF_EG1] = (p[17] & 4) ? amount : 0.0f;
    event.filter_freq_coefs[amy_embed::COEF_MOD] = 1.25f * normalized(8);
    amy_add_event(&event);
  }
  if (id == "vca.level" || group == "env." || vcaMode)
  {
    auto event = SynthEvent(engine, 0);
    event.amp_coefs[amy_embed::COEF_CONST] = normalized(10) * factory::kJunoGain[std::clamp(engine.patchNumber, 0, 127)];
    // Gate mode (amy/juno.py update_env): the VCA follows a gate on envelope 0 and the ADSR moves
    // to envelope 1, where only the VCF reads it.
    const bool gate = (p[17] & 4) != 0;
    auto* times = gate ? event.eg1_times : event.eg0_times;
    auto* levels = gate ? event.eg1_values : event.eg0_values;
    times[0] = 6 + 8 * p[11]; levels[0] = 1;
    times[1] = static_cast<std::uint32_t>(80 * std::exp2(0.085f * p[12]) - 80); levels[1] = normalized(13);
    times[2] = static_cast<std::uint32_t>(70 * std::exp2(0.066f * p[14]) - 70); levels[2] = 0;
    if (gate)
    {
      event.eg0_times[0] = 0; event.eg0_values[0] = 1;
      event.eg0_times[1] = 0; event.eg0_values[1] = 1;
      event.eg0_times[2] = 0; event.eg0_values[2] = 0;
    }
    amy_add_event(&event);
  }
}

float Dx7Level(int value)
{
  return std::exp2((value - 99.0f) / 8.0f);
}

void Dx7OperatorEnvelope(amy_event& event, const std::uint8_t* op)
{
  int current = op[7];
  event.eg0_times[0] = 0;
  event.eg0_values[0] = Dx7Level(current);
  for (int i = 0; i < 4; ++i)
  {
    const int target = op[4 + i];
    float seconds;
    if (target > current)
    {
      const float timeConstant = 0.008f * std::exp2((65.0f - op[i]) / 6.0f);
      seconds = timeConstant * std::log((109.0f - (std::max)(34, current)) / (109.0f - (std::max)(34, target)));
    }
    else
    {
      const int difference = (i == 3 && target == current) ? 60 : current - target;
      seconds = difference / (0.5f + 0.5f * std::exp2(op[i] / 6.0f));
    }
    event.eg0_times[i + 1] = static_cast<std::uint32_t>((std::max)(0.0f, seconds * 1000.0f));
    event.eg0_values[i + 1] = Dx7Level(target);
    current = target;
  }
}

void ApplyDx7ParameterNoLock(EngineState& engine, std::string_view id, int value)
{
  const int offset = factory::Dx7Offset(id);
  if (offset < 0) return;
  if (id == "global.transpose")
  {
    if (engine.noteOffset != value) ReleaseAllAmyNotesNoLock(engine);
    engine.noteOffset = std::clamp(value, -24, 24);
    engine.dx7[offset] = static_cast<std::uint8_t>(engine.noteOffset + 24);
    return;
  }
  int raw = value;
  if (id == "global.algorithm") --raw;
  if (id.size() > 4 && id.substr(4) == "detune") raw += 7;
  int maximum = 99;
  if (id == "global.lfo_wave") maximum = 5;
  else if (id == "global.lfo_pms") maximum = 7;
  else if (offset < 126 && offset % 21 == 17) maximum = 1;  // Operator mode: 0 ratio, 1 fixed.
  engine.dx7[offset] = static_cast<std::uint8_t>(std::clamp(raw, 0, maximum));
  const auto& p = engine.dx7;
  if (id == "global.algorithm" || id == "global.feedback" || id == "global.lfo_pitch" || id == "global.lfo_pms")
  {
    auto event = SynthEvent(engine, 0);
    event.algorithm = static_cast<std::uint8_t>(p[134] + 1);
    event.feedback = 0.00125f * std::exp2(float(p[135]));
    event.freq_coefs[amy_embed::COEF_MOD] = p[143] == 0 ? 0.0f :
        0.6f * std::pow(1.7f, p[143] - 1.0f) * p[139] / 1188.0f;
    amy_add_event(&event);
  }
  else if (id == "global.lfo_speed")
  {
    const int speed = p[137];
    auto event = SynthEvent(engine, 1);
    event.freq_coefs[amy_embed::COEF_CONST] = speed == 0 ? 0.064f : speed <= 64 ? speed / 6.0f :
        speed <= 85 ? speed - 64.0f * 5.0f / 6.0f : 31.67f + (speed - 85.0f) * 1.33f;
    amy_add_event(&event);
  }
  else if (id == "global.lfo_wave")
  {
    // DX7 order TRI, SAW DOWN, SAW UP, SQUARE, SINE, S/H, as amy/fm.py lfo_wave.
    constexpr std::array<std::uint16_t, 6> kWaves{{TRIANGLE, SAW_DOWN, SAW_UP, PULSE, SINE, NOISE}};
    auto event = SynthEvent(engine, 1);
    event.wave = kWaves[p[142]];
    amy_add_event(&event);
  }
  else
  {
    const bool all = id == "global.lfo_amp";
    for (int index = 0; index < 6; ++index)
    {
      if (!all && offset / 21 != index) continue;
      const auto* op = p.data() + index * 21;
      auto event = SynthEvent(engine, index + 2);
      if (all || offset % 21 == 16 || offset % 21 == 14)
      {
        event.amp_coefs[amy_embed::COEF_CONST] = 2.0f * Dx7Level(op[16]);
        // AMY's DX7 converter supports modulation sensitivity as an on/off switch.
        event.amp_coefs[amy_embed::COEF_MOD] = op[14] ? Dx7Level(p[140]) : 0.0f;
      }
      else if (offset % 21 >= 17)
      {
        const float fine = op[19] + (op[20] - 7.0f) / 8.0f;
        if (op[17] == 0) event.ratio = (op[18] == 0 ? 0.5f : float(op[18])) * (1.0f + fine / 100.0f);
        else
        {
          event.freq_coefs[amy_embed::COEF_CONST] = std::pow(10.0f, (op[18] & 3) + fine / 100.0f);
          // A fixed operator must drop its ratio: AMY stores log2(ratio) and treats NaN as unset.
          event.ratio = -1.0f;
        }
      }
      else Dx7OperatorEnvelope(event, op);
      amy_add_event(&event);
    }
  }
}

TrackMixState& TrackMixFor(const EngineState& engine)
{
  return g_trackMix[static_cast<std::size_t>(&engine - g_engines.data())];
}

float EffectiveTrackGain(const TrackMixState& mix)
{
  return mix.muted ? 0.0f : mix.gain;
}

float ApproachGain(float current, float target, float step)
{
  return current < target ? (std::min)(target, current + step) : (std::max)(target, current - step);
}

std::int32_t GainToQ16(float gain)
{
  return static_cast<std::int32_t>(std::lround(Clamp01(gain) * static_cast<float>(kTrackGainUnityQ16)));
}

// Runs before every AMY block: ramps each AMY track one step towards its effective gain.
void StepAmyTrackGainsNoLock()
{
  for (std::size_t index = 0; index < g_engines.size(); ++index)
  {
    const EngineState& engine = g_engines[index];
    TrackMixState& mix = g_trackMix[index];
    const float target = EffectiveTrackGain(mix);
    mix.amyGain = engine.configured && UsesAmy(engine.kind) ? ApproachGain(mix.amyGain, target, kTrackGainStepPerBlock) : target;
    mix.amyBlockStartQ16 = mix.amyBlockEndQ16;
    mix.amyBlockEndQ16 = GainToQ16(mix.amyGain);
  }
}

void RebuildAmyOscTrackMapNoLock()
{
  g_amyOscTrack.fill(0);
  if (!g_amyInitialized)
  {
    return;
  }
  for (std::size_t track = 0; track < g_engines.size(); ++track)
  {
    const EngineState& engine = g_engines[track];
    if (!engine.configured || !UsesAmy(engine.kind))
    {
      continue;
    }

    std::array<std::uint16_t, MAX_VOICES_PER_INSTRUMENT> voices{};
    const int voiceCount = amy_embed::instrument_get_num_voices(engine.synthId, voices.data());
    if (voiceCount <= 0)
    {
      continue;
    }
    const int oscsPerVoice = amy_embed::instrument_get_oscs_per_voice(engine.synthId);
    for (int voice = 0; voice < voiceCount; ++voice)
    {
      const int base = amy_embed::voice_to_base_osc[voices[voice]];
      for (int osc = base; osc < base + oscsPerVoice && osc < static_cast<int>(g_amyOscTrack.size()); ++osc)
      {
        g_amyOscTrack[static_cast<std::size_t>(osc)] = static_cast<std::uint8_t>(track + 1);
      }
    }
  }
}

void BumpTrackActivityNoLock(std::size_t engineIndex, float level)
{
  TrackMixState& mix = g_trackMix[engineIndex];
  mix.heldLevel = Clamp01(level);
  mix.activity = (std::max)(mix.activity, mix.heldLevel * EffectiveTrackGain(mix));
}

void ConfigureAmySynthNoLock(EngineState& engine)
{
  if (!UsesAmy(engine.kind))
  {
    return;
  }

  EnsureAmyInitialized();
  ReleaseAllAmyNotesNoLock(engine);

  amy_event event = amy_default_event();
  event.synth = static_cast<uint16_t>(engine.synthId);
  event.num_voices = static_cast<uint16_t>(VoiceCountFor(engine.kind));
  if (engine.kind == SynthEngineKind::Drums)
  {
    event.oscs_per_voice = 1;
    event.wave = PCM;
    // Do not use AMY's IGNORE_NOTE_OFFS flag: it also suppresses explicit stop.
    event.synth_flags = 0;
    event.amp_coefs[amy_embed::COEF_CONST] = 5.0f * Normalize(engine.drumMaster, 0, 127);
  }
  else
  {
    event.patch_number = static_cast<uint16_t>(engine.patchNumber < 0 ? DefaultPatchFor(engine.kind) : engine.patchNumber);
  }
  amy_add_event(&event);

  // AMY's Juno ROM models its HPF with global EQ. In this four-track rack that
  // would recolor every track and make project sound depend on preset load order.
  // Keep the shared master flat; per-track HPF remains unsupported in this version.
  event = amy_default_event();
  event.eq_l = event.eq_m = event.eq_h = 0.0f;
  amy_add_event(&event);

  // Fresh voices are silent: start them at the track gain without a ramp.
  TrackMixState& mix = TrackMixFor(engine);
  mix.amyGain = EffectiveTrackGain(mix);
  mix.amyBlockStartQ16 = mix.amyBlockEndQ16 = GainToQ16(mix.amyGain);
  RebuildAmyOscTrackMapNoLock();
}

void LoadDx7FactoryPresetNoLock(EngineState& engine, const Dx7FactoryPresetDefinition& preset)
{
  engine.patchNumber = preset.patchNumber;
  ConfigureAmySynthNoLock(engine);
}

SamplerVoice& AllocateSamplerVoice(EngineState& engine)
{
  auto iterator = std::find_if(engine.samplerVoices.begin(), engine.samplerVoices.end(), [](const SamplerVoice& voice) {
    return !voice.active;
  });

  if (iterator == engine.samplerVoices.end())
  {
    iterator = std::min_element(engine.samplerVoices.begin(), engine.samplerVoices.end(), [](const SamplerVoice& left, const SamplerVoice& right) {
      return left.serial < right.serial;
    });
  }

  *iterator = SamplerVoice{};
  iterator->active = true;
  iterator->serial = engine.nextSamplerVoiceSerial++;
  return *iterator;
}

float RenderSamplerVoice(const SamplerParams& params, SamplerVoice& voice)
{
  if (!voice.active || !params.sampleFrames || !params.sampleFrames->frames || params.sampleFrames->frameCount == 0)
  {
    voice.active = false;
    return 0.0f;
  }

  const float attackStep = 1.0f / (std::max)(16.0f, (0.002f + Normalize(params.attack, 0, 127) * 0.25f) * static_cast<float>(kSampleRate));
  const float releaseStep = 1.0f / (std::max)(16.0f, (0.01f + Normalize(params.release, 0, 127) * 1.2f) * static_cast<float>(kSampleRate));

  if (voice.releasing)
  {
    voice.envelope -= releaseStep;
    if (voice.envelope <= 0.0f)
    {
      voice = SamplerVoice{};
      return 0.0f;
    }
  }
  else
  {
    voice.envelope = (std::min)(1.0f, voice.envelope + attackStep);
  }

  {
    const std::size_t startFrame = SamplerStartFrame(params);
    const std::size_t endFrame = SamplerEndFrame(params, startFrame);
    if (voice.sampleCursor < static_cast<double>(startFrame))
    {
      voice.sampleCursor = static_cast<double>(startFrame);
    }
    if (voice.sampleCursor >= static_cast<double>(endFrame) && params.sampleLoop == 0)
    {
      voice = SamplerVoice{};
      return 0.0f;
    }

    const double sourceRate = static_cast<double>(params.sampleRate == 0 ? kSampleRate : params.sampleRate);
    const double pitchRatio = std::pow(2.0,
                                       static_cast<double>(static_cast<int>(voice.note) - params.sampleRoot + params.sampleTune) /
                                           12.0);
    const double step = (std::max)(0.0001, (sourceRate / static_cast<double>(kSampleRate)) * pitchRatio);
    float sample = ReadSamplerFrame(params, voice.sampleCursor, endFrame);
    sample = SamplerLowpass(sample, params, voice);
    sample = std::tanh(sample * (1.0f + Normalize(params.drive, 0, 127) * 4.0f));
    sample *= voice.envelope * (0.2f + 0.8f * voice.velocity) * Normalize(params.sampleGain, 0, 127) * Normalize(params.level, 0, 127);

    voice.sampleCursor += step;
    if (params.sampleLoop != 0)
    {
      const double loopStart = static_cast<double>(startFrame);
      const double loopSpan = static_cast<double>((std::max)(std::size_t{1}, endFrame - startFrame));
      while (voice.sampleCursor >= static_cast<double>(endFrame))
      {
        voice.sampleCursor = loopStart + std::fmod(voice.sampleCursor - loopStart, loopSpan);
      }
    }
    return sample;
  }
}

void ResetEngineNoLock(EngineState& engine, std::size_t engineIndex, SynthEngineKind kind)
{
  if (engine.configured && UsesAmy(engine.kind))
  {
    ReleaseAllAmyNotesNoLock(engine);
    amy_event release = amy_default_event();
    release.synth = static_cast<std::uint8_t>(engine.synthId);
    release.num_voices = 0;
    amy_add_event(&release);
  }

  engine = EngineState{};
  engine.kind = kind;
  engine.configured = kind != SynthEngineKind::None;
  engine.synthId = static_cast<int>(engineIndex) + 1;
  engine.patchNumber = DefaultPatchFor(kind);
  UpdateSamplerFilter(engine.sampler);
  RebuildAmyOscTrackMapNoLock();
}

bool MasterFxNeutral(const std::array<int, fx::kMasterParamCount>& master)
{
  return master[static_cast<std::size_t>(fx::MasterParam::EqLow)] == fx::kFilterCenter &&
         master[static_cast<std::size_t>(fx::MasterParam::EqHigh)] == fx::kFilterCenter &&
         master[static_cast<std::size_t>(fx::MasterParam::Compressor)] == 0 &&
         master[static_cast<std::size_t>(fx::MasterParam::Tape)] == 0;
}

int MasterFxValue(fx::MasterParam param)
{
  return g_fx.master[static_cast<std::size_t>(param)];
}

// The track's settings ask for effects (sends count even before their bus exists).
bool TrackFxRequested(std::size_t track)
{
  const TrackFxParams& params = g_fx.tracks[track];
  return !fx::dsp::IsNeutral(params.inserts) || params.delaySend > 0 || params.reverbSend > 0;
}

// Back to the defaults; the engine re-sends every non-default value right after.
void ResetTrackFxNoLock(std::size_t track, int chorus)
{
  g_fx.tracks[track] = TrackFxParams{};
  g_fx.tracks[track].inserts.chorus = std::clamp(chorus, 0, 3);
  g_fx.dirty = true;
}

void SetTrackFxParameterNoLock(std::size_t track, std::string_view id, int value)
{
  TrackFxParams& params = g_fx.tracks[track];
  const int clamped = std::clamp(value, 0, 127);
  if (id == "fx.drive") params.inserts.drive = clamped;
  else if (id == "fx.crush") params.inserts.crush = clamped;
  else if (id == "fx.filter") params.inserts.filter = clamped;
  else if (id == "fx.resonance") params.inserts.resonance = clamped;
  else if (id == "fx.chorus") params.inserts.chorus = std::clamp(value, 0, 3);
  else if (id == "fx.delay") params.delaySend = clamped;
  else if (id == "fx.reverb") params.reverbSend = clamped;
  else return;
  g_fx.dirty = true;
}

// Allocates what the current settings need (render thread, only when settings changed). A bus
// whose memory is missing stays silent and reports itself unavailable; the next settings change
// tries again.
void UpdateFxNoLock()
{
  if (!g_fx.dirty)
  {
    return;
  }
  g_fx.dirty = false;

  bool anyTrack = false;
  bool delaySends = false;
  bool reverbSends = false;
  for (std::size_t track = 0; track < g_fx.tracks.size(); ++track)
  {
    anyTrack = anyTrack || TrackFxRequested(track);
    delaySends = delaySends || g_fx.tracks[track].delaySend > 0;
    reverbSends = reverbSends || g_fx.tracks[track].reverbSend > 0;
  }
  const bool delayWanted = delaySends && MasterFxValue(fx::MasterParam::DelayLevel) > 0;
  const bool reverbWanted = reverbSends && MasterFxValue(fx::MasterParam::ReverbLevel) > 0;
  if (!g_fx.core)
  {
    if (!anyTrack && MasterFxNeutral(g_fx.master))
    {
      return;
    }
    g_fx.core.reset(new (std::nothrow) FxCore());
    if (!g_fx.core)
    {
      // Without the core no effect can run: the tracks stay on AMY's mix, the buses silent.
      g_fxBusFailed[fx::kBusDelay].store(delayWanted, std::memory_order_relaxed);
      g_fxBusFailed[fx::kBusReverb].store(reverbWanted, std::memory_order_relaxed);
      return;
    }
  }

  FxCore& core = *g_fx.core;
  if (delayWanted && !core.buses.DelayAllocated())
  {
    g_fxBusFailed[fx::kBusDelay].store(!core.buses.AllocateDelay(), std::memory_order_relaxed);
  }
  if (reverbWanted && !core.buses.ReverbAllocated())
  {
    g_fxBusFailed[fx::kBusReverb].store(!core.buses.AllocateReverb(), std::memory_order_relaxed);
  }
  for (std::size_t track = 0; track < g_fx.tracks.size(); ++track)
  {
    if (g_fx.tracks[track].inserts.chorus > 0)
    {
      (void)core.inserts[track].AllocateChorus();  // Without its line the chorus stays bypassed.
    }
    // Every track with effects renders into its bus; without the memory an AMY track still gets its
    // sends (from AMY's mix), only its inserts are lost.
    const EngineState& engine = g_engines[track];
    const bool needsBus = engine.configured && (UsesAmy(engine.kind) || engine.kind == SynthEngineKind::Sampler) &&
                          TrackFxRequested(track);
    if (needsBus && !g_fx.bus[track])
    {
      g_fx.bus[track].reset(new (std::nothrow) float[kAmyFramesPerBlock]);
    }
  }
}

// A track goes through its effects while its settings ask for them or its inserts and sends are
// still fading out, through its bus; an AMY track without a bus keeps only its sends, fed from
// AMY's mix.
void UpdateTrackRoutingNoLock()
{
  if (!g_fx.core)
  {
    g_fx.routed.fill(false);
    g_fx.insertRouted.fill(false);
    return;
  }
  const FxCore& core = *g_fx.core;
  for (std::size_t track = 0; track < g_engines.size(); ++track)
  {
    const EngineState& engine = g_engines[track];
    const TrackFxParams& params = g_fx.tracks[track];
    const bool insertsWanted = !fx::dsp::IsNeutral(params.inserts) || !core.inserts[track].Settled();
    const bool sendsWanted = (params.delaySend > 0 && core.buses.DelayAllocated()) || core.delayGain[track] != 0.0f ||
                             (params.reverbSend > 0 && core.buses.ReverbAllocated()) || core.reverbGain[track] != 0.0f;
    const bool hasBus = g_fx.bus[track] != nullptr;
    bool inserts = false;
    bool sends = false;
    if (engine.configured && engine.kind == SynthEngineKind::Sampler)
    {
      inserts = hasBus && (insertsWanted || sendsWanted);
    }
    else if (engine.configured && UsesAmy(engine.kind))
    {
      inserts = hasBus && (insertsWanted || sendsWanted);
      sends = !inserts && sendsWanted;
    }
    g_fx.insertRouted[track] = inserts;
    g_fx.routed[track] = inserts || sends;
  }
}

bool FxFloatPathWantedNoLock()
{
  if (!g_fx.core)
  {
    return false;
  }
  const FxCore& core = *g_fx.core;
  return std::any_of(g_fx.routed.begin(), g_fx.routed.end(), [](bool routed) { return routed; }) ||
         !MasterFxNeutral(g_fx.master) || !core.master.Bypassed() ||
         (core.buses.DelayAllocated() && !core.buses.DelayIdle()) || (core.buses.ReverbAllocated() && !core.buses.ReverbIdle());
}

float DelaySendTarget(std::size_t track)
{
  return g_fx.core->buses.DelayAllocated() ? fx::dsp::KnobGain(g_fx.tracks[track].delaySend) : 0.0f;
}

float ReverbSendTarget(std::size_t track)
{
  return g_fx.core->buses.ReverbAllocated() ? fx::dsp::KnobGain(g_fx.tracks[track].reverbSend) : 0.0f;
}

// Runs a routed track's samples through its inserts and adds them to the mix and, post-fader
// (the track gain is already in the samples), to the sends, in one pass.
SYNTH_FX_HOT void MixRoutedTrackNoLock(std::size_t track, float* samples, float* mix, float* delaySend, float* reverbSend, std::size_t frames)
{
  FxCore& core = *g_fx.core;
  core.inserts[track].Process(samples, frames, g_fx.tracks[track].inserts);
  const float delayFrom = core.delayGain[track];
  const float delayTo = DelaySendTarget(track);
  const float reverbFrom = core.reverbGain[track];
  const float reverbTo = ReverbSendTarget(track);
  core.delayGain[track] = delayTo;
  core.reverbGain[track] = reverbTo;
  if (delayFrom == 0.0f && delayTo == 0.0f && reverbFrom == 0.0f && reverbTo == 0.0f)
  {
    for (std::size_t index = 0; index < frames; ++index) mix[index] += samples[index];
    return;
  }
  if (delayFrom == delayTo && reverbFrom == reverbTo)
  {
    for (std::size_t index = 0; index < frames; ++index)
    {
      const float sample = samples[index];
      mix[index] += sample;
      delaySend[index] += sample * delayTo;
      reverbSend[index] += sample * reverbTo;
    }
    return;
  }
  const float inverse = frames == kAmyFramesPerBlock ? 1.0f / static_cast<float>(kAmyFramesPerBlock) : 1.0f / static_cast<float>(frames);
  const float delayStep = (delayTo - delayFrom) * inverse;
  const float reverbStep = (reverbTo - reverbFrom) * inverse;
  float delayGain = delayFrom, reverbGain = reverbFrom;
  for (std::size_t index = 0; index < frames; ++index)
  {
    const float sample = samples[index];
    delayGain += delayStep;
    reverbGain += reverbStep;
    mix[index] += sample;
    delaySend[index] += sample * delayGain;
    reverbSend[index] += sample * reverbGain;
  }
}

void QueuePendingEvent(PendingEvent event)
{
  std::lock_guard<std::mutex> lock(g_pendingEventMutex);
  g_pendingEvents.push_back(std::move(event));
}

void ApplyPendingEvent(const PendingEvent& event)
{
  if (event.engineIndex >= g_engines.size())
  {
    return;
  }

  EngineState& engine = g_engines[event.engineIndex];
  switch (event.type)
  {
    case PendingEventType::EnsureEngine:
      if (!engine.configured || engine.kind != event.kind)
      {
        const bool kindChanged = engine.kind != event.kind;
        ResetEngineNoLock(engine, event.engineIndex, event.kind);
        if (UsesAmy(event.kind))
        {
          ConfigureAmySynthNoLock(engine);
        }
        if (kindChanged)
        {
          ResetTrackFxNoLock(event.engineIndex, 0);
          if (g_fx.core)
          {
            // New voices: the old engine's insert state and send ramps must not leak into them.
            g_fx.core->inserts[event.engineIndex].Reset();
            g_fx.core->delayGain[event.engineIndex] = 0.0f;
            g_fx.core->reverbGain[event.engineIndex] = 0.0f;
          }
        }
      }
      break;
    case PendingEventType::SetPreset:
      if (!engine.configured)
      {
        return;
      }

      if (engine.kind == SynthEngineKind::Dx7)
      {
        if (engine.noteOffset != 0)
        {
          ReleaseAllAmyNotesNoLock(engine);
          engine.noteOffset = 0;
        }
        if (const Dx7FactoryPresetDefinition* preset = FindDx7FactoryPreset(event.text))
        {
          LoadDx7FactoryPresetNoLock(engine, *preset);
        }
        else
        {
          engine.patchNumber = MapDx7Preset(event.text);
          ConfigureAmySynthNoLock(engine);
        }
        std::copy_n(factory::kDx7[engine.patchNumber - 128], engine.dx7.size(), engine.dx7.begin());
        engine.noteOffset = factory::Dx7Value(engine.dx7.data(), "global.transpose");
      }
      else if (engine.kind == SynthEngineKind::Juno)
      {
        if (engine.noteOffset != 0)
        {
          ReleaseAllAmyNotesNoLock(engine);
          engine.noteOffset = 0;
        }
        engine.patchNumber = MapJunoPreset(event.text);
        std::copy_n(factory::kJuno[engine.patchNumber], engine.juno.size(), engine.juno.begin());
        ConfigureAmySynthNoLock(engine);
      }
      else if (engine.kind == SynthEngineKind::Drums && event.text == "drums.808")
      {
        engine.drumLevels.fill(100);
        engine.drumMaster = 100;
        ConfigureAmySynthNoLock(engine);
      }
      // Every preset starts from neutral effects, except the Juno chorus stored in the patch.
      ResetTrackFxNoLock(event.engineIndex, engine.kind == SynthEngineKind::Juno ?
          factory::JunoValue(factory::kJuno[std::clamp(engine.patchNumber, 0, 127)], "fx.chorus") : 0);
      break;
    case PendingEventType::SetParameter:
      if (fx::IsTrackParameter(event.text))
      {
        // Track effects belong to the track, not to the engine state (wiped on engine switches).
        SetTrackFxParameterNoLock(event.engineIndex, event.text, event.value);
        break;
      }
      switch (engine.kind)
      {
        case SynthEngineKind::Dx7:
          ApplyDx7ParameterNoLock(engine, event.text, event.value);
          break;
        case SynthEngineKind::Juno:
          ApplyJunoParameterNoLock(engine, event.text, event.value);
          break;
        case SynthEngineKind::Drums:
          if (event.text == "master.level")
          {
            engine.drumMaster = std::clamp(event.value, 0, 127);
            auto update = SynthEvent(engine, 0);
            update.amp_coefs[amy_embed::COEF_CONST] = 5.0f * Normalize(engine.drumMaster, 0, 127);
            amy_add_event(&update);
          }
          else
          {
            const auto parameter = std::find(kDrumParameterIds.begin(), kDrumParameterIds.end(), event.text);
            if (parameter != kDrumParameterIds.end()) engine.drumLevels[parameter - kDrumParameterIds.begin()] = std::clamp(event.value, 0, 127);
          }
          break;
        case SynthEngineKind::Sampler:
          if (event.text == "sample.start") engine.sampler.sampleStart = event.value;
          else if (event.text == "sample.end") engine.sampler.sampleEnd = event.value;
          else if (event.text == "sample.loop") engine.sampler.sampleLoop = event.value;
          else if (event.text == "sample.tune") engine.sampler.sampleTune = event.value;
          else if (event.text == "sample.root") engine.sampler.sampleRoot = event.value;
          else if (event.text == "sample.gain") engine.sampler.sampleGain = event.value;
          else if (event.text == "env.attack") engine.sampler.attack = event.value;
          else if (event.text == "env.release") engine.sampler.release = event.value;
          else if (event.text == "filter.cutoff") { engine.sampler.cutoff = event.value; UpdateSamplerFilter(engine.sampler); }
          else if (event.text == "filter.resonance") { engine.sampler.resonance = event.value; UpdateSamplerFilter(engine.sampler); }
          else if (event.text == "amp.level") engine.sampler.level = event.value;
          break;
        case SynthEngineKind::None:
        default:
          break;
      }
      break;
    case PendingEventType::SetSamplerFrames:
      engine.sampler.sampleFrames = event.frames;
      engine.sampler.sampleRate = event.sampleRate == 0 ? kSampleRate : event.sampleRate;
      for (SamplerVoice& voice : engine.samplerVoices)
      {
        voice = SamplerVoice{};
      }
      // Sampler cleanup can arrive after a track switches to AMY and plays a
      // note. Only sampler voices are reset here; keep AMY's release tracking.
      break;
    case PendingEventType::NoteOn:
      if (!engine.configured || engine.kind == SynthEngineKind::None)
      {
        return;
      }
      // A muted track drops new notes. Velocity-0 note-ons act as releases, so they still pass.
      if (g_trackMix[event.engineIndex].muted && event.velocity != 0)
      {
        return;
      }

      if (UsesAmy(engine.kind))
      {
        EnsureAmyInitialized();
        float velocity = std::clamp(static_cast<float>(event.velocity) / 127.0f, 0.0f, 1.0f);
        if (engine.kind == SynthEngineKind::Drums)
        {
          const auto drum = std::find(kDrumNotes.begin(), kDrumNotes.end(), event.note);
          if (drum == kDrumNotes.end() || event.velocity == 0) return;
          velocity *= Normalize(engine.drumLevels[drum - kDrumNotes.begin()], 0, 127);
          if (velocity == 0) return;
          if (event.note == 42 && engine.activeNotes[46])
          {
            SendAmyNoteEventNoLock(engine, 46, 0.0f);
            engine.activeNotes[46] = false;
          }
        }
        if (engine.activeNotes[event.note])
        {
          SendAmyNoteEventNoLock(engine, event.note, 0.0f);
        }
        const float meterLevel = velocity;
        if (engine.kind == SynthEngineKind::Dx7)
        {
          velocity = (std::min)(velocity, kDx7MaxAmyVelocity);
        }
        SendAmyNoteEventNoLock(engine, event.note, velocity);
        engine.activeNotes[event.note] = true;
        BumpTrackActivityNoLock(event.engineIndex, meterLevel);
        return;
      }

      {
        if (!engine.sampler.sampleFrames || !engine.sampler.sampleFrames->frames || engine.sampler.sampleFrames->frameCount == 0) return;
        ReleaseSamplerNoteNoLock(engine, event.note);
        SamplerVoice& voice = AllocateSamplerVoice(engine);
        voice.note = event.note;
        voice.velocity = std::clamp(static_cast<float>(event.velocity) / 127.0f, 0.0f, 1.0f);
        voice.sampleCursor = static_cast<double>(SamplerStartFrame(engine.sampler));
        BumpTrackActivityNoLock(event.engineIndex, voice.velocity);
      }
      break;
    case PendingEventType::NoteOff:
      if (engine.kind == SynthEngineKind::Drums) return;
      if (UsesAmy(engine.kind))
      {
        SendAmyNoteEventNoLock(engine, event.note, 0.0f);
        engine.activeNotes[event.note] = false;
        return;
      }

      ReleaseSamplerNoteNoLock(engine, event.note);
      break;
    case PendingEventType::AllNotesOff:
      if (UsesAmy(engine.kind))
      {
        ReleaseAllAmyNotesNoLock(engine);
        return;
      }

      for (SamplerVoice& voice : engine.samplerVoices)
      {
        voice.releasing = true;
      }
      break;
    case PendingEventType::SetTrackGain:
      // Applied by the ramps in StepAmyTrackGainsNoLock and the sampler mix loop.
      g_trackMix[event.engineIndex].gain = event.gain;
      break;
    case PendingEventType::SetTrackMuted:
      g_trackMix[event.engineIndex].muted = event.value != 0;
      break;
    case PendingEventType::SetMasterFx:
      if (event.note < fx::kMasterParamCount)
      {
        g_fx.master[event.note] = event.value;
        g_fx.dirty = true;
      }
      break;
  }
}

void DrainPendingEvents()
{
  std::deque<PendingEvent> pending;
  {
    std::lock_guard<std::mutex> lock(g_pendingEventMutex);
    pending.swap(g_pendingEvents);
  }

  for (const PendingEvent& event : pending)
  {
    ApplyPendingEvent(event);
  }
}

bool HasAmyEnginesNoLock()
{
  for (const EngineState& engine : g_engines)
  {
    if (engine.configured && UsesAmy(engine.kind))
    {
      return true;
    }
  }

  return false;
}

bool HasActiveSamplerVoices(const EngineState& engine)
{
  return engine.configured && engine.kind == SynthEngineKind::Sampler &&
         std::any_of(engine.samplerVoices.begin(), engine.samplerVoices.end(), [](const SamplerVoice& voice) { return voice.active; });
}

bool HasActiveSamplerVoicesNoLock()
{
  return std::any_of(g_engines.begin(), g_engines.end(), HasActiveSamplerVoices);
}

// Silent sampler tracks take a new gain at once; only sounding voices need the ramp.
void SettleIdleSamplerGainsNoLock()
{
  for (std::size_t index = 0; index < g_engines.size(); ++index)
  {
    if (!HasActiveSamplerVoices(g_engines[index]))
    {
      g_trackMix[index].samplerGain = EffectiveTrackGain(g_trackMix[index]);
    }
  }
}

bool HasHeldNotes(const EngineState& engine)
{
  if (!engine.configured)
  {
    return false;
  }

  switch (engine.kind)
  {
    case SynthEngineKind::Dx7:
    case SynthEngineKind::Juno:
      return std::any_of(engine.activeNotes.begin(), engine.activeNotes.end(), [](bool active) { return active; });
    case SynthEngineKind::Sampler:
      return std::any_of(engine.samplerVoices.begin(), engine.samplerVoices.end(), [](const SamplerVoice& voice) {
        return voice.active && !voice.releasing;
      });
    default:
      // Drum pads are one-shots: their meter follows the note-on bump only.
      return false;
  }
}

void UpdateMetersNoLock(const std::int16_t* destination, std::size_t frameCount)
{
  const float decay = std::exp(-static_cast<float>(frameCount) / (kMeterDecaySeconds * static_cast<float>(kSampleRate)));

  int peak = 0;
  for (std::size_t index = 0; index < frameCount; ++index)
  {
    peak = (std::max)(peak, std::abs(static_cast<int>(destination[index])));
  }
  g_outputPeakState = (std::max)((std::min)(1.0f, static_cast<float>(peak) / 32767.0f), g_outputPeakState * decay);
  if (g_outputPeakState < kMeterFloor)
  {
    g_outputPeakState = 0.0f;
  }
  g_outputPeak.store(g_outputPeakState, std::memory_order_relaxed);

  for (std::size_t index = 0; index < g_engines.size(); ++index)
  {
    TrackMixState& mix = g_trackMix[index];
    mix.activity *= decay;
    if (HasHeldNotes(g_engines[index]))
    {
      mix.activity = (std::max)(mix.activity, mix.heldLevel * kActivityHeldFraction * EffectiveTrackGain(mix));
    }
    if (mix.activity < kMeterFloor)
    {
      mix.activity = 0.0f;
    }
    g_publishedMix[index].activity.store(mix.activity, std::memory_order_relaxed);
  }
}

void UpdateFxMetersNoLock(std::size_t frameCount, float delayPeak, float reverbPeak)
{
  const std::array<float, fx::kBusCount> peaks{{delayPeak, reverbPeak}};
  if (std::all_of(g_fx.returnMeter.begin(), g_fx.returnMeter.end(), [](float level) { return level == 0.0f; }) &&
      delayPeak == 0.0f && reverbPeak == 0.0f)
  {
    return;
  }
  static const float kBlockDecay = std::exp(-static_cast<float>(kAmyFramesPerBlock) / (kMeterDecaySeconds * static_cast<float>(kSampleRate)));
  const float decay = frameCount == kAmyFramesPerBlock ? kBlockDecay
                                                       : std::exp(-static_cast<float>(frameCount) / (kMeterDecaySeconds * static_cast<float>(kSampleRate)));
  for (std::size_t bus = 0; bus < fx::kBusCount; ++bus)
  {
    float& level = g_fx.returnMeter[bus];
    level = (std::max)((std::min)(1.0f, peaks[bus]), level * decay);
    if (level < kMeterFloor)
    {
      level = 0.0f;
    }
    g_fxReturnLevel[bus].store(level, std::memory_order_relaxed);
  }
}

// Scale from one AMY voice sample (S8.23) to the float mix ((L + R) / 65536), mirroring the gains
// a voice gets on its way out of AMY: mix_with_pan multiplies it by the centre pan gain (AMY's
// approximate sqrt(0.5), 0.7314) and amy_fill_buffer by MUL4_SS(0.1, volume), both through
// MUL8_SS, which keeps only gain >> 12; the S8.23 result then becomes int16 with 15 fraction bits.
float AmyVoiceToMixScale()
{
  constexpr float kOne = 8388608.0f;  // 1.0 in S8.23
  const auto toSample = [](float value) { return static_cast<std::int32_t>(value * kOne); };
  const std::int32_t volume = ((toSample(0.1f) >> 10) * (toSample(amy_embed::amy_global.volume[0]) >> 9)) >> 4;
  const std::int32_t pan = toSample(amy_embed::dsps_sqrtf_f32_ansi(0.5f));
  return static_cast<float>(volume >> 12) / 2048.0f * static_cast<float>(pan >> 12) / 2048.0f / kOne * kAmyOutputShift;
}

// Fills the float mix of the current AMY block from AMY's output plus the AMY tracks with inserts,
// each through its chain, and adds their sends (the send-only tracks' share arrived through the
// render hook). Without a fresh AMY render (joining mid-block) the sends start empty.
SYNTH_FX_HOT void PrepareFxAmyBlockNoLock(const std::array<FxHookTrack, kSynthTrackCount>& plan, bool rendered)
{
  FxCore& core = *g_fx.core;
  float* mix = core.mix.data();
  // With every AMY track on its own bus, AMY's mix is silent: skip reading it.
  bool amyMixUsed = !rendered;
  for (std::size_t track = 0; track < plan.size() && !amyMixUsed; ++track)
  {
    amyMixUsed = g_engines[track].configured && UsesAmy(g_engines[track].kind) && plan[track].bus == nullptr;
  }
  if (amyMixUsed)
  {
    const std::int16_t* block = g_amyBlock.data();
    constexpr float kInverse = 1.0f / 65536.0f;
    for (std::size_t frame = 0; frame < kAmyFramesPerBlock; ++frame)
    {
      mix[frame] = static_cast<float>(static_cast<std::int32_t>(block[2 * frame]) + static_cast<std::int32_t>(block[2 * frame + 1])) * kInverse;
    }
  }
  else
  {
    core.mix.fill(0.0f);
  }
  static_assert(kAmyChannelCount == 2, "the float mix reads interleaved stereo AMY blocks");
  if (!rendered)
  {
    core.delaySend.fill(0.0f);
    core.reverbSend.fill(0.0f);
  }
  for (std::size_t track = 0; track < plan.size(); ++track)
  {
    if (plan[track].bus != nullptr)
    {
      MixRoutedTrackNoLock(track, plan[track].bus, mix, core.delaySend.data(), core.reverbSend.data(), kAmyFramesPerBlock);
    }
  }
  g_fx.blockPrepared = true;
}

// Plans what the render hook does with every AMY track in the next block: tracks with inserts
// render into their bus, tracks that only send feed the sends from AMY's mix (their send ramps
// advance here, one block at a time).
std::array<FxHookTrack, kSynthTrackCount> PlanFxAmyBlockNoLock()
{
  FxCore& core = *g_fx.core;
  UpdateTrackRoutingNoLock();
  std::array<FxHookTrack, kSynthTrackCount> plan{};
  core.delaySend.fill(0.0f);
  core.reverbSend.fill(0.0f);
  g_fxVoiceScale = AmyVoiceToMixScale();
  constexpr float kInverseBlock = 1.0f / static_cast<float>(kAmyFramesPerBlock);
  for (std::size_t track = 0; track < plan.size(); ++track)
  {
    if (!g_fx.routed[track] || !UsesAmy(g_engines[track].kind))
    {
      continue;
    }
    FxHookTrack& entry = plan[track];
    if (g_fx.insertRouted[track])
    {
      entry.bus = g_fx.bus[track].get();
      std::fill_n(entry.bus, kAmyFramesPerBlock, 0.0f);
      continue;
    }
    const float delayFrom = core.delayGain[track];
    const float delayTo = DelaySendTarget(track);
    const float reverbFrom = core.reverbGain[track];
    const float reverbTo = ReverbSendTarget(track);
    core.delayGain[track] = delayTo;
    core.reverbGain[track] = reverbTo;
    if (delayFrom != 0.0f || delayTo != 0.0f)
    {
      entry.delaySend = core.delaySend.data();
      entry.delayGain = delayFrom * g_fxVoiceScale;
      entry.delayStep = (delayTo - delayFrom) * g_fxVoiceScale * kInverseBlock;
    }
    if (reverbFrom != 0.0f || reverbTo != 0.0f)
    {
      entry.reverbSend = core.reverbSend.data();
      entry.reverbGain = reverbFrom * g_fxVoiceScale;
      entry.reverbStep = (reverbTo - reverbFrom) * g_fxVoiceScale * kInverseBlock;
    }
  }
  return plan;
}

void FetchAmyBlockNoLock(bool effects = false)
{
  EnsureAmyInitialized();
  StepAmyTrackGainsNoLock();
  std::array<FxHookTrack, kSynthTrackCount> plan{};
  if (effects)
  {
    plan = PlanFxAmyBlockNoLock();
    g_fxHook = plan;
  }
#if defined(ESP_PLATFORM) && defined(CARDENZA_AMY_EXPERIMENTAL_MULTICORE)
  const output_sample_type* block = amy_update();
#else
  const output_sample_type* block = amy_simple_fill_buffer();
#endif
  if (block != nullptr)
  {
    std::memcpy(g_amyBlock.data(), block, sizeof(std::int16_t) * g_amyBlock.size());
  }
  else
  {
    std::fill(g_amyBlock.begin(), g_amyBlock.end(), 0);
  }
  g_amyBlockCursor = 0;
  if (effects)
  {
    g_fxHook = {};
    PrepareFxAmyBlockNoLock(plan, true);
  }
  else
  {
    g_fx.blockPrepared = false;
  }
}

// Sampler tracks on the effects path: the legacy per-frame gain ramp and voice mix, with routed
// tracks summed into their bus first and then run through their inserts.
void RenderFxSamplerTracksNoLock(float* mix, float* delaySend, float* reverbSend, std::size_t frames,
                                 const std::array<float, kSynthTrackCount>& targets)
{
  for (std::size_t track = 0; track < g_engines.size(); ++track)
  {
    EngineState& engine = g_engines[track];
    if (!engine.configured || engine.kind != SynthEngineKind::Sampler)
    {
      continue;
    }
    TrackMixState& trackMix = g_trackMix[track];
    const bool routed = g_fx.insertRouted[track];
    if (!routed && !HasActiveSamplerVoices(engine))
    {
      for (std::size_t frame = 0; frame < frames; ++frame)
      {
        if (trackMix.samplerGain != targets[track])
        {
          trackMix.samplerGain = ApproachGain(trackMix.samplerGain, targets[track], kTrackGainStepPerFrame);
        }
      }
      continue;
    }

    float* out = routed ? g_fx.bus[track].get() : mix;
    if (routed)
    {
      std::fill_n(out, frames, 0.0f);
    }
    for (std::size_t frame = 0; frame < frames; ++frame)
    {
      if (trackMix.samplerGain != targets[track])
      {
        trackMix.samplerGain = ApproachGain(trackMix.samplerGain, targets[track], kTrackGainStepPerFrame);
      }
      const float trackGain = trackMix.samplerGain;
      for (SamplerVoice& voice : engine.samplerVoices)
      {
        if (voice.active)
        {
          out[frame] += RenderSamplerVoice(engine.sampler, voice) * 0.45f * trackGain;
        }
      }
    }
    if (routed)
    {
      MixRoutedTrackNoLock(track, out, mix, delaySend, reverbSend, frames);
    }
  }
}

// The effects path's output stage: tanh(0.75 x), crossfaded with the AMY-only path's linear
// stage while `blend` moves towards `target`.
SYNTH_FX_HOT void WriteFxOutput(const float* mix, std::int16_t* out, std::size_t frames, const fx::dsp::TanhTable& tanh, float& blend, float target)
{
  if (blend == 1.0f && target == 1.0f)
  {
    for (std::size_t offset = 0; offset < frames; ++offset)
    {
      out[offset] = static_cast<std::int16_t>(tanh(mix[offset] * kOutputGain) * 32767.0f);  // |tanh| < 1
    }
    return;
  }
  for (std::size_t offset = 0; offset < frames; ++offset)
  {
    const float sample = mix[offset] * kOutputGain;
    const float linear = std::clamp(sample, -1.0f, 1.0f);
    out[offset] = FloatToInt16(linear + blend * (tanh(sample) - linear));
    blend = ApproachGain(blend, target, 1.0f / static_cast<float>(kAmyFramesPerBlock));
  }
}

void RenderFxFramesNoLock(std::int16_t* destination, std::size_t frameCount, bool hasAmyEngines, float outputTarget)
{
  FxCore& core = *g_fx.core;
  std::array<float, kSynthTrackCount> samplerTargets{};
  for (std::size_t index = 0; index < g_engines.size(); ++index)
  {
    samplerTargets[index] = EffectiveTrackGain(g_trackMix[index]);
  }
  const auto value = [](fx::MasterParam param) { return MasterFxValue(param); };
  fx::dsp::SendBusSettings buses;
  buses.delaySeconds = fx::dsp::DelaySeconds(value(fx::MasterParam::DelayTime), g_fxTempo.load(std::memory_order_relaxed));
  buses.delayFeedback = value(fx::MasterParam::DelayFeedback);
  buses.delayTone = value(fx::MasterParam::DelayTone);
  buses.delayLevel = value(fx::MasterParam::DelayLevel);
  buses.reverbSize = value(fx::MasterParam::ReverbSize);
  buses.reverbDamp = value(fx::MasterParam::ReverbDamp);
  buses.reverbLowCut = value(fx::MasterParam::ReverbLowCut);
  buses.reverbLevel = value(fx::MasterParam::ReverbLevel);

  std::size_t frameIndex = 0;
  while (frameIndex < frameCount)
  {
    std::size_t base = 0;
    std::size_t frames = 0;
    if (hasAmyEngines)
    {
      if (g_amyBlockCursor >= kAmyFramesPerBlock)
      {
        FetchAmyBlockNoLock(true);
      }
      else if (!g_fx.blockPrepared)
      {
        // Joined in the middle of a block rendered without effects: AMY already mixed every voice.
        PrepareFxAmyBlockNoLock({}, false);
      }
      base = g_amyBlockCursor;
      frames = (std::min)(frameCount - frameIndex, kAmyFramesPerBlock - base);
    }
    else
    {
      frames = (std::min)(frameCount - frameIndex, kAmyFramesPerBlock);
      std::fill_n(core.mix.data(), frames, 0.0f);
      std::fill_n(core.delaySend.data(), frames, 0.0f);
      std::fill_n(core.reverbSend.data(), frames, 0.0f);
      g_fx.blockPrepared = false;
    }

    float* mix = core.mix.data() + base;
    float* delaySend = core.delaySend.data() + base;
    float* reverbSend = core.reverbSend.data() + base;
    UpdateTrackRoutingNoLock();
    RenderFxSamplerTracksNoLock(mix, delaySend, reverbSend, frames, samplerTargets);
    core.buses.Process(delaySend, reverbSend, mix, frames, buses);
    core.master.Process(mix, frames, value(fx::MasterParam::EqLow), value(fx::MasterParam::EqHigh), value(fx::MasterParam::Compressor),
                        value(fx::MasterParam::Tape));
    WriteFxOutput(mix, destination + frameIndex, frames, core.tanh, g_fx.outputBlend, outputTarget);
    UpdateFxMetersNoLock(frames, core.buses.DelayPeak(), core.buses.ReverbPeak());

    if (hasAmyEngines)
    {
      g_amyBlockCursor += frames;
    }
    frameIndex += frames;
  }
}

void RenderAmyOnlyFramesNoLock(std::int16_t* destination, std::size_t frameCount)
{
  std::size_t frameIndex = 0;
  while (frameIndex < frameCount)
  {
    if (g_amyBlockCursor >= kAmyFramesPerBlock)
    {
      FetchAmyBlockNoLock();
    }

    const std::size_t framesAvailable = (std::min)(frameCount - frameIndex, kAmyFramesPerBlock - g_amyBlockCursor);
    for (std::size_t offset = 0; offset < framesAvailable; ++offset)
    {
      const std::size_t sampleIndex = (g_amyBlockCursor + offset) * kAmyChannelCount;
      const std::int32_t stereoSum = static_cast<std::int32_t>(g_amyBlock[sampleIndex]) +
                                     static_cast<std::int32_t>(g_amyBlock[sampleIndex + 1]);
      destination[frameIndex + offset] = ClampToInt16((stereoSum * kAmyOnlyOutputGainNumerator) / kAmyOnlyOutputGainDenominator);
    }

    g_amyBlockCursor += framesAvailable;
    frameIndex += framesAvailable;
  }
}

std::size_t RenderFramesNoLock(std::int16_t* destination, std::size_t frameCount)
{
  if (destination == nullptr)
  {
    return 0;
  }

  DrainPendingEvents();
  UpdateFxNoLock();
  SettleIdleSamplerGainsNoLock();

  const bool hasAmyEngines = HasAmyEnginesNoLock();
  const bool hasActiveSamplerVoices = HasActiveSamplerVoicesNoLock();
  // The effects path starts as soon as anything needs it and ends on an AMY block boundary once
  // nothing is routed, the master chain is neutral and the delay and reverb tails have died out
  // (and its output stage has faded back to the plain path's).
  UpdateTrackRoutingNoLock();
  float outputTarget = 1.0f;
  if (FxFloatPathWantedNoLock())
  {
    if (!g_fx.floatPath)
    {
      g_fx.floatPath = true;
      g_fx.outputBlend = g_fx.lastPathLinear ? 0.0f : 1.0f;
    }
  }
  else if (g_fx.floatPath)
  {
    outputTarget = hasAmyEngines && !hasActiveSamplerVoices ? 0.0f : 1.0f;
    if ((!hasAmyEngines || g_amyBlockCursor >= kAmyFramesPerBlock) && g_fx.outputBlend == outputTarget)
    {
      g_fx.floatPath = false;
      g_fx.blockPrepared = false;
    }
  }
  if (g_fx.floatPath)
  {
    RenderFxFramesNoLock(destination, frameCount, hasAmyEngines, outputTarget);
    UpdateMetersNoLock(destination, frameCount);
    return frameCount;
  }
  UpdateFxMetersNoLock(frameCount, 0.0f, 0.0f);

  if (!hasAmyEngines && !hasActiveSamplerVoices)
  {
    std::fill(destination, destination + frameCount, 0);
    UpdateMetersNoLock(destination, frameCount);
    return frameCount;
  }

  g_fx.lastPathLinear = hasAmyEngines && !hasActiveSamplerVoices;
  if (hasAmyEngines && !hasActiveSamplerVoices)
  {
    RenderAmyOnlyFramesNoLock(destination, frameCount);
    UpdateMetersNoLock(destination, frameCount);
    return frameCount;
  }

  std::array<float, kSynthTrackCount> samplerTargets{};
  for (std::size_t index = 0; index < g_engines.size(); ++index)
  {
    samplerTargets[index] = EffectiveTrackGain(g_trackMix[index]);
  }

  for (std::size_t frameIndex = 0; frameIndex < frameCount; ++frameIndex)
  {
    float mixedSample = 0.0f;

    if (hasAmyEngines)
    {
      if (g_amyBlockCursor >= kAmyFramesPerBlock)
      {
        FetchAmyBlockNoLock();
      }

      const std::size_t sampleIndex = g_amyBlockCursor * kAmyChannelCount;
      mixedSample += (static_cast<float>(g_amyBlock[sampleIndex]) + static_cast<float>(g_amyBlock[sampleIndex + 1])) / 65536.0f;
      ++g_amyBlockCursor;
    }

    for (std::size_t index = 0; index < g_engines.size(); ++index)
    {
      EngineState& engine = g_engines[index];
      if (!engine.configured || engine.kind != SynthEngineKind::Sampler)
      {
        continue;
      }

      TrackMixState& mix = g_trackMix[index];
      if (mix.samplerGain != samplerTargets[index])
      {
        mix.samplerGain = ApproachGain(mix.samplerGain, samplerTargets[index], kTrackGainStepPerFrame);
      }
      // Muted voices keep running silently, so held notes resume on unmute.
      const float trackGain = mix.samplerGain;
      for (SamplerVoice& voice : engine.samplerVoices)
      {
        if (!voice.active)
        {
          continue;
        }
        mixedSample += RenderSamplerVoice(engine.sampler, voice) * 0.45f * trackGain;
      }
    }

    mixedSample = std::tanh(mixedSample * kOutputGain);
    destination[frameIndex] = FloatToInt16(mixedSample);
  }

  UpdateMetersNoLock(destination, frameCount);
  return frameCount;
}

}  // namespace

void EnsureEngine(std::size_t engineIndex, SynthEngineKind kind)
{
  if (engineIndex >= g_engines.size())
  {
    return;
  }

  PendingEvent event;
  event.type = PendingEventType::EnsureEngine;
  event.engineIndex = engineIndex;
  event.kind = kind;
  QueuePendingEvent(std::move(event));
}

void SetEnginePreset(std::size_t engineIndex, std::string_view presetId)
{
  if (engineIndex >= g_engines.size())
  {
    return;
  }

  PendingEvent event;
  event.type = PendingEventType::SetPreset;
  event.engineIndex = engineIndex;
  event.text = presetId;
  QueuePendingEvent(std::move(event));
}

void SetEngineParameter(std::size_t engineIndex, std::string_view parameterId, int value)
{
  if (engineIndex >= g_engines.size())
  {
    return;
  }

  PendingEvent event;
  event.type = PendingEventType::SetParameter;
  event.engineIndex = engineIndex;
  event.value = value;
  event.text = parameterId;
  QueuePendingEvent(std::move(event));
}

void SetSamplerFrames(std::size_t engineIndex, SharedSampleBuffer frames, std::uint32_t sampleRate)
{
  if (engineIndex >= g_engines.size())
  {
    return;
  }

  PendingEvent event;
  event.type = PendingEventType::SetSamplerFrames;
  event.engineIndex = engineIndex;
  event.frames = std::move(frames);
  event.sampleRate = sampleRate;
  QueuePendingEvent(std::move(event));
}

void NoteOn(std::size_t engineIndex, std::uint8_t note, std::uint8_t velocity)
{
  if (engineIndex >= g_engines.size() || note > 127)
  {
    return;
  }

  PendingEvent event;
  event.type = PendingEventType::NoteOn;
  event.engineIndex = engineIndex;
  event.note = note;
  event.velocity = velocity;
  QueuePendingEvent(std::move(event));
}

void NoteOff(std::size_t engineIndex, std::uint8_t note)
{
  if (engineIndex >= g_engines.size() || note > 127)
  {
    return;
  }

  PendingEvent event;
  event.type = PendingEventType::NoteOff;
  event.engineIndex = engineIndex;
  event.note = note;
  QueuePendingEvent(std::move(event));
}

void AllNotesOff(std::size_t engineIndex)
{
  if (engineIndex >= g_engines.size())
  {
    return;
  }

  PendingEvent event;
  event.type = PendingEventType::AllNotesOff;
  event.engineIndex = engineIndex;
  QueuePendingEvent(std::move(event));
}

void SetTrackGain(std::size_t engineIndex, float gain)
{
  if (engineIndex >= g_engines.size())
  {
    return;
  }

  // NaN and negative values close the fader.
  gain = gain > 0.0f ? (std::min)(gain, 1.0f) : 0.0f;
  PendingEvent event;
  event.type = PendingEventType::SetTrackGain;
  event.engineIndex = engineIndex;
  event.gain = gain;
  // Publish and queue under one lock, so concurrent setters cannot reorder the two.
  std::lock_guard<std::mutex> lock(g_pendingEventMutex);
  if (g_publishedMix[engineIndex].gain.exchange(gain, std::memory_order_relaxed) != gain)
  {
    g_pendingEvents.push_back(std::move(event));
  }
}

float TrackGain(std::size_t engineIndex)
{
  return engineIndex < g_publishedMix.size() ? g_publishedMix[engineIndex].gain.load(std::memory_order_relaxed) : 1.0f;
}

void SetTrackMuted(std::size_t engineIndex, bool muted)
{
  if (engineIndex >= g_engines.size())
  {
    return;
  }

  PendingEvent event;
  event.type = PendingEventType::SetTrackMuted;
  event.engineIndex = engineIndex;
  event.value = muted ? 1 : 0;
  std::lock_guard<std::mutex> lock(g_pendingEventMutex);
  if (g_publishedMix[engineIndex].muted.exchange(muted, std::memory_order_relaxed) != muted)
  {
    g_pendingEvents.push_back(std::move(event));
  }
}

bool TrackMuted(std::size_t engineIndex)
{
  return engineIndex < g_publishedMix.size() && g_publishedMix[engineIndex].muted.load(std::memory_order_relaxed);
}

float TrackActivity(std::size_t engineIndex)
{
  return engineIndex < g_publishedMix.size() ? g_publishedMix[engineIndex].activity.load(std::memory_order_relaxed) : 0.0f;
}

float OutputPeak()
{
  return g_outputPeak.load(std::memory_order_relaxed);
}

void SetMasterFx(fx::MasterParam param, int value)
{
  const auto index = static_cast<std::size_t>(param);
  if (index >= fx::kMasterParamCount)
  {
    return;
  }
  value = std::clamp(value, fx::kMasterParams[index].minValue, fx::kMasterParams[index].maxValue);
  PendingEvent event;
  event.type = PendingEventType::SetMasterFx;
  event.note = static_cast<std::uint8_t>(index);
  event.value = value;
  // Publish and queue under one lock, so concurrent setters cannot reorder the two.
  std::lock_guard<std::mutex> lock(g_pendingEventMutex);
  if (g_publishedMasterFx[index].exchange(value, std::memory_order_relaxed) != value)
  {
    g_pendingEvents.push_back(std::move(event));
  }
}

int MasterFx(fx::MasterParam param)
{
  const auto index = static_cast<std::size_t>(param);
  return index < fx::kMasterParamCount ? g_publishedMasterFx[index].load(std::memory_order_relaxed) : 0;
}

void SetTempo(float bpm)
{
  if (bpm > 0.0f && std::isfinite(bpm))
  {
    g_fxTempo.store(bpm, std::memory_order_relaxed);
  }
}

float FxReturnLevel(std::size_t bus)
{
  return bus < fx::kBusCount ? g_fxReturnLevel[bus].load(std::memory_order_relaxed) : 0.0f;
}

bool FxBusAvailable(std::size_t bus)
{
  return bus < fx::kBusCount && !g_fxBusFailed[bus].load(std::memory_order_relaxed);
}

void WarmUp()
{
  std::lock_guard<std::mutex> lock(g_renderMutex);
  // Allocate the backend before the device starts requesting audio. Engine changes remain
  // queued for the render thread; rendering scratch audio here would skip live voices and tails.
  EnsureAmyInitialized();
}

std::size_t RenderFrames(std::int16_t* destination, std::size_t frameCount)
{
  std::lock_guard<std::mutex> lock(g_renderMutex);
  return RenderFramesNoLock(destination, frameCount);
}

}  // namespace synth::audio
