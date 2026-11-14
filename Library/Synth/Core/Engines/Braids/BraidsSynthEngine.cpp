#include "Library/Synth/Core/Engines/Braids/BraidsSynthEngine.h"

#include <algorithm>
#include <string>

#include "Library/Synth/Audio/Braids/BraidsModels.h"
#include "Library/Synth/Audio/SynthFx.h"
#include "Library/Synth/Core/Engines/Braids/BraidsFactoryPresets.h"

namespace synth {

namespace {

std::string Signed(int value)
{
  return value > 0 ? "+" + std::to_string(value) : std::to_string(value);
}

}  // namespace

BraidsSynthEngine::BraidsSynthEngine()
    : PagedSynthEngine({SynthEngineKind::Braids, "braids", "Braids"})
{
  // The defaults are also the audio runtime's (braids::Params, BraidsAmpParams): what a preset starts
  // from before its values arrive.
  ReserveParameters(kBraidsParameterCount);
  const int lastModel = static_cast<int>(braids::kModelCount) - 1;
  RegisterParameter({"osc.model", "Model", "Oscillator", SynthParameterKind::Choice, 0, lastModel, 1, 0, 0});
  RegisterParameter({"osc.timbre", "Timbre", "Oscillator", SynthParameterKind::Continuous, 0, 127, 1, 64, 64});
  RegisterParameter({"osc.color", "Color", "Oscillator", SynthParameterKind::Continuous, 0, 127, 1, 64, 64});
  RegisterParameter({"osc.tune", "Tune", "Oscillator", SynthParameterKind::Continuous, -24, 24, 1, 0, 0});
  RegisterParameter({"osc.fine", "Fine", "Oscillator", SynthParameterKind::Continuous, -50, 50, 1, 0, 0});
  RegisterParameter({"env.attack", "Attack", "Envelope", SynthParameterKind::Continuous, 0, 127, 1, 0, 0});
  RegisterParameter({"env.decay", "Decay", "Envelope", SynthParameterKind::Continuous, 0, 127, 1, 64, 64});
  RegisterParameter({"env.sustain", "Sustain", "Envelope", SynthParameterKind::Continuous, 0, 127, 1, 127, 127});
  RegisterParameter({"env.release", "Release", "Envelope", SynthParameterKind::Continuous, 0, 127, 1, 24, 24});
  RegisterParameter({"amp.level", "Level", "Envelope", SynthParameterKind::Continuous, 0, 127, 1, 100, 100});
  RegisterParameter({"mod.attack", "Mod Attack", "Mod", SynthParameterKind::Continuous, 0, 127, 1, 0, 0});
  RegisterParameter({"mod.decay", "Mod Decay", "Mod", SynthParameterKind::Continuous, 0, 127, 1, 48, 48});
  RegisterParameter({"mod.timbre", "Mod Timbre", "Mod", SynthParameterKind::Continuous, -63, 63, 1, 0, 0});
  RegisterParameter({"mod.color", "Mod Color", "Mod", SynthParameterKind::Continuous, -63, 63, 1, 0, 0});
  RegisterParameter({"mod.pitch", "Mod Pitch", "Mod", SynthParameterKind::Continuous, -63, 63, 1, 0, 0});
  RegisterParameter({"lfo.rate", "LFO Rate", "LFO", SynthParameterKind::Continuous, 0, 127, 1, 64, 64});
  RegisterParameter({"lfo.timbre", "LFO Timbre", "LFO", SynthParameterKind::Continuous, 0, 127, 1, 0, 0});
  RegisterParameter({"lfo.color", "LFO Color", "LFO", SynthParameterKind::Continuous, 0, 127, 1, 0, 0});
  RegisterParameter({"lfo.pitch", "LFO Pitch", "LFO", SynthParameterKind::Continuous, 0, 127, 1, 0, 0});
  RegisterTrackFxParameters();

  IndexFactoryPresets();

  (void)LoadPreset(kBraidsDefaultPreset);
}

std::string BraidsSynthEngine::FormatParameterValue(const SynthParameterInfo& parameter) const
{
  if (parameter.id == "osc.model") return braids::Model(parameter.value).name;
  if (parameter.id == "osc.tune" || parameter.id == "osc.fine" || parameter.id == "mod.timbre" || parameter.id == "mod.color" ||
      parameter.id == "mod.pitch")
    return Signed(parameter.value);
  if (parameter.id == "lfo.timbre" || parameter.id == "lfo.color" || parameter.id == "lfo.pitch")
    return parameter.value == 0 ? "OFF" : std::to_string(parameter.value);
  return PagedSynthEngine::FormatParameterValue(parameter);
}

int BraidsSynthEngine::FactoryParameterValue(const std::string& presetId, const SynthParameterInfo& parameter) const
{
  // The factory presets leave the track's effects at their defaults.
  if (fx::IsTrackParameter(parameter.id)) return parameter.defaultValue;
  const BraidsFactoryPreset* preset = FindBraidsFactoryPreset(presetId);
  if (preset == nullptr) return parameter.defaultValue;
  const auto id = std::find_if(kBraidsParameterIds.begin(), kBraidsParameterIds.end(), [&](const char* candidate) { return parameter.id == candidate; });
  return id == kBraidsParameterIds.end() ? parameter.defaultValue : preset->values[static_cast<std::size_t>(id - kBraidsParameterIds.begin())];
}

std::size_t BraidsSynthEngine::FactoryPresetCount() const
{
  return BraidsFactoryPresets().size();
}

std::string_view BraidsSynthEngine::FactoryPresetId(std::size_t index) const
{
  return BraidsFactoryPresets()[index].id;
}

std::string BraidsSynthEngine::FactoryPresetName(std::size_t index) const
{
  return std::string(BraidsFactoryPresets()[index].name);
}

}  // namespace synth
