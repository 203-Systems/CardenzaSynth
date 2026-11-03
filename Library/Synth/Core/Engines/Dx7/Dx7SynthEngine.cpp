#include "Library/Synth/Core/Engines/Dx7/Dx7SynthEngine.h"
#include "Library/Synth/Core/Engines/Dx7/Dx7FactoryPresets.h"
#include "Library/Synth/Audio/Amy/FactoryParameters.h"
#include "Library/Synth/Audio/SynthFx.h"

#include <algorithm>
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
  // Eight global parameters, then fourteen per operator.
  ReserveParameters(8 + 6 * 14);
  RegisterParameter({"global.algorithm", "Algorithm", "Global", SynthParameterKind::Choice, 1, 32, 1, 5, 5});
  RegisterParameter({"global.feedback", "Feedback", "Global", SynthParameterKind::Continuous, 0, 7, 1, 2, 2});
  RegisterParameter({"global.lfo_speed", "LFO Speed", "Global", SynthParameterKind::Continuous, 0, 99, 1, 40, 40});
  RegisterParameter({"global.lfo_pitch", "Pitch Mod", "Global", SynthParameterKind::Continuous, 0, 99, 1, 0, 0});
  RegisterParameter({"global.lfo_amp", "Amp Mod", "Global", SynthParameterKind::Continuous, 0, 99, 1, 20, 20});
  RegisterParameter({"global.transpose", "Transpose", "Global", SynthParameterKind::Continuous, -24, 24, 1, 0, 0});
  RegisterParameter({"global.lfo_wave", "LFO Wave", "Global", SynthParameterKind::Choice, 0, 5, 1, 0, 0});
  RegisterParameter({"global.lfo_pms", "Pitch Sens", "Global", SynthParameterKind::Continuous, 0, 7, 1, 3, 3});

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
    RegisterParameter({prefix + "mode", "Mode", group, SynthParameterKind::Choice, 0, 1, 1, 0, 0});
    RegisterParameter({prefix + "rate3", "Rate 3", group, SynthParameterKind::Continuous, 0, 99, 1, 50, 50});
    RegisterParameter({prefix + "level1", "Level 1", group, SynthParameterKind::Continuous, 0, 99, 1, 99, 99});
    RegisterParameter({prefix + "level2", "Level 2", group, SynthParameterKind::Continuous, 0, 99, 1, 90, 90});
    RegisterParameter({prefix + "level4", "Level 4", group, SynthParameterKind::Continuous, 0, 99, 1, 0, 0});
  }
  RegisterTrackFxParameters();

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
  if (parameter.id == "global.lfo_wave")
  {
    static constexpr const char* kWaves[] = {"TRI", "SAW-", "SAW+", "SQR", "SIN", "S/H"};
    return kWaves[std::clamp(parameter.value, 0, 5)];
  }
  if (parameter.id.size() > 4 && parameter.id.compare(parameter.id.size() - 5, 5, ".mode") == 0)
  {
    return parameter.value == 0 ? "RATIO" : "FIXED";
  }
  if (parameter.id.size() > 6 && parameter.id.compare(parameter.id.size() - 7, 7, ".coarse") == 0)
  {
    // Ratio mode multiplies the note frequency; fixed mode picks 1 / 10 / 100 / 1000 Hz.
    const SynthParameterInfo* mode = FindParameter(parameter.id.substr(0, parameter.id.size() - 6) + "mode");
    if (mode != nullptr && mode->value != 0)
    {
      static constexpr const char* kFixed[] = {"1HZ", "10HZ", "100HZ", "1KHZ"};
      return kFixed[parameter.value & 3];
    }
    return parameter.value == 0 ? "x0.5" : "x" + std::to_string(parameter.value);
  }
  if (parameter.id == "global.transpose")
  {
    return parameter.value >= 0 ? "+" + std::to_string(parameter.value) : std::to_string(parameter.value);
  }
  return PagedSynthEngine::FormatParameterValue(parameter);
}

int Dx7SynthEngine::FactoryParameterValue(const std::string& presetId, const SynthParameterInfo& parameter) const
{
  if (fx::IsTrackParameter(parameter.id)) return parameter.defaultValue;
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
