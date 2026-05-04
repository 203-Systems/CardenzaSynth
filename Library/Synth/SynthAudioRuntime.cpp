#include "Library/Synth/SynthAudioRuntime.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <mutex>

namespace synth::audio {

namespace {

constexpr float kTwoPi = 6.28318530717958647692f;
constexpr std::size_t kVoiceCountPerEngine = 4;
constexpr float kMasterGain = 0.18f;
constexpr float kSamplerReferenceFrequency = 261.625565f;

enum class EnvelopePhase {
  Idle,
  Attack,
  Decay,
  Sustain,
  Release,
};

struct Dx7Params {
  int algorithm = 5;
  int feedback = 2;
  int lfoSpeed = 40;
  int lfoDelay = 20;
  int lfoPitch = 0;
  int lfoAmp = 20;
  int transpose = 0;
  std::array<int, 6> output{{90, 75, 75, 75, 75, 75}};
  std::array<int, 6> coarse{{1, 2, 2, 2, 2, 2}};
  std::array<int, 6> fine{{0, 0, 0, 0, 0, 0}};
  std::array<int, 6> detune{{0, 0, 0, 0, 0, 0}};
  std::array<int, 6> attack{{80, 80, 80, 80, 80, 80}};
  std::array<int, 6> decay{{55, 55, 55, 55, 55, 55}};
  std::array<int, 6> sustain{{65, 65, 65, 65, 65, 65}};
  std::array<int, 6> release{{40, 40, 40, 40, 40, 40}};
};

struct JunoParams {
  int range = 1;
  int pwm = 48;
  int sub = 68;
  int noise = 0;
  int saw = 1;
  int cutoff = 92;
  int resonance = 18;
  int env = 54;
  int lfo = 18;
  int level = 96;
  int attack = 28;
  int decay = 42;
  int sustain = 78;
  int release = 36;
  int lfoRate = 40;
  int lfoDelay = 22;
  int chorusMode = 0;
  int hpfMode = 0;
};

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

struct EnvelopeShape {
  float attackSeconds = 0.01f;
  float decaySeconds = 0.15f;
  float sustainLevel = 0.75f;
  float releaseSeconds = 0.3f;
};

struct VoiceState {
  bool active = false;
  std::uint8_t note = 0;
  float velocity = 0.0f;
  EnvelopePhase envelopePhase = EnvelopePhase::Idle;
  float envelopeLevel = 0.0f;
  float releaseStartLevel = 0.0f;
  double carrierPhase = 0.0;
  double modPhase = 0.0;
  double auxPhase = 0.0;
  double lfoPhase = 0.0;
  float filterState1 = 0.0f;
  float filterState2 = 0.0f;
  float feedbackSample = 0.0f;
  float samplePosition = 0.0f;
  std::uint32_t noiseState = 0x12345678u;
  std::uint64_t serial = 0;
};

struct EngineState {
  SynthEngineKind kind = SynthEngineKind::None;
  bool configured = false;
  Dx7Params dx7{};
  JunoParams juno{};
  SamplerParams sampler{};
  std::array<VoiceState, kVoiceCountPerEngine> voices{};
  std::uint64_t nextVoiceSerial = 1;
};

std::array<EngineState, kSynthTrackCount> g_engines{};
std::mutex g_engineMutex;

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

float SawFromPhase(double phase)
{
  return static_cast<float>(phase / kTwoPi) * 2.0f - 1.0f;
}

float PhaseFraction(double phase)
{
  return static_cast<float>(phase / kTwoPi);
}

float NextNoise(VoiceState& voice)
{
  voice.noiseState = voice.noiseState * 1664525u + 1013904223u;
  return static_cast<float>((voice.noiseState >> 8) & 0x00FFFFFFu) / 8388607.5f - 1.0f;
}

float ExponentialTime(float normalized, float minimumSeconds, float maximumSeconds)
{
  const float clamped = Clamp01(normalized);
  return minimumSeconds * std::pow(maximumSeconds / minimumSeconds, clamped);
}

EnvelopeShape Dx7EnvelopeFor(const Dx7Params& params)
{
  const float attackAverage = (params.attack[0] + params.attack[1]) * 0.5f;
  const float decayAverage = (params.decay[0] + params.decay[1]) * 0.5f;
  const float sustainAverage = (params.sustain[0] + params.sustain[1]) * 0.5f;
  const float releaseAverage = (params.release[0] + params.release[1]) * 0.5f;
  return {
      ExponentialTime(1.0f - Normalize(static_cast<int>(attackAverage), 0, 99), 0.0025f, 1.4f),
      ExponentialTime(1.0f - Normalize(static_cast<int>(decayAverage), 0, 99), 0.015f, 3.2f),
      Normalize(static_cast<int>(sustainAverage), 0, 99),
      ExponentialTime(1.0f - Normalize(static_cast<int>(releaseAverage), 0, 99), 0.02f, 3.5f),
  };
}

EnvelopeShape JunoEnvelopeFor(const JunoParams& params)
{
  return {
      ExponentialTime(Normalize(params.attack, 0, 127), 0.0025f, 3.4f),
      ExponentialTime(Normalize(params.decay, 0, 127), 0.02f, 3.8f),
      Normalize(params.sustain, 0, 127),
      ExponentialTime(Normalize(params.release, 0, 127), 0.02f, 4.0f),
  };
}

EnvelopeShape SamplerEnvelopeFor(const SamplerParams& params)
{
  return {
      ExponentialTime(Normalize(params.attack, 0, 127), 0.0015f, 1.5f),
      0.08f,
      1.0f,
      ExponentialTime(Normalize(params.release, 0, 127), 0.01f, 2.2f),
  };
}

float AdvanceEnvelope(VoiceState& voice, const EnvelopeShape& shape)
{
  switch (voice.envelopePhase)
  {
    case EnvelopePhase::Idle:
      voice.envelopeLevel = 0.0f;
      voice.active = false;
      break;
    case EnvelopePhase::Attack: {
      const float step = 1.0f / std::max(1.0f, shape.attackSeconds * static_cast<float>(kSampleRate));
      voice.envelopeLevel += step;
      if (voice.envelopeLevel >= 1.0f)
      {
        voice.envelopeLevel = 1.0f;
        voice.envelopePhase = EnvelopePhase::Decay;
      }
      break;
    }
    case EnvelopePhase::Decay: {
      if (voice.envelopeLevel <= shape.sustainLevel)
      {
        voice.envelopeLevel = shape.sustainLevel;
        voice.envelopePhase = EnvelopePhase::Sustain;
        break;
      }

      const float step = (1.0f - shape.sustainLevel) / std::max(1.0f, shape.decaySeconds * static_cast<float>(kSampleRate));
      voice.envelopeLevel -= step;
      if (voice.envelopeLevel <= shape.sustainLevel)
      {
        voice.envelopeLevel = shape.sustainLevel;
        voice.envelopePhase = EnvelopePhase::Sustain;
      }
      break;
    }
    case EnvelopePhase::Sustain:
      voice.envelopeLevel = shape.sustainLevel;
      break;
    case EnvelopePhase::Release: {
      const float step = std::max(voice.releaseStartLevel / std::max(1.0f, shape.releaseSeconds * static_cast<float>(kSampleRate)), 0.00002f);
      voice.envelopeLevel -= step;
      if (voice.envelopeLevel <= 0.0001f)
      {
        voice.envelopeLevel = 0.0f;
        voice.envelopePhase = EnvelopePhase::Idle;
        voice.active = false;
      }
      break;
    }
  }

  return voice.envelopeLevel;
}

float OnePoleLowpass(float input, float cutoffHz, float& state)
{
  const float constrainedCutoff = std::clamp(cutoffHz, 20.0f, 18000.0f);
  const float alpha = std::clamp(1.0f - std::exp(-kTwoPi * constrainedCutoff / static_cast<float>(kSampleRate)), 0.001f, 0.99f);
  state += alpha * (input - state);
  return state;
}

void ReleaseVoice(VoiceState& voice)
{
  if (!voice.active || voice.envelopePhase == EnvelopePhase::Release || voice.envelopePhase == EnvelopePhase::Idle)
  {
    return;
  }

  voice.releaseStartLevel = std::max(voice.envelopeLevel, 0.0005f);
  voice.envelopePhase = EnvelopePhase::Release;
}

VoiceState& AllocateVoice(EngineState& engine)
{
  auto iterator = std::find_if(engine.voices.begin(), engine.voices.end(), [](const VoiceState& voice) {
    return !voice.active;
  });

  if (iterator == engine.voices.end())
  {
    iterator = std::min_element(engine.voices.begin(), engine.voices.end(), [](const VoiceState& left, const VoiceState& right) {
      return left.serial < right.serial;
    });
  }

  *iterator = VoiceState{};
  iterator->active = true;
  iterator->serial = engine.nextVoiceSerial++;
  return *iterator;
}

void ResetEngine(EngineState& engine, SynthEngineKind kind)
{
  engine = EngineState{};
  engine.kind = kind;
  engine.configured = kind != SynthEngineKind::None;
}

float Dx7OperatorRatio(int coarse, int fine, int detune)
{
  const float coarseRatio = coarse <= 0 ? 0.5f : static_cast<float>(coarse);
  const float fineRatio = Normalize(fine, 0, 99);
  const float detuneRatio = static_cast<float>(detune) / 70.0f;
  return std::max(0.25f, coarseRatio + fineRatio + detuneRatio);
}

float RenderDx7Voice(EngineState& engine, VoiceState& voice)
{
  const EnvelopeShape envelope = Dx7EnvelopeFor(engine.dx7);
  const float envelopeLevel = AdvanceEnvelope(voice, envelope);
  if (!voice.active)
  {
    return 0.0f;
  }

  const float lfoRateHz = 0.15f + Normalize(engine.dx7.lfoSpeed, 0, 99) * 8.0f;
  voice.lfoPhase = AdvancePhase(voice.lfoPhase, lfoRateHz);
  const float lfoValue = std::sin(voice.lfoPhase);
  const float pitchLfoDepth = Normalize(engine.dx7.lfoPitch, 0, 99) * 0.03f;
  const float ampLfoDepth = Normalize(engine.dx7.lfoAmp, 0, 99) * 0.15f;

  const float baseFrequency = MidiNoteToFrequency(static_cast<int>(voice.note) + engine.dx7.transpose) * (1.0f + lfoValue * pitchLfoDepth);
  const float carrierRatio = Dx7OperatorRatio(engine.dx7.coarse[0], engine.dx7.fine[0], engine.dx7.detune[0]);
  const float modRatio = Dx7OperatorRatio(engine.dx7.coarse[1], engine.dx7.fine[1], engine.dx7.detune[1]);
  const float auxRatio = Dx7OperatorRatio(engine.dx7.coarse[2], engine.dx7.fine[2], engine.dx7.detune[2]);

  voice.modPhase = AdvancePhase(voice.modPhase, baseFrequency * modRatio);
  voice.carrierPhase = AdvancePhase(voice.carrierPhase, baseFrequency * carrierRatio);
  voice.auxPhase = AdvancePhase(voice.auxPhase, baseFrequency * auxRatio);

  const float feedbackDepth = Normalize(engine.dx7.feedback, 0, 7) * 2.4f;
  const float feedback = voice.feedbackSample * feedbackDepth;
  const float modulator = std::sin(voice.modPhase + feedback);
  const float algorithmColor = Normalize(engine.dx7.algorithm, 1, 32);
  const float modIndex = 0.4f + Normalize(engine.dx7.output[1], 0, 99) * 8.0f + algorithmColor * 3.0f;
  const float carrier = std::sin(voice.carrierPhase + modulator * modIndex);
  const float shimmer = std::sin(voice.auxPhase + modulator * (0.3f + Normalize(engine.dx7.output[5], 0, 99) * 2.0f));
  const float carrierLevel = 0.15f + Normalize(engine.dx7.output[0], 0, 99) * 0.85f;
  const float shimmerLevel = 0.05f + Normalize(engine.dx7.output[2], 0, 99) * 0.25f;

  float sample = carrier * carrierLevel + shimmer * shimmerLevel;
  sample *= envelopeLevel * (0.25f + 0.75f * voice.velocity) * (1.0f + lfoValue * ampLfoDepth);
  voice.feedbackSample = carrier;
  return sample * 0.55f;
}

int JunoRangeSemitoneOffset(int range)
{
  switch (range)
  {
    case 0:
      return -12;
    case 2:
      return 12;
    default:
      return 0;
  }
}

float RenderJunoVoice(EngineState& engine, VoiceState& voice)
{
  const EnvelopeShape envelope = JunoEnvelopeFor(engine.juno);
  const float envelopeLevel = AdvanceEnvelope(voice, envelope);
  if (!voice.active)
  {
    return 0.0f;
  }

  const float baseFrequency = MidiNoteToFrequency(static_cast<int>(voice.note) + JunoRangeSemitoneOffset(engine.juno.range));
  const float lfoRateHz = 0.15f + Normalize(engine.juno.lfoRate, 0, 127) * 7.0f;
  voice.lfoPhase = AdvancePhase(voice.lfoPhase, lfoRateHz);
  const float lfoValue = std::sin(voice.lfoPhase);
  const float detuneRatio = 1.0f + 0.0025f * static_cast<float>(engine.juno.chorusMode);

  voice.carrierPhase = AdvancePhase(voice.carrierPhase, baseFrequency);
  voice.modPhase = AdvancePhase(voice.modPhase, baseFrequency * 0.5f);
  voice.auxPhase = AdvancePhase(voice.auxPhase, baseFrequency * detuneRatio);

  const float dutyCycle = std::clamp(0.08f + Normalize(engine.juno.pwm, 0, 127) * 0.84f + lfoValue * 0.08f, 0.05f, 0.95f);
  const float saw = engine.juno.saw != 0 ? SawFromPhase(voice.carrierPhase) : 0.0f;
  const float pulse = PhaseFraction(voice.carrierPhase) < dutyCycle ? 1.0f : -1.0f;
  const float sub = PhaseFraction(voice.modPhase) < 0.5f ? 1.0f : -1.0f;
  const float chorus = engine.juno.chorusMode > 0 ? SawFromPhase(voice.auxPhase) * 0.35f * static_cast<float>(engine.juno.chorusMode) : 0.0f;
  const float noise = NextNoise(voice) * Normalize(engine.juno.noise, 0, 127);

  float raw = saw + pulse * 0.35f + sub * Normalize(engine.juno.sub, 0, 127) * 0.75f + noise * 0.45f + chorus;
  raw *= 0.35f + 0.65f * voice.velocity;

  const float cutoffHz = 70.0f + std::pow(Normalize(engine.juno.cutoff, 0, 127), 2.2f) * 12000.0f +
                         envelopeLevel * Normalize(engine.juno.env, 0, 127) * 4800.0f +
                         (0.5f + 0.5f * lfoValue) * Normalize(engine.juno.lfo, 0, 127) * 1200.0f;
  float filtered = OnePoleLowpass(raw, cutoffHz, voice.filterState1);

  if (engine.juno.hpfMode > 0)
  {
    const float lowBand = OnePoleLowpass(filtered, 25.0f + static_cast<float>(engine.juno.hpfMode) * 90.0f, voice.filterState2);
    filtered -= lowBand * (0.3f * static_cast<float>(engine.juno.hpfMode));
  }

  filtered = std::tanh(filtered * (1.0f + Normalize(engine.juno.resonance, 0, 127) * 0.7f));
  filtered *= envelopeLevel * Normalize(engine.juno.level, 0, 127);
  return filtered * 0.5f;
}

float ProceduralSampleValue(float position, VoiceState& voice)
{
  const float t = Clamp01(position);
  const float body = std::sin(kTwoPi * (2.0f * t + 0.1f * std::sin(kTwoPi * 6.0f * t)));
  const float formant = 0.45f * std::sin(kTwoPi * 5.1f * t) + 0.20f * std::sin(kTwoPi * 11.3f * t);
  const float transient = std::exp(-6.0f * t) * (0.8f * std::sin(kTwoPi * (18.0f * t + 2.0f * t * t)) + 0.2f * NextNoise(voice));
  const float noise = NextNoise(voice) * (0.25f + 0.75f * (1.0f - t));
  return body * 0.5f + formant * 0.25f + transient * 0.20f + noise * 0.10f;
}

float RenderSamplerVoice(EngineState& engine, VoiceState& voice)
{
  const EnvelopeShape envelope = SamplerEnvelopeFor(engine.sampler);
  const float envelopeLevel = AdvanceEnvelope(voice, envelope);
  if (!voice.active)
  {
    return 0.0f;
  }

  const float pitchedFrequency = MidiNoteToFrequency(static_cast<int>(voice.note) + engine.sampler.sampleTune);
  const float playbackRate = pitchedFrequency / kSamplerReferenceFrequency;
  const float start = Normalize(engine.sampler.sampleStart, 0, 127);
  const float end = std::max(start + 0.02f, Normalize(engine.sampler.sampleEnd, 0, 127));
  const float span = std::max(0.02f, end - start);
  const float samplePosition = start + span * Clamp01(voice.samplePosition);

  voice.carrierPhase = AdvancePhase(voice.carrierPhase, pitchedFrequency);
  float sample = ProceduralSampleValue(samplePosition, voice) * 0.75f + std::sin(voice.carrierPhase) * 0.25f;
  sample = OnePoleLowpass(sample, 80.0f + std::pow(Normalize(engine.sampler.cutoff, 0, 127), 2.1f) * 14000.0f, voice.filterState1);
  sample = std::tanh(sample * (1.0f + Normalize(engine.sampler.drive, 0, 127) * 5.0f));
  sample *= envelopeLevel * Normalize(engine.sampler.level, 0, 127) * (0.15f + 0.85f * voice.velocity);

  const float increment = std::max(1.0f / static_cast<float>(kSampleRate), playbackRate / static_cast<float>(kSampleRate));
  if (engine.sampler.sampleLoop != 0)
  {
    voice.samplePosition += increment * 4.0f;
    if (voice.samplePosition >= 1.0f)
    {
      voice.samplePosition -= std::floor(voice.samplePosition);
    }
  }
  else
  {
    voice.samplePosition += increment * (1.0f + span * 2.0f);
    if (voice.samplePosition >= 1.0f)
    {
      voice.samplePosition = 1.0f;
      ReleaseVoice(voice);
    }
  }

  return sample * 0.75f;
}

float RenderVoice(EngineState& engine, VoiceState& voice)
{
  switch (engine.kind)
  {
    case SynthEngineKind::Dx7:
      return RenderDx7Voice(engine, voice);
    case SynthEngineKind::Juno:
      return RenderJunoVoice(engine, voice);
    case SynthEngineKind::Sampler:
      return RenderSamplerVoice(engine, voice);
    case SynthEngineKind::None:
    default:
      return 0.0f;
  }
}

void ApplyDx7Parameter(Dx7Params& params, std::string_view parameterId, int value)
{
  if (parameterId == "global.algorithm")
  {
    params.algorithm = value;
    return;
  }
  if (parameterId == "global.feedback")
  {
    params.feedback = value;
    return;
  }
  if (parameterId == "global.lfo_speed")
  {
    params.lfoSpeed = value;
    return;
  }
  if (parameterId == "global.lfo_delay")
  {
    params.lfoDelay = value;
    return;
  }
  if (parameterId == "global.lfo_pitch")
  {
    params.lfoPitch = value;
    return;
  }
  if (parameterId == "global.lfo_amp")
  {
    params.lfoAmp = value;
    return;
  }
  if (parameterId == "global.transpose")
  {
    params.transpose = value;
    return;
  }

  if (parameterId.size() < 5 || parameterId[0] != 'o' || parameterId[1] != 'p' || parameterId[3] != '.')
  {
    return;
  }

  const int operatorIndex = parameterId[2] - '1';
  if (operatorIndex < 0 || operatorIndex >= static_cast<int>(params.output.size()))
  {
    return;
  }

  const std::string_view suffix = parameterId.substr(4);
  if (suffix == "output")
  {
    params.output[operatorIndex] = value;
  }
  else if (suffix == "coarse")
  {
    params.coarse[operatorIndex] = value;
  }
  else if (suffix == "fine")
  {
    params.fine[operatorIndex] = value;
  }
  else if (suffix == "detune")
  {
    params.detune[operatorIndex] = value;
  }
  else if (suffix == "attack")
  {
    params.attack[operatorIndex] = value;
  }
  else if (suffix == "decay")
  {
    params.decay[operatorIndex] = value;
  }
  else if (suffix == "sustain")
  {
    params.sustain[operatorIndex] = value;
  }
  else if (suffix == "release")
  {
    params.release[operatorIndex] = value;
  }
}

void ApplyJunoParameter(JunoParams& params, std::string_view parameterId, int value)
{
  if (parameterId == "dco.range")
  {
    params.range = value;
  }
  else if (parameterId == "dco.pwm")
  {
    params.pwm = value;
  }
  else if (parameterId == "dco.sub")
  {
    params.sub = value;
  }
  else if (parameterId == "dco.noise")
  {
    params.noise = value;
  }
  else if (parameterId == "dco.saw")
  {
    params.saw = value;
  }
  else if (parameterId == "vcf.cutoff")
  {
    params.cutoff = value;
  }
  else if (parameterId == "vcf.resonance")
  {
    params.resonance = value;
  }
  else if (parameterId == "vcf.env")
  {
    params.env = value;
  }
  else if (parameterId == "vcf.lfo")
  {
    params.lfo = value;
  }
  else if (parameterId == "vca.level")
  {
    params.level = value;
  }
  else if (parameterId == "env.attack")
  {
    params.attack = value;
  }
  else if (parameterId == "env.decay")
  {
    params.decay = value;
  }
  else if (parameterId == "env.sustain")
  {
    params.sustain = value;
  }
  else if (parameterId == "env.release")
  {
    params.release = value;
  }
  else if (parameterId == "lfo.rate")
  {
    params.lfoRate = value;
  }
  else if (parameterId == "lfo.delay")
  {
    params.lfoDelay = value;
  }
  else if (parameterId == "chorus.mode")
  {
    params.chorusMode = value;
  }
  else if (parameterId == "hpf.mode")
  {
    params.hpfMode = value;
  }
}

void ApplySamplerParameter(SamplerParams& params, std::string_view parameterId, int value)
{
  if (parameterId == "sample.start")
  {
    params.sampleStart = value;
  }
  else if (parameterId == "sample.end")
  {
    params.sampleEnd = value;
  }
  else if (parameterId == "sample.loop")
  {
    params.sampleLoop = value;
  }
  else if (parameterId == "sample.tune")
  {
    params.sampleTune = value;
  }
  else if (parameterId == "env.attack")
  {
    params.attack = value;
  }
  else if (parameterId == "env.release")
  {
    params.release = value;
  }
  else if (parameterId == "filter.cutoff")
  {
    params.cutoff = value;
  }
  else if (parameterId == "filter.resonance")
  {
    params.resonance = value;
  }
  else if (parameterId == "fx.drive")
  {
    params.drive = value;
  }
  else if (parameterId == "fx.send")
  {
    params.send = value;
  }
  else if (parameterId == "amp.level")
  {
    params.level = value;
  }
  else if (parameterId == "amp.pan")
  {
    params.pan = value;
  }
}

}  // namespace

void EnsureEngine(std::size_t engineIndex, SynthEngineKind kind)
{
  if (engineIndex >= g_engines.size())
  {
    return;
  }

  std::lock_guard<std::mutex> lock(g_engineMutex);
  EngineState& engine = g_engines[engineIndex];
  if (!engine.configured || engine.kind != kind)
  {
    ResetEngine(engine, kind);
  }
}

void SetEngineParameter(std::size_t engineIndex, std::string_view parameterId, int value)
{
  if (engineIndex >= g_engines.size())
  {
    return;
  }

  std::lock_guard<std::mutex> lock(g_engineMutex);
  EngineState& engine = g_engines[engineIndex];
  switch (engine.kind)
  {
    case SynthEngineKind::Dx7:
      ApplyDx7Parameter(engine.dx7, parameterId, value);
      break;
    case SynthEngineKind::Juno:
      ApplyJunoParameter(engine.juno, parameterId, value);
      break;
    case SynthEngineKind::Sampler:
      ApplySamplerParameter(engine.sampler, parameterId, value);
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

  std::lock_guard<std::mutex> lock(g_engineMutex);
  EngineState& engine = g_engines[engineIndex];
  if (!engine.configured || engine.kind == SynthEngineKind::None)
  {
    return;
  }

  VoiceState& voice = AllocateVoice(engine);
  voice.note = note;
  voice.velocity = std::clamp(static_cast<float>(velocity) / 127.0f, 0.0f, 1.0f);
  voice.envelopePhase = EnvelopePhase::Attack;
  voice.envelopeLevel = 0.0f;
  voice.releaseStartLevel = 0.0f;
  voice.samplePosition = 0.0f;
  voice.noiseState ^= (static_cast<std::uint32_t>(note) << 8) | velocity;
}

void NoteOff(std::size_t engineIndex, std::uint8_t note)
{
  if (engineIndex >= g_engines.size())
  {
    return;
  }

  std::lock_guard<std::mutex> lock(g_engineMutex);
  for (VoiceState& voice : g_engines[engineIndex].voices)
  {
    if (voice.active && voice.note == note)
    {
      ReleaseVoice(voice);
    }
  }
}

void AllNotesOff(std::size_t engineIndex)
{
  if (engineIndex >= g_engines.size())
  {
    return;
  }

  std::lock_guard<std::mutex> lock(g_engineMutex);
  for (VoiceState& voice : g_engines[engineIndex].voices)
  {
    ReleaseVoice(voice);
  }
}

std::size_t RenderFrames(std::int16_t* destination, std::size_t frameCount)
{
  if (destination == nullptr)
  {
    return 0;
  }

  std::fill(destination, destination + frameCount, 0);
  std::lock_guard<std::mutex> lock(g_engineMutex);
  for (std::size_t frameIndex = 0; frameIndex < frameCount; ++frameIndex)
  {
    float mixedSample = 0.0f;
    std::size_t activeVoiceCount = 0;

    for (EngineState& engine : g_engines)
    {
      if (!engine.configured || engine.kind == SynthEngineKind::None)
      {
        continue;
      }

      for (VoiceState& voice : engine.voices)
      {
        if (!voice.active)
        {
          continue;
        }

        mixedSample += RenderVoice(engine, voice);
        ++activeVoiceCount;
      }
    }

    if (activeVoiceCount == 0)
    {
      continue;
    }

    mixedSample = std::tanh(mixedSample * kMasterGain);
    destination[frameIndex] = static_cast<std::int16_t>(std::clamp(mixedSample, -1.0f, 1.0f) * 32767.0f);
  }

  return frameCount;
}

}  // namespace synth::audio