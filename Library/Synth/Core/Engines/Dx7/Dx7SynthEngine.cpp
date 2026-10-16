#include "Library/Synth/Core/Engines/Dx7/Dx7SynthEngine.h"
#include "Library/Synth/Core/Engines/Dx7/Dx7FactoryPresets.h"
#include "Library/Synth/Audio/Amy/FactoryParameters.h"

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
    RegisterParameter({prefix + "attack", "Rate 1", group, SynthParameterKind::Continuous, 0, 99, 1, 80, 80});
    RegisterParameter({prefix + "decay", "Rate 2", group, SynthParameterKind::Continuous, 0, 99, 1, 55, 55});
    RegisterParameter({prefix + "sustain", "Level 3", group, SynthParameterKind::Continuous, 0, 99, 1, 65, 65});
    RegisterParameter({prefix + "release", "Rate 4", group, SynthParameterKind::Continuous, 0, 99, 1, 40, 40});
    RegisterParameter({prefix + "amp_mod", "Amp Mod", group, SynthParameterKind::Toggle, 0, 1, 1, 0, 0});
  }

  IndexFactoryPresets();

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

int Dx7SynthEngine::FactoryParameterValue(const std::string& presetId, const SynthParameterInfo& parameter) const
{
  const auto* preset = FindDx7FactoryPreset(presetId);
  return preset ? factory::Dx7Value(factory::kDx7[preset->patchNumber - 128], parameter.id) : parameter.defaultValue;
}

std::size_t Dx7SynthEngine::FactoryPresetCount() const
{
  return Dx7FactoryPresets().size();
}

std::string_view Dx7SynthEngine::FactoryPresetId(std::size_t index) const
{
  return Dx7FactoryPresets()[index].id;
}

std::string Dx7SynthEngine::FactoryPresetName(std::size_t index) const
{
  return NormalizeDx7PresetName(Dx7FactoryPresets()[index].name);
}

}  // namespace synth
