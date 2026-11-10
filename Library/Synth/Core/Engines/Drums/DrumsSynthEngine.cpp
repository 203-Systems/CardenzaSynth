#include "Library/Synth/Core/Engines/Drums/DrumsSynthEngine.h"

#include <cstdio>

#include "Library/Synth/Audio/SynthFx.h"

namespace synth {

int DrumPadOfParameter(const std::string& id)
{
  // "padN.sound" / "padN.tune", N = 1..8
  if (id.size() < 6 || id.compare(0, 3, "pad") != 0 || id[3] < '1' || id[3] > '8' || id[4] != '.') return -1;
  return id[3] - '1';
}

DrumsSynthEngine::DrumsSynthEngine()
    : PagedSynthEngine({SynthEngineKind::Drums, "drums", "Drums"})
{
  ReserveParameters(kDrumNotes.size() * 3 + 1);
  const int lastSound = static_cast<int>(drums::SoundCount()) - 1;
  const drums::DrumKit& first = drums::Kit(0);
  for (std::size_t pad = 0; pad < kDrumNotes.size(); ++pad)
  {
    RegisterParameter({kDrumSoundIds[pad], kDrumLabels[pad], "Pads", SynthParameterKind::Choice, 0, lastSound, 1,
                       first.pads[pad].sound, first.pads[pad].sound});
    RegisterParameter({kDrumTuneIds[pad], kDrumLabels[pad], "Pads", SynthParameterKind::Continuous, -drums::kMaxTune, drums::kMaxTune, 1,
                       first.pads[pad].tune, first.pads[pad].tune});
  }
  for (std::size_t pad = 0; pad < kDrumNotes.size(); ++pad)
    RegisterParameter({kDrumParameterIds[pad], kDrumLabels[pad], "Kit", SynthParameterKind::Continuous, 0, 127, 1, 100, 100});
  RegisterParameter({"master.level", "Master", "Kit", SynthParameterKind::Continuous, 0, 127, 1, 100, 100});
  RegisterTrackFxParameters();
  // A kit is a preset: what each pad sounds, at what pitch, all at the same level.
  for (std::size_t kit = 0; kit < drums::KitCount(); ++kit)
  {
    const drums::DrumKit& source = drums::Kit(kit);
    std::map<std::string, int> preset;
    for (std::size_t pad = 0; pad < kDrumNotes.size(); ++pad)
    {
      preset[kDrumSoundIds[pad]] = source.pads[pad].sound;
      preset[kDrumTuneIds[pad]] = source.pads[pad].tune;
      preset[kDrumParameterIds[pad]] = 100;
    }
    preset["master.level"] = 100;
    RegisterPreset(source.id, source.name, preset);
  }
  (void)LoadPreset(first.id);
}

std::string DrumsSynthEngine::FormatParameterValue(const SynthParameterInfo& parameter) const
{
  if (fx::IsTrackParameter(parameter.id)) return PagedSynthEngine::FormatParameterValue(parameter);
  const int pad = DrumPadOfParameter(parameter.id);
  if (pad >= 0 && parameter.id.compare(5, std::string::npos, "sound") == 0) return drums::Sound(static_cast<std::size_t>(parameter.value)).name;
  if (pad >= 0)
  {
    char text[8];
    std::snprintf(text, sizeof(text), parameter.value > 0 ? "+%d" : "%d", parameter.value);
    return text;
  }
  return std::to_string(parameter.value * 100 / 127) + "%";
}

}  // namespace synth
