#include "Library/Synth/SynthAudioRuntime.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string_view>

#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if __has_include(<unistd.h>)
#include <unistd.h>
#endif

#if __has_include(<pthread.h>)
#include <pthread.h>
#endif

#if __has_include(<esp_heap_caps.h>)
#include <esp_heap_caps.h>
#endif

#if __has_include(<driver/uart.h>)
#include <driver/uart.h>
#endif

#if __has_include(<soc/uart_reg.h>)
#include <soc/uart_reg.h>
#endif

#if __has_include(<esp_task.h>)
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
  int attack = 4;
  int release = 18;
  int cutoff = 96;
  int resonance = 18;
  int drive = 0;
  int send = 24;
  int level = 96;
  int pan = 64;
};

struct SamplerVoice {
  bool active = false;
  bool releasing = false;
  std::uint8_t note = 0;
  float velocity = 0.0f;
  float envelope = 0.0f;
  float samplePosition = 0.0f;
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

std::once_flag g_amyInitFlag;
bool g_amyInitialized = false;
std::array<EngineState, kSynthTrackCount> g_engines{};
std::array<std::int16_t, kAmyFramesPerBlock * kAmyChannelCount> g_amyBlock{};
std::size_t g_amyBlockCursor = kAmyFramesPerBlock;
std::mutex g_mutex;

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

  const float frequency = MidiNoteToFrequency(static_cast<int>(voice.note) + params.sampleTune);
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
  sample *= voice.envelope * (0.2f + 0.8f * voice.velocity) * Normalize(params.level, 0, 127);

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

}  // namespace

void EnsureEngine(std::size_t engineIndex, SynthEngineKind kind)
{
  if (engineIndex >= g_engines.size())
  {
    return;
  }

  std::lock_guard<std::mutex> lock(g_mutex);
  EngineState& engine = g_engines[engineIndex];
  if (!engine.configured || engine.kind != kind)
  {
    ResetEngineNoLock(engine, engineIndex, kind);
    if (UsesAmy(kind))
    {
      ConfigureAmySynthNoLock(engine);
    }
  }
}

void SetEnginePreset(std::size_t engineIndex, std::string_view presetId)
{
  if (engineIndex >= g_engines.size())
  {
    return;
  }

  std::lock_guard<std::mutex> lock(g_mutex);
  EngineState& engine = g_engines[engineIndex];
  if (!engine.configured)
  {
    return;
  }

  if (engine.kind == SynthEngineKind::Dx7)
  {
    engine.patchNumber = MapDx7Preset(presetId);
    ConfigureAmySynthNoLock(engine);
  }
  else if (engine.kind == SynthEngineKind::Juno)
  {
    engine.patchNumber = MapJunoPreset(presetId);
    ConfigureAmySynthNoLock(engine);
  }
}

void SetEngineParameter(std::size_t engineIndex, std::string_view parameterId, int value)
{
  if (engineIndex >= g_engines.size())
  {
    return;
  }

  std::lock_guard<std::mutex> lock(g_mutex);
  EngineState& engine = g_engines[engineIndex];
  switch (engine.kind)
  {
    case SynthEngineKind::Dx7:
      if (parameterId == "global.transpose")
      {
        engine.noteOffset = value;
      }
      break;
    case SynthEngineKind::Juno:
      if (parameterId == "dco.range")
      {
        engine.noteOffset = value == 0 ? -12 : (value == 2 ? 12 : 0);
      }
      else if (parameterId == "vcf.cutoff")
      {
        engine.junoCutoff = value;
        ApplyJunoControlsNoLock(engine);
      }
      else if (parameterId == "vcf.resonance")
      {
        engine.junoResonance = value;
        ApplyJunoControlsNoLock(engine);
      }
      break;
    case SynthEngineKind::Sampler:
      if (parameterId == "sample.start") engine.sampler.sampleStart = value;
      else if (parameterId == "sample.end") engine.sampler.sampleEnd = value;
      else if (parameterId == "sample.loop") engine.sampler.sampleLoop = value;
      else if (parameterId == "sample.tune") engine.sampler.sampleTune = value;
      else if (parameterId == "env.attack") engine.sampler.attack = value;
      else if (parameterId == "env.release") engine.sampler.release = value;
      else if (parameterId == "filter.cutoff") engine.sampler.cutoff = value;
      else if (parameterId == "filter.resonance") engine.sampler.resonance = value;
      else if (parameterId == "fx.drive") engine.sampler.drive = value;
      else if (parameterId == "fx.send") engine.sampler.send = value;
      else if (parameterId == "amp.level") engine.sampler.level = value;
      else if (parameterId == "amp.pan") engine.sampler.pan = value;
      break;
    case SynthEngineKind::None:
    default:
      break;
  }
}

void NoteOn(std::size_t engineIndex, std::uint8_t note, std::uint8_t velocity)
{
  if (engineIndex >= g_engines.size())
  {
    return;
  }

  std::lock_guard<std::mutex> lock(g_mutex);
  EngineState& engine = g_engines[engineIndex];
  if (!engine.configured || engine.kind == SynthEngineKind::None)
  {
    return;
  }

  if (UsesAmy(engine.kind))
  {
    EnsureAmyInitialized();
    SendAmyNoteEventNoLock(engine, note, std::clamp(static_cast<float>(velocity) / 127.0f, 0.0f, 1.0f));
    engine.activeNotes[note] = true;
    return;
  }

  SamplerVoice& voice = AllocateSamplerVoice(engine);
  voice.note = note;
  voice.velocity = std::clamp(static_cast<float>(velocity) / 127.0f, 0.0f, 1.0f);
}

void NoteOff(std::size_t engineIndex, std::uint8_t note)
{
  if (engineIndex >= g_engines.size())
  {
    return;
  }

  std::lock_guard<std::mutex> lock(g_mutex);
  EngineState& engine = g_engines[engineIndex];
  if (UsesAmy(engine.kind))
  {
    SendAmyNoteEventNoLock(engine, note, 0.0f);
    engine.activeNotes[note] = false;
    return;
  }

  for (SamplerVoice& voice : engine.samplerVoices)
  {
    if (voice.active && voice.note == note)
    {
      voice.releasing = true;
    }
  }
}

void AllNotesOff(std::size_t engineIndex)
{
  if (engineIndex >= g_engines.size())
  {
    return;
  }

  std::lock_guard<std::mutex> lock(g_mutex);
  EngineState& engine = g_engines[engineIndex];
  if (UsesAmy(engine.kind))
  {
    ReleaseAllAmyNotesNoLock(engine);
    return;
  }

  for (SamplerVoice& voice : engine.samplerVoices)
  {
    voice.releasing = true;
  }
}

std::size_t RenderFrames(std::int16_t* destination, std::size_t frameCount)
{
  if (destination == nullptr)
  {
    return 0;
  }

  std::fill(destination, destination + frameCount, 0);
  std::lock_guard<std::mutex> lock(g_mutex);

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