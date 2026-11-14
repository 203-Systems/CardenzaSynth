#include "Library/Synth/Core/Engines/Plaits/PlaitsSynthEngine.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>

#include "Library/Synth/Audio/SynthFx.h"

namespace synth {

namespace {

// A factory preset: the model and where its controls stand (the audio runtime gets every value, it
// keeps no preset table of its own). HARMONICS, TIMBRE and MORPH mean something else in every
// model; Library/third_party/eurorack/plaits/dsp/engine* says what.
struct PlaitsPreset {
  const char* id;
  const char* name;
  std::int8_t model;
  std::int8_t harmonics;
  std::int8_t timbre;
  std::int8_t morph;
  std::int8_t aux;
  std::int8_t tune;
  std::int8_t mode;
  std::int8_t attack;
  std::int8_t decay;
  std::int8_t colour;
  std::int8_t frequencyEnv;
  std::int8_t timbreEnv;
  std::int8_t morphEnv;
  std::int8_t level;
};

constexpr std::int8_t G = kPlaitsEnvelopeGate;
constexpr std::int8_t P = kPlaitsEnvelopePing;

constexpr PlaitsPreset kPresets[] = {
    // id, name, model, HARM, TIMBRE, MORPH, AUX, TUNE, mode, ATTACK, DECAY, COLOUR, pitch / timbre / morph env, level
    {"plaits.analog", "Analog Pair", 8, 66, 64, 64, 0, 0, G, 0, 56, 64, 0, 0, 0, 100},
    {"plaits.sync", "Sync Lead", 8, 80, 100, 50, 127, 0, G, 0, 50, 90, 0, 24, 0, 100},
    {"plaits.vcf", "Filter Saw", 0, 20, 60, 40, 0, 0, G, 0, 56, 64, 0, 32, 0, 100},
    {"plaits.acid", "Acid Bass", 0, 110, 40, 20, 0, -12, G, 0, 40, 64, 0, 45, 0, 100},
    {"plaits.phase", "Phase Bass", 1, 40, 70, 64, 0, -12, G, 0, 45, 64, 0, 30, 0, 100},
    {"plaits.fm6a", "FM Bank A", 2, 0, 80, 64, 0, 0, G, 0, 64, 64, 0, 0, 0, 100},
    {"plaits.fm6b", "FM Bank B", 3, 0, 80, 64, 0, 0, G, 0, 64, 64, 0, 0, 0, 100},
    {"plaits.fm6c", "FM Bank C", 4, 0, 80, 64, 0, 0, G, 0, 64, 64, 0, 0, 0, 100},
    {"plaits.terrain", "Wave Terrain", 5, 40, 70, 50, 0, 0, G, 0, 60, 64, 0, 0, 20, 100},
    {"plaits.ensemble", "String Machine", 6, 50, 90, 30, 0, 0, G, 40, 84, 100, 0, 0, 0, 100},
    {"plaits.chip", "Chip Arp", 7, 40, 20, 40, 0, 0, G, 0, 40, 64, 0, 30, 0, 100},
    {"plaits.fold", "Wavefolder", 9, 30, 70, 64, 0, 0, G, 0, 55, 64, 0, 30, 0, 100},
    {"plaits.bell", "FM Bell", 10, 90, 60, 64, 0, 0, P, 0, 90, 100, 0, 36, 0, 100},
    {"plaits.formant", "Formant", 11, 64, 70, 50, 0, 0, G, 0, 56, 64, 0, 0, 0, 100},
    {"plaits.organ", "Drawbar Organ", 12, 40, 30, 40, 127, 0, G, 0, 20, 127, 0, 0, 0, 100},
    {"plaits.harmonic", "Harmonics", 12, 50, 50, 70, 0, 0, G, 0, 56, 64, 0, 25, 0, 100},
    {"plaits.wavetable", "Wavetable", 13, 20, 64, 64, 0, 0, G, 12, 70, 64, 0, 0, 40, 100},
    {"plaits.chord", "Chord Stab", 14, 40, 50, 30, 0, 0, P, 0, 72, 80, 0, 0, 0, 100},
    {"plaits.vowel", "Vowels", 15, 20, 64, 30, 0, 0, G, 10, 56, 64, 0, 0, 30, 100},
    {"plaits.swarm", "Swarm Pad", 16, 40, 90, 110, 0, 0, G, 60, 92, 64, 0, 0, 0, 100},
    {"plaits.noise", "Noise Hit", 17, 64, 80, 90, 0, 0, P, 0, 50, 64, 0, 0, 0, 100},
    {"plaits.particle", "Particles", 18, 70, 90, 30, 0, 0, G, 20, 80, 64, 0, 0, 0, 100},
    {"plaits.string", "Plucked String", 19, 30, 70, 80, 0, 0, P, 0, 64, 64, 0, 0, 0, 100},
    {"plaits.modal", "Modal Bell", 20, 80, 60, 90, 0, 0, P, 0, 64, 64, 0, 0, 0, 100},
    {"plaits.mallet", "Mallet", 20, 30, 50, 55, 0, 0, P, 0, 64, 64, 0, 0, 0, 100},
    {"plaits.kick", "Kick", 21, 50, 50, 55, 0, -12, P, 0, 64, 64, 0, 0, 0, 100},
    {"plaits.snare", "Snare", 22, 64, 50, 50, 0, 0, P, 0, 64, 64, 0, 0, 0, 100},
    {"plaits.hat", "Hi-Hat", 23, 64, 80, 30, 0, 12, P, 0, 64, 64, 0, 0, 0, 100},
};

std::string Signed(int value)
{
  return value > 0 ? "+" + std::to_string(value) : std::to_string(value);
}

}  // namespace

PlaitsSynthEngine::PlaitsSynthEngine()
    : PagedSynthEngine({SynthEngineKind::Plaits, "plaits", "Plaits"})
{
  ReserveParameters(14);
  RegisterParameter({"osc.model", "Model", "Osc", SynthParameterKind::Choice, 0, static_cast<int>(kPlaitsModelCount) - 1, 1, 8, 8});
  RegisterParameter({"osc.harmonics", "Harmonics", "Osc", SynthParameterKind::Continuous, 0, 127, 1, 64, 64});
  RegisterParameter({"osc.timbre", "Timbre", "Osc", SynthParameterKind::Continuous, 0, 127, 1, 64, 64});
  RegisterParameter({"osc.morph", "Morph", "Osc", SynthParameterKind::Continuous, 0, 127, 1, 64, 64});
  RegisterParameter({"osc.aux", "Aux Mix", "Osc", SynthParameterKind::Continuous, 0, 127, 1, 0, 0});
  RegisterParameter({"osc.tune", "Tune", "Osc", SynthParameterKind::Continuous, -24, 24, 1, 0, 0});
  RegisterParameter({"lpg.mode", "Envelope", "LPG", SynthParameterKind::Choice, 0, 1, 1, kPlaitsEnvelopeGate, kPlaitsEnvelopeGate});
  RegisterParameter({"lpg.attack", "Attack", "LPG", SynthParameterKind::Continuous, 0, 127, 1, 0, 0});
  RegisterParameter({"lpg.decay", "Decay", "LPG", SynthParameterKind::Continuous, 0, 127, 1, 64, 64});
  RegisterParameter({"lpg.colour", "Colour", "LPG", SynthParameterKind::Continuous, 0, 127, 1, 64, 64});
  RegisterParameter({"mod.frequency", "Pitch Env", "Mod", SynthParameterKind::Continuous, -63, 63, 1, 0, 0});
  RegisterParameter({"mod.timbre", "Timbre Env", "Mod", SynthParameterKind::Continuous, -63, 63, 1, 0, 0});
  RegisterParameter({"mod.morph", "Morph Env", "Mod", SynthParameterKind::Continuous, -63, 63, 1, 0, 0});
  RegisterParameter({"amp.level", "Level", "Amp", SynthParameterKind::Continuous, 0, 127, 1, 100, 100});
  RegisterTrackFxParameters();

  for (const PlaitsPreset& preset : kPresets)
  {
    std::map<std::string, int> values;
    values["osc.model"] = preset.model;
    values["osc.harmonics"] = preset.harmonics;
    values["osc.timbre"] = preset.timbre;
    values["osc.morph"] = preset.morph;
    values["osc.aux"] = preset.aux;
    values["osc.tune"] = preset.tune;
    values["lpg.mode"] = preset.mode;
    values["lpg.attack"] = preset.attack;
    values["lpg.decay"] = preset.decay;
    values["lpg.colour"] = preset.colour;
    values["mod.frequency"] = preset.frequencyEnv;
    values["mod.timbre"] = preset.timbreEnv;
    values["mod.morph"] = preset.morphEnv;
    values["amp.level"] = preset.level;
    RegisterPreset(preset.id, preset.name, values);
  }

  (void)LoadPreset("plaits.analog");
}

std::string PlaitsSynthEngine::FormatParameterValue(const SynthParameterInfo& parameter) const
{
  if (fx::IsTrackParameter(parameter.id)) return PagedSynthEngine::FormatParameterValue(parameter);
  if (parameter.id == "osc.model")
    return kPlaitsModelNames[static_cast<std::size_t>(std::clamp(parameter.value, 0, static_cast<int>(kPlaitsModelCount) - 1))];
  if (parameter.id == "lpg.mode") return parameter.value == kPlaitsEnvelopePing ? "PING" : "GATE";
  if (parameter.id == "osc.aux")
  {
    if (parameter.value <= 0) return "OUT";
    if (parameter.value >= 127) return "AUX";
    return std::to_string(parameter.value);
  }
  if (parameter.id == "osc.tune" || parameter.id == "mod.frequency" || parameter.id == "mod.timbre" || parameter.id == "mod.morph")
    return Signed(parameter.value);
  return PagedSynthEngine::FormatParameterValue(parameter);
}

}  // namespace synth
