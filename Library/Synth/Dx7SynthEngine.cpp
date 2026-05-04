#include "Library/Synth/Dx7SynthEngine.h"

#include <map>

namespace synth {

namespace {

std::map<std::string, int> Dx7Preset(int algorithm,
                                     int feedback,
                                     int transpose,
                                     int carrierOutput,
                                     int modOutput,
                                     int coarseCarrier,
                                     int coarseMod)
{
  std::map<std::string, int> preset;
  preset["global.algorithm"] = algorithm;
  preset["global.feedback"] = feedback;
  preset["global.transpose"] = transpose;
  preset["global.lfo_speed"] = 35 + algorithm;
  preset["global.lfo_delay"] = 12 + feedback * 3;
  preset["global.lfo_amp"] = 16 + algorithm % 10;
  preset["op1.output"] = carrierOutput;
  preset["op1.coarse"] = coarseCarrier;
  preset["op1.attack"] = 92;
  preset["op1.decay"] = 54;
  preset["op1.sustain"] = 68;
  preset["op1.release"] = 40;
  preset["op2.output"] = modOutput;
  preset["op2.coarse"] = coarseMod;
  preset["op2.attack"] = 84;
  preset["op2.decay"] = 60;
  preset["op2.sustain"] = 44;
  preset["op2.release"] = 32;
  preset["op3.output"] = carrierOutput - 10;
  preset["op3.coarse"] = coarseCarrier;
  preset["op4.output"] = modOutput - 8;
  preset["op4.coarse"] = coarseMod;
  preset["op5.output"] = carrierOutput - 18;
  preset["op5.coarse"] = coarseCarrier + 1;
  preset["op6.output"] = modOutput - 18;
  preset["op6.coarse"] = coarseMod + 1;
  return preset;
}

}  // namespace

Dx7SynthEngine::Dx7SynthEngine()
    : PagedSynthEngine({SynthEngineKind::Dx7, "dx7", "DX7"})
{
  RegisterParameter({"global.algorithm", "Algorithm", "Global", SynthParameterKind::Choice, 1, 32, 1, 5, 5});
  RegisterParameter({"global.feedback", "Feedback", "Global", SynthParameterKind::Continuous, 0, 7, 1, 2, 2});
  RegisterParameter({"global.lfo_speed", "LFO Speed", "Global", SynthParameterKind::Continuous, 0, 99, 1, 40, 40});
  RegisterParameter({"global.lfo_delay", "LFO Delay", "Global", SynthParameterKind::Continuous, 0, 99, 1, 20, 20});
  RegisterParameter({"global.lfo_pitch", "Pitch Mod", "Global", SynthParameterKind::Continuous, 0, 99, 1, 0, 0});
  RegisterParameter({"global.lfo_amp", "Amp Mod", "Global", SynthParameterKind::Continuous, 0, 99, 1, 20, 20});
  RegisterParameter({"global.transpose", "Transpose", "Global", SynthParameterKind::Continuous, -24, 24, 1, 0, 0});

  for (int op = 1; op <= 6; ++op)
  {
    const std::string prefix = "op" + std::to_string(op) + ".";
    const std::string group = "Operator " + std::to_string(op);
    RegisterParameter({prefix + "output", "Output", group, SynthParameterKind::Continuous, 0, 99, 1, op == 1 ? 90 : 75, op == 1 ? 90 : 75});
    RegisterParameter({prefix + "coarse", "Coarse", group, SynthParameterKind::Continuous, 0, 31, 1, op == 1 ? 1 : 2, op == 1 ? 1 : 2});
    RegisterParameter({prefix + "fine", "Fine", group, SynthParameterKind::Continuous, 0, 99, 1, 0, 0});
    RegisterParameter({prefix + "detune", "Detune", group, SynthParameterKind::Continuous, -7, 7, 1, 0, 0});
    RegisterParameter({prefix + "attack", "Attack", group, SynthParameterKind::Continuous, 0, 99, 1, 80, 80});
    RegisterParameter({prefix + "decay", "Decay", group, SynthParameterKind::Continuous, 0, 99, 1, 55, 55});
    RegisterParameter({prefix + "sustain", "Sustain", group, SynthParameterKind::Continuous, 0, 99, 1, 65, 65});
    RegisterParameter({prefix + "release", "Release", group, SynthParameterKind::Continuous, 0, 99, 1, 40, 40});
    RegisterParameter({prefix + "velocity", "Velocity", group, SynthParameterKind::Continuous, 0, 7, 1, 3, 3});
    RegisterParameter({prefix + "amp_mod", "Amp Mod", group, SynthParameterKind::Continuous, 0, 3, 1, 0, 0});
  }

  RegisterPreset("dx7.piano1", "Piano 1", Dx7Preset(5, 2, 0, 92, 72, 1, 2));
  RegisterPreset("dx7.piano3", "Piano 3", Dx7Preset(6, 3, 0, 96, 68, 1, 3));
  RegisterPreset("dx7.epiano1", "E.Piano 1", Dx7Preset(11, 4, 0, 88, 77, 1, 14));
  RegisterPreset("dx7.brass1", "Brass 1", Dx7Preset(20, 2, -12, 84, 70, 2, 1));
  RegisterPreset("dx7.organ1", "E.Organ 1", Dx7Preset(32, 1, 0, 90, 62, 1, 1));
  RegisterPreset("dx7.bell", "FM Bell", Dx7Preset(1, 6, 0, 97, 83, 1, 8));

  (void)LoadPreset("dx7.epiano1");
}

std::string Dx7SynthEngine::FormatParameterValue(const SynthParameterInfo& parameter) const
{
  if (parameter.id == "global.algorithm")
  {
    return "ALG " + std::to_string(parameter.value);
  }
  if (parameter.id.find("detune") != std::string::npos)
  {
    return parameter.value >= 0 ? "+" + std::to_string(parameter.value) : std::to_string(parameter.value);
  }
  if (parameter.id == "global.transpose")
  {
    return parameter.value >= 0 ? "+" + std::to_string(parameter.value) : std::to_string(parameter.value);
  }
  return PagedSynthEngine::FormatParameterValue(parameter);
}

}  // namespace synth