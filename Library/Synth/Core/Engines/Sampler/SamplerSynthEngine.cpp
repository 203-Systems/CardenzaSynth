#include "Library/Synth/Core/Engines/Sampler/SamplerSynthEngine.h"

#include <algorithm>
#include <array>
#include <map>
#include <string>

namespace synth {

namespace {

std::map<std::string, int> SamplerPreset(int start,
                                         int end,
                                         int loop,
                                         int tune,
                                         int root,
                                         int gain,
                                         int cutoff,
                                         int resonance,
                                         int release)
{
  std::map<std::string, int> preset;
  preset["sample.start"] = start;
  preset["sample.end"] = end;
  preset["sample.loop"] = loop;
  preset["sample.tune"] = tune;
  preset["sample.root"] = root;
  preset["sample.gain"] = gain;
  preset["env.attack"] = 4;
  preset["env.release"] = release;
  preset["filter.cutoff"] = cutoff;
  preset["filter.resonance"] = resonance;
  preset["amp.level"] = 96;
  return preset;
}

std::string FormatSignedSemitones(int value)
{
  return value >= 0 ? "+" + std::to_string(value) : std::to_string(value);
}

std::string FormatMidiNote(int value)
{
  static constexpr std::array<const char*, 12> kNoteNames = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  const int note = std::clamp(value, 0, 127);
  return std::string(kNoteNames[static_cast<std::size_t>(note % 12)]) + std::to_string(note / 12 - 1);
}

std::string FormatPercent127(int value)
{
  return std::to_string((std::clamp(value, 0, 127) * 100) / 127) + "%";
}

}  // namespace

SamplerSynthEngine::SamplerSynthEngine()
    : PagedSynthEngine({SynthEngineKind::Sampler, "sampler", "Sampler"})
{
  ReserveParameters(11);
  RegisterParameter({"sample.start", "Start", "Sample", SynthParameterKind::Continuous, 0, 127, 1, 0, 0});
  RegisterParameter({"sample.end", "End", "Sample", SynthParameterKind::Continuous, 0, 127, 1, 127, 127});
  RegisterParameter({"sample.loop", "Loop", "Sample", SynthParameterKind::Toggle, 0, 1, 1, 0, 0});
  RegisterParameter({"sample.tune", "Tune", "Sample", SynthParameterKind::Continuous, -24, 24, 1, 0, 0});
  RegisterParameter({"sample.root", "Root", "Sample", SynthParameterKind::Choice, 0, 127, 1, 60, 60});
  RegisterParameter({"sample.gain", "Gain", "Sample", SynthParameterKind::Continuous, 0, 127, 1, 96, 96});
  RegisterParameter({"env.attack", "Attack", "Envelope", SynthParameterKind::Continuous, 0, 127, 1, 4, 4});
  RegisterParameter({"env.release", "Release", "Envelope", SynthParameterKind::Continuous, 0, 127, 1, 18, 18});
  RegisterParameter({"filter.cutoff", "Cutoff", "Filter", SynthParameterKind::Continuous, 0, 127, 1, 96, 96});
  RegisterParameter({"filter.resonance", "Resonance", "Filter", SynthParameterKind::Continuous, 0, 127, 1, 18, 18});
  RegisterParameter({"amp.level", "Level", "Amp", SynthParameterKind::Continuous, 0, 127, 1, 96, 96});
  RegisterTrackFxParameters();

  RegisterPreset("sampler.empty", "Empty Slot", SamplerPreset(0, 127, 0, 0, 60, 96, 96, 18, 18));
  RegisterPreset("sampler.oneshot", "One Shot", SamplerPreset(0, 112, 0, 0, 60, 104, 104, 12, 14));
  RegisterPreset("sampler.loop", "Looping", SamplerPreset(0, 127, 1, 0, 60, 96, 92, 16, 22));
  RegisterPreset("sampler.chop", "Tight Chop", SamplerPreset(8, 72, 0, 0, 60, 112, 112, 10, 8));
  RegisterPreset("sampler.low", "Low Root", SamplerPreset(0, 127, 0, -7, 48, 100, 70, 20, 28));
  RegisterPreset("sampler.soft", "Soft Fade", SamplerPreset(4, 120, 0, 0, 60, 88, 82, 8, 48));

  (void)LoadPreset("sampler.empty");
}

std::string SamplerSynthEngine::FormatParameterValue(const SynthParameterInfo& parameter) const
{
  if (parameter.id == "sample.loop")
  {
    return parameter.value > 0 ? "ON" : "OFF";
  }
  if (parameter.id == "sample.tune")
  {
    return FormatSignedSemitones(parameter.value);
  }
  if (parameter.id == "sample.root")
  {
    return FormatMidiNote(parameter.value);
  }
  if (parameter.id == "sample.start" || parameter.id == "sample.end" || parameter.id == "sample.gain" ||
      parameter.id == "amp.level")
  {
    return FormatPercent127(parameter.value);
  }
  return PagedSynthEngine::FormatParameterValue(parameter);
}

}  // namespace synth
