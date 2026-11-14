#include "Library/Synth/Core/Engines/Drums/DrumsSynthEngine.h"

#include "Library/Synth/Audio/SynthFx.h"
#if defined(CARDENZA_AMY_BENCH)
#include "Library/Synth/Audio/Amy/AmyBench.h"
#endif

namespace synth {

DrumsSynthEngine::DrumsSynthEngine()
    : PagedSynthEngine({SynthEngineKind::Drums, "drums", "Drums"})
{
  ReserveParameters(kDrumNotes.size() + 1);
  std::map<std::string, int> preset;
  for (std::size_t i = 0; i < kDrumNotes.size(); ++i)
  {
    RegisterParameter({kDrumParameterIds[i], kDrumLabels[i], "Kit", SynthParameterKind::Continuous, 0, 127, 1, 100, 100});
    preset[kDrumParameterIds[i]] = 100;
  }
  RegisterParameter({"master.level", "Master", "Kit", SynthParameterKind::Continuous, 0, 127, 1, 100, 100});
  preset["master.level"] = 100;
  RegisterTrackFxParameters();
#if defined(CARDENZA_AMY_BENCH)
  // Test builds: AMY's Gamma9001 kits (AmyBench.h), the 808 first.
  for (const bench::DrumKit& kit : bench::kDrumKits) RegisterPreset(kit.id, kit.name, preset);
#else
  RegisterPreset("drums.808", "808 Kit", preset);
#endif
  (void)LoadPreset("drums.808");
}

std::string DrumsSynthEngine::FormatParameterValue(const SynthParameterInfo& parameter) const
{
  if (fx::IsTrackParameter(parameter.id)) return PagedSynthEngine::FormatParameterValue(parameter);
  return std::to_string(parameter.value * 100 / 127) + "%";
}

}  // namespace synth
