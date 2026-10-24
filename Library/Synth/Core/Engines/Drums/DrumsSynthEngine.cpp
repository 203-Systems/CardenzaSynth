#include "Library/Synth/Core/Engines/Drums/DrumsSynthEngine.h"

#include "Library/Synth/Audio/SynthFx.h"

namespace synth {

DrumsSynthEngine::DrumsSynthEngine()
    : PagedSynthEngine({SynthEngineKind::Drums, "drums", "Drums"})
{
  std::map<std::string, int> preset;
  for (std::size_t i = 0; i < kDrumNotes.size(); ++i)
  {
    RegisterParameter({kDrumParameterIds[i], kDrumLabels[i], "Kit", SynthParameterKind::Continuous, 0, 127, 1, 100, 100});
    preset[kDrumParameterIds[i]] = 100;
  }
  RegisterParameter({"master.level", "Master", "Kit", SynthParameterKind::Continuous, 0, 127, 1, 100, 100});
  preset["master.level"] = 100;
  RegisterTrackFxParameters();
  RegisterPreset("drums.808", "808 Kit", preset);
  (void)LoadPreset("drums.808");
}

std::string DrumsSynthEngine::FormatParameterValue(const SynthParameterInfo& parameter) const
{
  if (fx::IsTrackParameter(parameter.id)) return PagedSynthEngine::FormatParameterValue(parameter);
  return std::to_string(parameter.value * 100 / 127) + "%";
}

}  // namespace synth
