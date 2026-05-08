#include "Library/Synth/Core/Engines/Juno/JunoSynthEngine.h"

#include <algorithm>
#include <map>

namespace synth {

namespace {

std::map<std::string, int> JunoPreset(int cutoff,
                                      int resonance,
                                      int attack,
                                      int release,
                                      int chorus,
                                      int sub,
                                      int noise)
{
  std::map<std::string, int> preset;
  preset["dco.range"] = 1;
  preset["dco.pwm"] = 48;
  preset["dco.sub"] = sub;
  preset["dco.noise"] = noise;
  preset["dco.saw"] = 1;
  preset["vcf.cutoff"] = cutoff;
  preset["vcf.resonance"] = resonance;
  preset["vcf.env"] = 54;
  preset["vcf.lfo"] = 18;
  preset["vca.level"] = 96;
  preset["env.attack"] = attack;
  preset["env.decay"] = 42;
  preset["env.sustain"] = 78;
  preset["env.release"] = release;
  preset["chorus.mode"] = chorus;
  preset["hpf.mode"] = 0;
  preset["lfo.rate"] = 40;
  preset["lfo.delay"] = 22;
  return preset;
}

}  // namespace

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
  RegisterParameter({"chorus.mode", "Chorus", "FX", SynthParameterKind::Choice, 0, 2, 1, 0, 0});
  RegisterParameter({"hpf.mode", "HPF", "VCF", SynthParameterKind::Choice, 0, 3, 1, 0, 0});

  RegisterPreset("juno.saw", "Saw", JunoPreset(100, 10, 16, 24, 0, 40, 0));
  RegisterPreset("juno.pwm", "PWM", JunoPreset(92, 14, 20, 28, 1, 32, 0));
  RegisterPreset("juno.hoover", "Hoover", JunoPreset(88, 64, 4, 58, 2, 84, 10));
  RegisterPreset("juno.strings", "Strings", JunoPreset(74, 22, 48, 50, 1, 74, 0));
  RegisterPreset("juno.brass", "Brass", JunoPreset(68, 40, 6, 30, 0, 52, 5));
  RegisterPreset("juno.bass", "Bass", JunoPreset(54, 32, 2, 18, 0, 96, 0));
  RegisterPreset("juno.pad", "Pad", JunoPreset(82, 20, 54, 60, 2, 64, 0));

  (void)LoadPreset("juno.saw");
}

std::string JunoSynthEngine::FormatParameterValue(const SynthParameterInfo& parameter) const
{
  if (parameter.id == "chorus.mode")
  {
    static const char* kModes[] = {"OFF", "I", "II"};
    return kModes[std::clamp(parameter.value, 0, 2)];
  }
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

}  // namespace synth