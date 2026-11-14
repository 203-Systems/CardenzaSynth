#pragma once

// The Wave engine: two oscillators (AMY's wavetables scanned by POSITION, or the classic shapes),
// unison on the first, one filter, two envelopes and an LFO. Its parameters and factory presets, for
// the engine (WaveSynthEngine) and for the audio runtime that turns them into AMY's oscillators
// (SynthAudioRuntimeAmy.cpp: ApplyWaveGroupNoLock), which reads a preset's values here, not from the
// engine: SetEnginePreset names the preset, and only the values edited since come as parameters.

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string_view>

#include "Library/Synth/Core/ISynthEngine.h"

namespace synth::wave {

// An oscillator's wave. The first five are AMY's wavetables (64 cycles each, POSITION crosses them);
// PULSE takes POSITION as its width, from square to thin. Oscillator 2's parameter is one more: 0 is
// OFF.
enum Wave : int { kTable111, kTableBraids, kTablePpg, kTableSineSaw, kTableViral, kSaw, kPulse, kTriangle, kSine, kWaveCount };
inline constexpr int kWavetableCount = 5;
inline constexpr std::array<const char*, kWaveCount> kWaveNames{{"111", "BRAIDS", "PPG", "SIN-SAW", "VIRAL", "SAW", "PULSE", "TRI", "SINE"}};
inline constexpr std::array<const char*, 4> kFilterNames{{"LP24", "LP12", "BP", "HP"}};
inline constexpr std::array<const char*, 4> kLfoNames{{"TRI", "SINE", "SAW", "SQR"}};
inline constexpr int kMaxUnison = 7;

enum Param : std::uint8_t {
  kOsc1Wave, kOsc1Position, kOsc1Octave, kOsc1Unison, kOsc1Spread, kPositionEnv, kPositionLfo,
  kOsc2Wave, kOsc2Position, kOsc2Tune, kOsc2Fine, kOscMix,
  kFilterType, kFilterCutoff, kFilterResonance, kFilterEnv, kFilterLfo, kFilterKeyboard,
  kEnvAttack, kEnvDecay, kEnvSustain, kEnvRelease, kAmpLevel,
  kEnv2Attack, kEnv2Decay, kEnv2Sustain, kEnv2Release,
  kLfoRate, kLfoDelay, kLfoWave, kLfoPitch,
  kParamCount
};

// IDs, labels and groups stay within a std::string's own 15 characters: an engine keeps them all in RAM.
struct ParamInfo {
  const char* id;
  const char* label;
  const char* group;
  SynthParameterKind kind;
  std::int8_t minimum;
  std::int8_t maximum;
  std::int8_t defaultValue;
};

inline constexpr std::array<ParamInfo, kParamCount> kParams{{
    {"osc1.wave", "Wave", "OSC1", SynthParameterKind::Choice, 0, kWaveCount - 1, kSaw},
    {"osc1.position", "Position", "OSC1", SynthParameterKind::Continuous, 0, 127, 0},
    {"osc1.octave", "Octave", "OSC1", SynthParameterKind::Continuous, -2, 2, 0},
    {"osc1.unison", "Unison", "OSC1", SynthParameterKind::Continuous, 1, kMaxUnison, 1},
    {"osc1.spread", "Spread", "OSC1", SynthParameterKind::Continuous, 0, 127, 40},
    {"pos.env", "Pos Env", "OSC1", SynthParameterKind::Continuous, -63, 63, 0},
    {"pos.lfo", "Pos LFO", "OSC1", SynthParameterKind::Continuous, 0, 127, 0},
    {"osc2.wave", "Wave 2", "OSC2", SynthParameterKind::Choice, 0, kWaveCount, 0},
    {"osc2.position", "Position 2", "OSC2", SynthParameterKind::Continuous, 0, 127, 0},
    {"osc2.tune", "Tune", "OSC2", SynthParameterKind::Continuous, -24, 24, 0},
    {"osc2.fine", "Fine", "OSC2", SynthParameterKind::Continuous, -50, 50, 0},
    {"osc.mix", "Mix", "OSC2", SynthParameterKind::Continuous, 0, 127, 64},
    {"vcf.type", "Filter", "VCF", SynthParameterKind::Choice, 0, 3, 0},
    {"vcf.cutoff", "Cutoff", "VCF", SynthParameterKind::Continuous, 0, 127, 100},
    {"vcf.resonance", "Resonance", "VCF", SynthParameterKind::Continuous, 0, 127, 15},
    {"vcf.env", "Env Amt", "VCF", SynthParameterKind::Continuous, -63, 63, 0},
    {"vcf.lfo", "LFO Amt", "VCF", SynthParameterKind::Continuous, 0, 127, 0},
    {"vcf.keyboard", "Key Follow", "VCF", SynthParameterKind::Continuous, 0, 127, 64},
    {"env.attack", "Attack", "ENV", SynthParameterKind::Continuous, 0, 127, 2},
    {"env.decay", "Decay", "ENV", SynthParameterKind::Continuous, 0, 127, 50},
    {"env.sustain", "Sustain", "ENV", SynthParameterKind::Continuous, 0, 127, 100},
    {"env.release", "Release", "ENV", SynthParameterKind::Continuous, 0, 127, 30},
    {"amp.level", "Level", "ENV", SynthParameterKind::Continuous, 0, 127, 100},
    {"env2.attack", "Attack 2", "ENV2", SynthParameterKind::Continuous, 0, 127, 0},
    {"env2.decay", "Decay 2", "ENV2", SynthParameterKind::Continuous, 0, 127, 60},
    {"env2.sustain", "Sustain 2", "ENV2", SynthParameterKind::Continuous, 0, 127, 0},
    {"env2.release", "Release 2", "ENV2", SynthParameterKind::Continuous, 0, 127, 40},
    {"lfo.rate", "LFO Rate", "LFO", SynthParameterKind::Continuous, 0, 127, 80},
    {"lfo.delay", "LFO Delay", "LFO", SynthParameterKind::Continuous, 0, 127, 0},
    {"lfo.wave", "LFO Wave", "LFO", SynthParameterKind::Choice, 0, 3, 0},
    {"lfo.pitch", "Vibrato", "LFO", SynthParameterKind::Continuous, 0, 127, 0},
}};

constexpr int ParamIndex(std::string_view id)
{
  for (std::size_t index = 0; index < kParams.size(); ++index)
    if (id == kParams[index].id) return static_cast<int>(index);
  return -1;
}

using Values = std::array<std::int8_t, kParamCount>;

struct Setting {
  Param param;
  int value;
};

// The defaults (a saw through an open filter), with what a preset sets.
constexpr Values Make(std::initializer_list<Setting> settings)
{
  Values values{};
  for (std::size_t index = 0; index < kParams.size(); ++index) values[index] = kParams[index].defaultValue;
  for (const Setting& setting : settings) values[setting.param] = static_cast<std::int8_t>(setting.value);
  return values;
}

struct FactoryPreset {
  const char* id;
  const char* name;
  std::int8_t levelDb;  // evens the presets out, as PresetLevels.inc does (PRINT_WAVE_LEVELS=1 SynthRegression)
  Values values;
};

inline constexpr char kDefaultPreset[] = "wave.ppg";

inline constexpr std::array<FactoryPreset, 16> kFactoryPresets{{
    {"wave.init", "Init Saw", 6, Make({})},
    {"wave.ppg", "PPG Sweep", 8,
     Make({{kOsc1Wave, kTablePpg}, {kOsc1Position, 10}, {kPositionEnv, 50}, {kOsc2Wave, kTablePpg + 1}, {kOsc2Position, 40}, {kOsc2Fine, 8},
           {kOscMix, 50}, {kFilterCutoff, 90}, {kFilterEnv, 15}, {kEnvAttack, 5}, {kEnvDecay, 70}, {kEnvSustain, 90}, {kEnvRelease, 55},
           {kEnv2Decay, 85}, {kEnv2Sustain, 20}, {kEnv2Release, 60}})},
    {"wave.bass", "Braids Bass", 9,
     Make({{kOsc1Wave, kTableBraids}, {kOsc1Position, 30}, {kOsc1Octave, -1}, {kPositionEnv, 30}, {kOsc2Wave, kSine + 1}, {kOsc2Tune, -12},
           {kOscMix, 45}, {kFilterCutoff, 55}, {kFilterResonance, 30}, {kFilterEnv, 35}, {kEnvAttack, 0}, {kEnvDecay, 60}, {kEnvSustain, 70},
           {kEnvRelease, 25}, {kEnv2Decay, 45}, {kEnv2Release, 30}})},
    {"wave.viral", "Viral Lead", 7,
     Make({{kOsc1Wave, kTableViral}, {kOsc1Position, 50}, {kOsc1Unison, 3}, {kOsc1Spread, 35}, {kPositionLfo, 20}, {kFilterCutoff, 95},
           {kFilterResonance, 25}, {kEnvAttack, 3}, {kEnvSustain, 95}, {kEnvRelease, 35}, {kLfoRate, 95}, {kLfoDelay, 60}, {kLfoPitch, 12}})},
    {"wave.supersaw", "Supersaw", 3,
     Make({{kOsc1Unison, 7}, {kOsc1Spread, 60}, {kOsc2Wave, kSaw + 1}, {kOsc2Tune, 12}, {kOscMix, 40}, {kFilterCutoff, 92}, {kFilterResonance, 10},
           {kFilterEnv, 10}, {kEnvAttack, 8}, {kEnvDecay, 60}, {kEnvSustain, 95}, {kEnvRelease, 50}, {kEnv2Decay, 70}, {kEnv2Sustain, 40},
           {kEnv2Release, 50}})},
    {"wave.bell", "Glass Bell", 7,
     Make({{kOsc1Wave, kTable111}, {kOsc1Position, 80}, {kPositionEnv, -40}, {kOsc2Wave, kSine + 1}, {kOsc2Tune, 19}, {kOsc2Fine, 3}, {kOscMix, 45},
           {kFilterCutoff, 110}, {kEnvAttack, 0}, {kEnvDecay, 75}, {kEnvSustain, 0}, {kEnvRelease, 70}, {kEnv2Decay, 60}, {kEnv2Release, 60}})},
    {"wave.morph", "Morph Pad", 2,
     Make({{kOsc1Wave, kTableSineSaw}, {kOsc1Position, 20}, {kOsc1Unison, 3}, {kOsc1Spread, 30}, {kPositionLfo, 70}, {kOsc2Wave, kTableSineSaw + 1},
           {kOsc2Position, 60}, {kOsc2Fine, -7}, {kFilterCutoff, 80}, {kFilterResonance, 10}, {kFilterEnv, 10}, {kEnvAttack, 70}, {kEnvDecay, 60},
           {kEnvRelease, 75}, {kEnv2Attack, 80}, {kEnv2Decay, 70}, {kEnv2Sustain, 80}, {kEnv2Release, 70}, {kLfoRate, 20}})},
    {"wave.pwm", "PWM Strings", 3,
     Make({{kOsc1Wave, kPulse}, {kOsc1Position, 20}, {kPositionLfo, 80}, {kOsc2Wave, kSaw + 1}, {kOsc2Fine, 9}, {kOscMix, 55}, {kFilterCutoff, 88},
           {kFilterResonance, 8}, {kEnvAttack, 45}, {kEnvRelease, 60}, {kLfoRate, 45}})},
    {"wave.square", "Square Lead", 2,
     Make({{kOsc1Wave, kPulse}, {kOsc2Wave, kPulse + 1}, {kOsc2Tune, -12}, {kOscMix, 40}, {kFilterCutoff, 98}, {kFilterResonance, 20},
           {kFilterEnv, 8}, {kEnvDecay, 40}, {kEnvRelease, 25}, {kLfoRate, 96}, {kLfoDelay, 70}, {kLfoPitch, 18}})},
    {"wave.sub", "Sub Bass", 6,
     Make({{kOsc1Wave, kTriangle}, {kOsc1Octave, -2}, {kOsc2Wave, kSaw + 1}, {kOsc2Tune, 12}, {kOscMix, 40}, {kFilterCutoff, 60},
           {kFilterEnv, 20}, {kEnvAttack, 0}, {kEnvRelease, 20}, {kEnv2Decay, 50}, {kEnv2Sustain, 10}, {kEnv2Release, 30}})},
    {"wave.acid", "Acid", 3,
     Make({{kOsc1Octave, -1}, {kFilterCutoff, 45}, {kFilterResonance, 95}, {kFilterEnv, 45}, {kFilterKeyboard, 40}, {kEnvAttack, 0},
           {kEnvDecay, 55}, {kEnvSustain, 60}, {kEnvRelease, 15}, {kEnv2Decay, 42}, {kEnv2Release, 30}})},
    {"wave.keys", "Hollow Keys", 5,
     Make({{kOsc1Wave, kTriangle}, {kOsc2Wave, kPulse + 1}, {kOsc2Position, 30}, {kOsc2Tune, 12}, {kOscMix, 45}, {kFilterCutoff, 85},
           {kFilterEnv, 25}, {kEnvAttack, 0}, {kEnvDecay, 72}, {kEnvSustain, 30}, {kEnvRelease, 45}, {kEnv2Decay, 55}, {kEnv2Sustain, 10},
           {kEnv2Release, 45}})},
    {"wave.fifths", "Fifths", 7,
     Make({{kOsc1Unison, 3}, {kOsc1Spread, 25}, {kOsc2Wave, kSaw + 1}, {kOsc2Tune, 7}, {kFilterCutoff, 85}, {kFilterResonance, 12},
           {kFilterEnv, 15}, {kEnvAttack, 4}, {kEnvDecay, 60}, {kEnvSustain, 90}, {kEnvRelease, 45}, {kEnv2Decay, 65}, {kEnv2Sustain, 30},
           {kEnv2Release, 45}})},
    {"wave.pluck", "PPG Pluck", 11,
     Make({{kOsc1Wave, kTablePpg}, {kOsc1Position, 90}, {kPositionEnv, -60}, {kFilterCutoff, 70}, {kFilterResonance, 20}, {kFilterEnv, 40},
           {kEnvAttack, 0}, {kEnvDecay, 58}, {kEnvSustain, 0}, {kEnvRelease, 45}, {kEnv2Decay, 48}})},
    {"wave.choir", "Digi Choir", 9,
     Make({{kOsc1Wave, kTableBraids}, {kOsc1Position, 60}, {kOsc1Unison, 5}, {kOsc1Spread, 45}, {kPositionLfo, 35}, {kFilterType, 2},
           {kFilterCutoff, 82}, {kFilterResonance, 30}, {kEnvAttack, 60}, {kEnvDecay, 60}, {kEnvRelease, 70}, {kLfoRate, 30}, {kLfoPitch, 6}})},
    {"wave.wobble", "Wobble", 6,
     Make({{kOsc1Wave, kTableViral}, {kOsc1Position, 30}, {kOsc1Octave, -1}, {kPositionLfo, 40}, {kFilterCutoff, 50}, {kFilterResonance, 60},
           {kFilterLfo, 90}, {kEnvAttack, 0}, {kEnvRelease, 20}, {kLfoRate, 82}})},
}};

constexpr const FactoryPreset* FindFactoryPreset(std::string_view id)
{
  for (const FactoryPreset& preset : kFactoryPresets)
    if (id == preset.id) return &preset;
  return nullptr;
}

}  // namespace synth::wave
