#include "Library/Synth/Core/Engines/Dx7/Dx7SynthEngine.h"
#include "Library/Synth/Core/Engines/Dx7/Dx7FactoryPresets.h"

#include <cctype>
#include <string>
#include <string_view>

namespace synth {

namespace {

std::string NormalizeDx7PresetName(std::string_view name)
{
  std::string normalized;
  normalized.reserve(name.size());

  bool lastWasSpace = true;
  for (char character : name)
  {
    if (std::isspace(static_cast<unsigned char>(character)) != 0)
    {
      if (!lastWasSpace)
      {
        normalized.push_back(' ');
        lastWasSpace = true;
      }
      continue;
    }

    normalized.push_back(character);
    lastWasSpace = false;
  }

  if (!normalized.empty() && normalized.back() == ' ')
  {
    normalized.pop_back();
  }

  return normalized;
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

  const auto& presets = Dx7FactoryPresets();
  for (const Dx7FactoryPresetDefinition& preset : presets)
  {
    RegisterPreset(std::string(preset.id), NormalizeDx7PresetName(preset.name), {});
  }

  (void)LoadPreset("dx7.rom1a.11");
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

bool Dx7SynthEngine::ShouldSyncParameterToRuntime(const SynthParameterInfo& parameter) const
{
  return parameter.id == "global.transpose";
}

bool Dx7SynthEngine::ShouldSyncParameterOnPresetLoad(const SynthParameterInfo& parameter) const
{
  (void)parameter;
  return false;
}

}  // namespace synth
