#include "Library/Synth/Core/Engines/Wave/WaveSynthEngine.h"

#include "Library/Synth/Audio/SynthFx.h"
#include "Library/Synth/Core/Engines/Wave/WaveParameters.h"

#include <algorithm>
#include <string>

namespace synth {

WaveSynthEngine::WaveSynthEngine()
    : PagedSynthEngine({SynthEngineKind::Wave, "wave", "Wave"})
{
  ReserveParameters(wave::kParams.size());
  for (const wave::ParamInfo& parameter : wave::kParams)
    RegisterParameter({parameter.id, parameter.label, parameter.group, parameter.kind, parameter.minimum, parameter.maximum, 1,
                       parameter.defaultValue, parameter.defaultValue});
  RegisterTrackFxParameters();
  IndexFactoryPresets();
  (void)LoadPreset(wave::kDefaultPreset);
}

std::string WaveSynthEngine::FormatParameterValue(const SynthParameterInfo& parameter) const
{
  if (fx::IsTrackParameter(parameter.id)) return PagedSynthEngine::FormatParameterValue(parameter);
  const int value = parameter.value;
  const auto signedText = [&] { return value > 0 ? "+" + std::to_string(value) : std::to_string(value); };
  switch (wave::ParamIndex(parameter.id))
  {
    case wave::kOsc1Wave: return wave::kWaveNames[static_cast<std::size_t>(std::clamp(value, 0, wave::kWaveCount - 1))];
    case wave::kOsc2Wave: return value <= 0 ? "OFF" : wave::kWaveNames[static_cast<std::size_t>(std::min(value, static_cast<int>(wave::kWaveCount)) - 1)];
    case wave::kFilterType: return wave::kFilterNames[static_cast<std::size_t>(std::clamp(value, 0, 3))];
    case wave::kLfoWave: return wave::kLfoNames[static_cast<std::size_t>(std::clamp(value, 0, 3))];
    case wave::kOsc1Octave:
    case wave::kOsc2Tune:
    case wave::kOsc2Fine:
    case wave::kPositionEnv:
    case wave::kFilterEnv: return signedText();
    default: return PagedSynthEngine::FormatParameterValue(parameter);
  }
}

int WaveSynthEngine::FactoryParameterValue(const std::string& presetId, const SynthParameterInfo& parameter) const
{
  // A factory preset is the voice only: the track's effects start at their defaults.
  const int index = wave::ParamIndex(parameter.id);
  const wave::FactoryPreset* preset = wave::FindFactoryPreset(presetId);
  return preset != nullptr && index >= 0 ? preset->values[static_cast<std::size_t>(index)] : parameter.defaultValue;
}

std::size_t WaveSynthEngine::FactoryPresetCount() const
{
  return wave::kFactoryPresets.size();
}

std::string_view WaveSynthEngine::FactoryPresetId(std::size_t index) const
{
  return wave::kFactoryPresets[index].id;
}

std::string WaveSynthEngine::FactoryPresetName(std::size_t index) const
{
  return wave::kFactoryPresets[index].name;
}

}  // namespace synth
