#include "Library/Synth/Audio/SynthAudioRuntime.h"

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
using amy_embed::output_sample_type;

namespace synth::audio {

namespace {

constexpr std::size_t kAmyFramesPerBlock = AMY_BLOCK_SIZE;
constexpr std::size_t kAmyChannelCount = AMY_NCHANS;
constexpr std::size_t kSamplerVoiceCount = 4;
constexpr float kOutputGain = 0.75f;
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
  int send = 24;
  int level = 96;
  int pan = 64;
  std::vector<std::int16_t> sampleFrames;
  std::uint32_t sampleRate = kSampleRate;
};

struct SamplerVoice {
  bool active = false;
  bool releasing = false;
  std::uint8_t note = 0;
  float velocity = 0.0f;
  float envelope = 0.0f;
  float samplePosition = 0.0f;
  double sampleCursor = 0.0;
  double phase = 0.0;
  float filterState = 0.0f;
  std::uint32_t noiseState = 0x87654321u;
  std::uint64_t serial = 0;
};

struct EngineState {
  SynthEngineKind kind = SynthEngineKind::None;
  bool configured = false;
  int synthId = 0;
  int patchNumber = -1;
  int noteOffset = 0;
  int junoCutoff = 92;
  int junoResonance = 18;
  std::array<bool, 128> activeNotes{};
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
  std::vector<std::int16_t> frames;
  std::uint32_t sampleRate = kSampleRate;
};

std::once_flag g_amyInitFlag;
bool g_amyInitialized = false;
std::array<EngineState, kSynthTrackCount> g_engines{};
std::array<std::int16_t, kAmyFramesPerBlock * kAmyChannelCount> g_amyBlock{};
std::size_t g_amyBlockCursor = kAmyFramesPerBlock;
std::deque<PendingEvent> g_pendingEvents;
std::mutex g_pendingEventMutex;

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

float Normalize(int value, int minimum, int maximum)
{
  if (maximum <= minimum)
  {
    return 0.0f;
  }
  return Clamp01(static_cast<float>(value - minimum) / static_cast<float>(maximum - minimum));
}

float MidiNoteToFrequency(int midiNote)
{
  return 440.0f * std::pow(2.0f, static_cast<float>(midiNote - 69) / 12.0f);
}

double AdvancePhase(double phase, float frequency)
{
  phase += static_cast<double>(frequency) * static_cast<double>(kTwoPi) / static_cast<double>(kSampleRate);
  if (phase >= kTwoPi)
  {
    phase -= kTwoPi * std::floor(phase / kTwoPi);
  }
  return phase;
}

std::size_t SamplerStartFrame(const SamplerParams& params)
{
  if (params.sampleFrames.empty())
  {
    return 0;
  }

  const std::size_t lastFrame = params.sampleFrames.size() - 1;
  return (std::min)(lastFrame,
                    static_cast<std::size_t>((static_cast<std::uint64_t>(lastFrame) * static_cast<std::uint64_t>(std::clamp(params.sampleStart, 0, 127))) /
                                             127u));
}

std::size_t SamplerEndFrame(const SamplerParams& params, std::size_t startFrame)
{
  if (params.sampleFrames.empty())
  {
    return 0;
  }

  const std::size_t frameCount = params.sampleFrames.size();
  const std::size_t endFrame = static_cast<std::size_t>((static_cast<std::uint64_t>(frameCount) * static_cast<std::uint64_t>(std::clamp(params.sampleEnd, 0, 127))) /
                                                        127u);
  return std::clamp(endFrame, startFrame + 1, frameCount);
}

float ReadSamplerFrame(const SamplerParams& params, double frameCursor, std::size_t endFrame)
{
  if (params.sampleFrames.empty())
  {
    return 0.0f;
  }

  const std::size_t clampedEnd = std::clamp<std::size_t>(endFrame, 1, params.sampleFrames.size());
  const std::size_t index0 = (std::min)(clampedEnd - 1, static_cast<std::size_t>((std::max)(0.0, std::floor(frameCursor))));
  const std::size_t index1 = (std::min)(clampedEnd - 1, index0 + 1);
  const float fraction = static_cast<float>(std::clamp(frameCursor - static_cast<double>(index0), 0.0, 1.0));
  const float sample0 = static_cast<float>(params.sampleFrames[index0]) / 32768.0f;
  const float sample1 = static_cast<float>(params.sampleFrames[index1]) / 32768.0f;
  return sample0 + (sample1 - sample0) * fraction;
}

float NextNoise(SamplerVoice& voice)
{
  voice.noiseState = voice.noiseState * 1664525u + 1013904223u;
  return static_cast<float>((voice.noiseState >> 8) & 0x00FFFFFFu) / 8388607.5f - 1.0f;
}

float OnePoleLowpass(float input, float cutoffHz, float& state)
{
  const float constrainedCutoff = std::clamp(cutoffHz, 20.0f, 16000.0f);
  const float alpha = std::clamp(1.0f - std::exp(-kTwoPi * constrainedCutoff / static_cast<float>(kSampleRate)), 0.001f, 0.99f);
  state += alpha * (input - state);
  return state;
}

bool UsesAmy(SynthEngineKind kind)
{
  return kind == SynthEngineKind::Dx7 || kind == SynthEngineKind::Juno;
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
    config.platform.multicore = 0;
    config.platform.multithread = 0;
    config.max_synths = 16;
    config.max_voices = 32;
    config.max_oscs = 180;
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

float JunoCutoffHz(int value)
{
  return 30.0f * std::pow(2.0f, Normalize(value, 0, 127) * 8.5f);
}

void ApplyJunoControlsNoLock(const EngineState& engine)
{
  if (!g_amyInitialized || engine.kind != SynthEngineKind::Juno)
  {
    return;
  }

  amy_event event = amy_default_event();
  event.synth = static_cast<uint16_t>(engine.synthId);
  event.osc = 0;
  event.filter_freq_coefs[amy_embed::COEF_CONST] = JunoCutoffHz(engine.junoCutoff);
  event.resonance = Normalize(engine.junoResonance, 0, 127);
  amy_add_event(&event);
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
  event.patch_number = static_cast<uint16_t>(engine.patchNumber < 0 ? DefaultPatchFor(engine.kind) : engine.patchNumber);
  event.num_voices = 6;
  amy_add_event(&event);

  if (engine.kind == SynthEngineKind::Juno)
  {
    ApplyJunoControlsNoLock(engine);
  }
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
  if (!voice.active)
  {
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

  if (!params.sampleFrames.empty())
  {
    const std::size_t startFrame = SamplerStartFrame(params);
    const std::size_t endFrame = SamplerEndFrame(params, startFrame);
    if (voice.sampleCursor < static_cast<double>(startFrame) || voice.sampleCursor >= static_cast<double>(endFrame))
    {
      voice.sampleCursor = static_cast<double>(startFrame);
    }

    const double sourceRate = static_cast<double>(params.sampleRate == 0 ? kSampleRate : params.sampleRate);
    const double pitchRatio = std::pow(2.0,
                                       static_cast<double>(static_cast<int>(voice.note) - params.sampleRoot + params.sampleTune) /
                                           12.0);
    const double step = (std::max)(0.0001, (sourceRate / static_cast<double>(kSampleRate)) * pitchRatio);
    float sample = ReadSamplerFrame(params, voice.sampleCursor, endFrame);
    sample = OnePoleLowpass(sample,
                            80.0f + std::pow(Normalize(params.cutoff, 0, 127), 2.2f) * 12000.0f,
                            voice.filterState);
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
    else if (voice.sampleCursor >= static_cast<double>(endFrame))
    {
      voice.releasing = true;
    }

    return sample;
  }

  const float frequency = MidiNoteToFrequency(static_cast<int>(voice.note) - params.sampleRoot + 60 + params.sampleTune);
  voice.phase = AdvancePhase(voice.phase, frequency);
  const float start = Normalize(params.sampleStart, 0, 127);
  const float end = (std::max)(start + 0.05f, Normalize(params.sampleEnd, 0, 127));
  const float samplePos = start + (end - start) * Clamp01(voice.samplePosition);
  const float tonal = std::sin(static_cast<float>(voice.phase)) * 0.55f;
  const float formant = std::sin(static_cast<float>(voice.phase * 2.37)) * 0.18f;
  const float transient = std::exp(-6.0f * samplePos) * NextNoise(voice) * 0.4f;
  float sample = tonal + formant + transient;
  sample = OnePoleLowpass(sample,
                          80.0f + std::pow(Normalize(params.cutoff, 0, 127), 2.2f) * 12000.0f,
                          voice.filterState);
  sample = std::tanh(sample * (1.0f + Normalize(params.drive, 0, 127) * 4.0f));
  sample *= voice.envelope * (0.2f + 0.8f * voice.velocity) * Normalize(params.sampleGain, 0, 127) * Normalize(params.level, 0, 127);

  voice.samplePosition += (std::max)(1.0f / static_cast<float>(kSampleRate), frequency / 12000.0f / static_cast<float>(kSampleRate));
  if (params.sampleLoop != 0)
  {
    if (voice.samplePosition >= 1.0f)
    {
      voice.samplePosition -= std::floor(voice.samplePosition);
    }
  }
  else if (voice.samplePosition >= 1.0f)
  {
    voice.releasing = true;
  }

  return sample;
}

void ResetEngineNoLock(EngineState& engine, std::size_t engineIndex, SynthEngineKind kind)
{
  engine = EngineState{};
  engine.kind = kind;
  engine.configured = kind != SynthEngineKind::None;
  engine.synthId = static_cast<int>(engineIndex) + 1;
  engine.patchNumber = DefaultPatchFor(kind);
}

void QueuePendingEvent(const PendingEvent& event)
{
  std::lock_guard<std::mutex> lock(g_pendingEventMutex);
  g_pendingEvents.push_back(event);
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
        engine.patchNumber = MapDx7Preset(event.text);
        ConfigureAmySynthNoLock(engine);
      }
      else if (engine.kind == SynthEngineKind::Juno)
      {
        engine.patchNumber = MapJunoPreset(event.text);
        ConfigureAmySynthNoLock(engine);
      }
      break;
    case PendingEventType::SetParameter:
      switch (engine.kind)
      {
        case SynthEngineKind::Dx7:
          if (event.text == "global.transpose")
          {
            engine.noteOffset = event.value;
          }
          break;
        case SynthEngineKind::Juno:
          if (event.text == "dco.range")
          {
            engine.noteOffset = event.value == 0 ? -12 : (event.value == 2 ? 12 : 0);
          }
          else if (event.text == "vcf.cutoff")
          {
            engine.junoCutoff = event.value;
            ApplyJunoControlsNoLock(engine);
          }
          else if (event.text == "vcf.resonance")
          {
            engine.junoResonance = event.value;
            ApplyJunoControlsNoLock(engine);
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
          else if (event.text == "filter.cutoff") engine.sampler.cutoff = event.value;
          else if (event.text == "filter.resonance") engine.sampler.resonance = event.value;
          else if (event.text == "fx.drive") engine.sampler.drive = event.value;
          else if (event.text == "fx.send") engine.sampler.send = event.value;
          else if (event.text == "amp.level") engine.sampler.level = event.value;
          else if (event.text == "amp.pan") engine.sampler.pan = event.value;
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
      engine.activeNotes.fill(false);
      break;
    case PendingEventType::NoteOn:
      if (!engine.configured || engine.kind == SynthEngineKind::None)
      {
        return;
      }

      if (UsesAmy(engine.kind))
      {
        EnsureAmyInitialized();
        if (engine.activeNotes[event.note])
        {
          SendAmyNoteEventNoLock(engine, event.note, 0.0f);
        }
        SendAmyNoteEventNoLock(engine, event.note, std::clamp(static_cast<float>(event.velocity) / 127.0f, 0.0f, 1.0f));
        engine.activeNotes[event.note] = true;
        return;
      }

      {
        ReleaseSamplerNoteNoLock(engine, event.note);
        SamplerVoice& voice = AllocateSamplerVoice(engine);
        voice.note = event.note;
        voice.velocity = std::clamp(static_cast<float>(event.velocity) / 127.0f, 0.0f, 1.0f);
        voice.sampleCursor = static_cast<double>(SamplerStartFrame(engine.sampler));
      }
      break;
    case PendingEventType::NoteOff:
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

// UI and MIDI handlers enqueue state changes here so they do not block behind a
// full AMY render while the audio task is generating the next block.
void DrainPendingEvents()
{
  for (;;)
  {
    PendingEvent event;
    {
      std::lock_guard<std::mutex> lock(g_pendingEventMutex);
      if (g_pendingEvents.empty())
      {
        return;
      }

      event = g_pendingEvents.front();
      g_pendingEvents.pop_front();
    }

    ApplyPendingEvent(event);
  }
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
  QueuePendingEvent(event);
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
  QueuePendingEvent(event);
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
  QueuePendingEvent(event);
}

void SetSamplerFrames(std::size_t engineIndex, const std::vector<std::int16_t>& frames, std::uint32_t sampleRate)
{
  if (engineIndex >= g_engines.size())
  {
    return;
  }

  PendingEvent event;
  event.type = PendingEventType::SetSamplerFrames;
  event.engineIndex = engineIndex;
  event.frames = frames;
  event.sampleRate = sampleRate;
  QueuePendingEvent(event);
}

void NoteOn(std::size_t engineIndex, std::uint8_t note, std::uint8_t velocity)
{
  if (engineIndex >= g_engines.size())
  {
    return;
  }

  PendingEvent event;
  event.type = PendingEventType::NoteOn;
  event.engineIndex = engineIndex;
  event.note = note;
  event.velocity = velocity;
  QueuePendingEvent(event);
}

void NoteOff(std::size_t engineIndex, std::uint8_t note)
{
  if (engineIndex >= g_engines.size())
  {
    return;
  }

  PendingEvent event;
  event.type = PendingEventType::NoteOff;
  event.engineIndex = engineIndex;
  event.note = note;
  QueuePendingEvent(event);
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
  QueuePendingEvent(event);
}

std::size_t RenderFrames(std::int16_t* destination, std::size_t frameCount)
{
  if (destination == nullptr)
  {
    return 0;
  }

  std::fill(destination, destination + frameCount, 0);
  DrainPendingEvents();

  bool hasAmyEngines = false;
  for (const EngineState& engine : g_engines)
  {
    if (engine.configured && UsesAmy(engine.kind))
    {
      hasAmyEngines = true;
      break;
    }
  }

  for (std::size_t frameIndex = 0; frameIndex < frameCount; ++frameIndex)
  {
    float mixedSample = 0.0f;

    if (hasAmyEngines)
    {
      if (g_amyBlockCursor >= kAmyFramesPerBlock)
      {
        EnsureAmyInitialized();
        const output_sample_type* block = amy_simple_fill_buffer();
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
        mixedSample += RenderSamplerVoice(engine.sampler, voice) * 0.45f;
      }
    }

    mixedSample = std::tanh(mixedSample * kOutputGain);
    destination[frameIndex] = static_cast<std::int16_t>(std::clamp(mixedSample, -1.0f, 1.0f) * 32767.0f);
  }

  return frameCount;
}

}  // namespace synth::audio
