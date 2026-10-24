#include "Library/Synth/Core/Engines/Juno/JunoSynthEngine.h"
#include "Library/Synth/Core/Engines/Juno/JunoFactoryPresets.h"
#include "Library/Synth/Audio/Amy/FactoryParameters.h"
#include "Library/Synth/Audio/SynthFx.h"

#include <algorithm>
#include <string>

namespace synth {

JunoSynthEngine::JunoSynthEngine()
    : PagedSynthEngine({SynthEngineKind::Juno, "juno", "Juno"})
{
  RegisterParameter({"dco.range", "DCO Range", "DCO", SynthParameterKind::Choice, 0, 2, 1, 1, 1});
  RegisterParameter({"dco.pwm", "PWM", "DCO", SynthParameterKind::Continuous, 0, 127, 1, 48, 48});
  RegisterParameter({"dco.sub", "Sub", "DCO", SynthParameterKind::Continuous, 0, 127, 1, 68, 68});
  RegisterParameter({"dco.noise", "Noise", "DCO", SynthParameterKind::Continuous, 0, 127, 1, 0, 0});
  RegisterParameter({"dco.saw", "Saw", "DCO", SynthParameterKind::Toggle, 0, 1, 1, 1, 1});
  RegisterParameter({"vcf.cutoff", "Cutoff", "VCF", SynthParameterKind::Continuous, 0, 127, 1, 92, 92});
  RegisterParameter({"vcf.resonance", "Resonance", "VCF", SynthParameterKind::Continuous, 0, 127, 1, 18, 18});
  RegisterParameter({"vcf.env", "Env Amt", "VCF", SynthParameterKind::Continuous, 0, 127, 1, 54, 54});
  RegisterParameter({"vcf.lfo", "LFO Amt", "VCF", SynthParameterKind::Continuous, 0, 127, 1, 18, 18});
  RegisterParameter({"vca.level", "VCA Level", "VCA", SynthParameterKind::Continuous, 0, 127, 1, 96, 96});
  RegisterParameter({"env.attack", "Attack", "ENV", SynthParameterKind::Continuous, 0, 127, 1, 28, 28});
  RegisterParameter({"env.decay", "Decay", "ENV", SynthParameterKind::Continuous, 0, 127, 1, 42, 42});
  RegisterParameter({"env.sustain", "Sustain", "ENV", SynthParameterKind::Continuous, 0, 127, 1, 78, 78});
  RegisterParameter({"env.release", "Release", "ENV", SynthParameterKind::Continuous, 0, 127, 1, 36, 36});
  RegisterParameter({"lfo.rate", "LFO Rate", "LFO", SynthParameterKind::Continuous, 0, 127, 1, 40, 40});
  RegisterParameter({"lfo.delay", "LFO Delay", "LFO", SynthParameterKind::Continuous, 0, 127, 1, 22, 22});
  RegisterParameter({"dco.pulse", "Pulse", "DCO", SynthParameterKind::Toggle, 0, 1, 1, 1, 1});
  RegisterParameter({"dco.pwm_mode", "PWM Mode", "DCO", SynthParameterKind::Choice, 0, 1, 1, 0, 0});
  RegisterParameter({"dco.lfo", "DCO LFO", "DCO", SynthParameterKind::Continuous, 0, 127, 1, 0, 0});
  RegisterParameter({"vcf.polarity", "Env Polarity", "VCF", SynthParameterKind::Choice, 0, 1, 1, 0, 0});
  RegisterParameter({"vcf.keyboard", "Key Follow", "VCF", SynthParameterKind::Continuous, 0, 127, 1, 0, 0});
  RegisterParameter({"vca.mode", "VCA Mode", "VCA", SynthParameterKind::Choice, 0, 1, 1, 0, 0});
  RegisterTrackFxParameters();

  IndexFactoryPresets();

  (void)LoadPreset("juno.a51");
}

std::string JunoSynthEngine::FormatParameterValue(const SynthParameterInfo& parameter) const
{
  if (parameter.id == "dco.pwm_mode") return parameter.value == 0 ? "LFO" : "MAN";
  if (parameter.id == "vcf.polarity") return parameter.value == 0 ? "POS" : "NEG";
  if (parameter.id == "vca.mode") return parameter.value == 0 ? "ENV" : "GATE";
  if (parameter.id == "dco.range")
  {
    static const char* kRanges[] = {"16'", "8'", "4'"};
    return kRanges[std::clamp(parameter.value, 0, 2)];
  }
  if (parameter.id == "hpf.mode")
  {
    static const char* kModes[] = {"0", "1", "2", "3"};
    return kModes[std::clamp(parameter.value, 0, 3)];
  }
  return PagedSynthEngine::FormatParameterValue(parameter);
}

int JunoSynthEngine::FactoryParameterValue(const std::string& presetId, const SynthParameterInfo& parameter) const
{
  // The Juno patch carries its chorus; the other effects start at their defaults.
  if (fx::IsTrackParameter(parameter.id) && parameter.id != "fx.chorus") return parameter.defaultValue;
  const auto* preset = FindJunoFactoryPreset(presetId);
  return preset ? factory::JunoValue(factory::kJuno[preset->patchNumber], parameter.id) : parameter.defaultValue;
}

std::size_t JunoSynthEngine::FactoryPresetCount() const
{
  return JunoFactoryPresets().size();
}

std::string_view JunoSynthEngine::FactoryPresetId(std::size_t index) const
{
  return JunoFactoryPresets()[index].id;
}

std::string JunoSynthEngine::FactoryPresetName(std::size_t index) const
{
  return std::string(JunoFactoryPresets()[index].name);
}

}  // namespace synth
