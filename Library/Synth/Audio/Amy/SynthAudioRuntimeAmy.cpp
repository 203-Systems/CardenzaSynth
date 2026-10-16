#include "Library/Synth/Audio/SynthAudioRuntime.h"
#include "Library/Synth/Core/Engines/Dx7/Dx7FactoryPresets.h"
#include "Library/Synth/Core/Engines/Juno/JunoFactoryPresets.h"
#include "Library/Synth/Core/Engines/Drums/DrumsSynthEngine.h"
#include "Library/Synth/Audio/Amy/FactoryParameters.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <string_view>
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
#if defined(ESP_PLATFORM) && defined(CCOS_AMY_EXPERIMENTAL_MULTICORE)
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
};

std::once_flag g_amyInitFlag;
bool g_amyInitialized = false;
std::array<EngineState, kSynthTrackCount> g_engines{};
std::array<std::int16_t, kAmyFramesPerBlock * kAmyChannelCount> g_amyBlock{};
std::size_t g_amyBlockCursor = kAmyFramesPerBlock;
std::deque<PendingEvent> g_pendingEvents;
std::mutex g_pendingEventMutex;
std::mutex g_renderMutex;

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
    // CCOS sequences in SynthSequencer; AMY's own tag table would only cost internal RAM.
    config.max_sequencer_tags = 1;
    config.max_synths = kAmyMaxSynths;
    config.max_voices = kAmyMaxVoices;
    config.max_oscs = kAmyMaxOscs;
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
  if (offset >= 0) engine.juno[offset] = static_cast<std::uint8_t>(std::clamp(value, 0, 127));
  else if (id == "dco.saw") engine.juno[16] = (engine.juno[16] & ~16u) | (value ? 16u : 0u);
  else if (id == "dco.range") engine.juno[16] = (engine.juno[16] & ~7u) | (1u << std::clamp(value, 0, 2));
  else return;
  const auto& p = engine.juno;
  const auto normalized = [&](int i) { return p[i] / 127.0f; };
  if (id.substr(0, 4) == "lfo.")
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
  if (id.substr(0, 4) == "dco.")
  {
    const float baseFrequency = static_cast<float>(ZERO_LOGFREQ_IN_HZ) * ((p[16] & 1) ? 0.5f : ((p[16] & 4) ? 2.0f : 1.0f));
    for (int osc = 2; osc <= 5; ++osc)
    {
      auto event = SynthEvent(engine, osc);
      event.amp_coefs[amy_embed::COEF_CONST] = osc == 2 ? float((p[16] & 8) != 0) :
          osc == 3 ? float((p[16] & 16) != 0) : normalized(osc == 4 ? 15 : 4);
      if (osc < 5) event.freq_coefs[amy_embed::COEF_CONST] = baseFrequency * (osc == 4 ? 0.5f : 1.0f);
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
  if (id.substr(0, 4) == "vcf.")
  {
    auto event = SynthEvent(engine, 0);
    event.resonance = 0.7f * std::exp2(4.0f * normalized(6));
    event.filter_freq_coefs[amy_embed::COEF_CONST] = 13.0f * std::exp2(0.0938f * p[5] + 0.75f * normalized(9));
    const float amount = 11.0f * ((p[17] & 2) ? -1.0f : 1.0f) * normalized(7);
    event.filter_freq_coefs[amy_embed::COEF_EG0] = (p[17] & 4) ? 0.0f : amount;
    event.filter_freq_coefs[amy_embed::COEF_EG1] = (p[17] & 4) ? amount : 0.0f;
    event.filter_freq_coefs[amy_embed::COEF_MOD] = 1.25f * normalized(8);
    amy_add_event(&event);
    return;
  }
  if (id == "vca.level" || id.substr(0, 4) == "env.")
  {
    auto event = SynthEvent(engine, 0);
    event.amp_coefs[amy_embed::COEF_CONST] = normalized(10);
    // Factory gate patches use envelope 1 for the VCF; preserve their gate routing.
    auto* times = (p[17] & 4) ? event.eg1_times : event.eg0_times;
    auto* levels = (p[17] & 4) ? event.eg1_values : event.eg0_values;
    times[0] = 6 + 8 * p[11]; levels[0] = 1;
    times[1] = static_cast<std::uint32_t>(80 * std::exp2(0.085f * p[12]) - 80); levels[1] = normalized(13);
    times[2] = static_cast<std::uint32_t>(70 * std::exp2(0.066f * p[14]) - 70); levels[2] = 0;
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
  engine.dx7[offset] = static_cast<std::uint8_t>(std::clamp(raw, 0, 99));
  const auto& p = engine.dx7;
  if (id == "global.algorithm" || id == "global.feedback" || id == "global.lfo_pitch")
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
      else if (offset % 21 >= 18)
      {
        const float fine = op[19] + (op[20] - 7.0f) / 8.0f;
        if (op[17] == 0) event.ratio = (op[18] == 0 ? 0.5f : float(op[18])) * (1.0f + fine / 100.0f);
        else event.freq_coefs[amy_embed::COEF_CONST] = std::pow(10.0f, (op[18] & 3) + fine / 100.0f);
      }
      else Dx7OperatorEnvelope(event, op);
      amy_add_event(&event);
    }
  }
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
    event.synth_flags = _SYNTH_FLAGS_MIDI_DRUMS;
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
        ResetEngineNoLock(engine, event.engineIndex, event.kind);
        if (UsesAmy(event.kind))
        {
          ConfigureAmySynthNoLock(engine);
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
      break;
    case PendingEventType::SetParameter:
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
          else if (event.text == "fx.drive") engine.sampler.drive = event.value;
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
        if (engine.kind == SynthEngineKind::Dx7)
        {
          velocity = (std::min)(velocity, kDx7MaxAmyVelocity);
        }
        SendAmyNoteEventNoLock(engine, event.note, velocity);
        engine.activeNotes[event.note] = true;
        return;
      }

      {
        if (!engine.sampler.sampleFrames || !engine.sampler.sampleFrames->frames || engine.sampler.sampleFrames->frameCount == 0) return;
        ReleaseSamplerNoteNoLock(engine, event.note);
        SamplerVoice& voice = AllocateSamplerVoice(engine);
        voice.note = event.note;
        voice.velocity = std::clamp(static_cast<float>(event.velocity) / 127.0f, 0.0f, 1.0f);
        voice.sampleCursor = static_cast<double>(SamplerStartFrame(engine.sampler));
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

bool HasActiveSamplerVoicesNoLock()
{
  for (const EngineState& engine : g_engines)
  {
    if (!engine.configured || engine.kind != SynthEngineKind::Sampler)
    {
      continue;
    }

    for (const SamplerVoice& voice : engine.samplerVoices)
    {
      if (voice.active)
      {
        return true;
      }
    }
  }

  return false;
}

void FetchAmyBlockNoLock()
{
  EnsureAmyInitialized();
#if defined(ESP_PLATFORM) && defined(CCOS_AMY_EXPERIMENTAL_MULTICORE)
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

  const bool hasAmyEngines = HasAmyEnginesNoLock();
  const bool hasActiveSamplerVoices = HasActiveSamplerVoicesNoLock();
  if (!hasAmyEngines && !hasActiveSamplerVoices)
  {
    std::fill(destination, destination + frameCount, 0);
    return frameCount;
  }

  if (hasAmyEngines && !hasActiveSamplerVoices)
  {
    RenderAmyOnlyFramesNoLock(destination, frameCount);
    return frameCount;
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

    for (EngineState& engine : g_engines)
    {
      if (!engine.configured || engine.kind != SynthEngineKind::Sampler)
      {
        continue;
      }

      for (SamplerVoice& voice : engine.samplerVoices)
      {
        if (!voice.active)
        {
          continue;
        }
        mixedSample += RenderSamplerVoice(engine.sampler, voice) * 0.45f;
      }
    }

    mixedSample = std::tanh(mixedSample * kOutputGain);
    destination[frameIndex] = FloatToInt16(mixedSample);
  }

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

void WarmUp()
{
  std::array<std::int16_t, kAmyFramesPerBlock> scratch{};
  std::lock_guard<std::mutex> lock(g_renderMutex);
  (void)RenderFramesNoLock(scratch.data(), scratch.size());
}

std::size_t RenderFrames(std::int16_t* destination, std::size_t frameCount)
{
  std::lock_guard<std::mutex> lock(g_renderMutex);
  return RenderFramesNoLock(destination, frameCount);
}

}  // namespace synth::audio
