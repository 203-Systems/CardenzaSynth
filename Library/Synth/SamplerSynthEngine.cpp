#include "Library/Synth/SamplerSynthEngine.h"

#include <map>

namespace synth {

namespace {

std::map<std::string, int> SamplerPreset(int start,
                                         int end,
                                         int tune,
                                         int cutoff,
                                         int drive)
{
  std::map<std::string, int> preset;
  preset["sample.start"] = start;
  preset["sample.end"] = end;
  preset["sample.loop"] = 0;
  preset["sample.tune"] = tune;
  preset["env.attack"] = 4;
  preset["env.release"] = 18;
  preset["filter.cutoff"] = cutoff;
  preset["filter.resonance"] = 18;
  preset["fx.drive"] = drive;
  preset["fx.send"] = 24;
  preset["amp.level"] = 96;
  preset["amp.pan"] = 64;
  return preset;
}

}  // namespace

SamplerSynthEngine::SamplerSynthEngine()
    : PagedSynthEngine({SynthEngineKind::Sampler, "sampler", "Sampler"})
{
  RegisterParameter({"sample.start", "Start", "Sample", SynthParameterKind::Continuous, 0, 127, 1, 0, 0});
  RegisterParameter({"sample.end", "End", "Sample", SynthParameterKind::Continuous, 0, 127, 1, 127, 127});
  RegisterParameter({"sample.loop", "Loop", "Sample", SynthParameterKind::Toggle, 0, 1, 1, 0, 0});
  RegisterParameter({"sample.tune", "Tune", "Sample", SynthParameterKind::Continuous, -24, 24, 1, 0, 0});
  RegisterParameter({"env.attack", "Attack", "Envelope", SynthParameterKind::Continuous, 0, 127, 1, 4, 4});
  RegisterParameter({"env.release", "Release", "Envelope", SynthParameterKind::Continuous, 0, 127, 1, 18, 18});
  RegisterParameter({"filter.cutoff", "Cutoff", "Filter", SynthParameterKind::Continuous, 0, 127, 1, 96, 96});
  RegisterParameter({"filter.resonance", "Resonance", "Filter", SynthParameterKind::Continuous, 0, 127, 1, 18, 18});
  RegisterParameter({"fx.drive", "Drive", "FX", SynthParameterKind::Continuous, 0, 127, 1, 0, 0});
  RegisterParameter({"fx.send", "Send", "FX", SynthParameterKind::Continuous, 0, 127, 1, 24, 24});
  RegisterParameter({"amp.level", "Level", "Amp", SynthParameterKind::Continuous, 0, 127, 1, 96, 96});
  RegisterParameter({"amp.pan", "Pan", "Amp", SynthParameterKind::Continuous, 0, 127, 1, 64, 64});

  RegisterPreset("sampler.kick", "Kick", SamplerPreset(0, 80, -12, 72, 36));
  RegisterPreset("sampler.snare", "Snare", SamplerPreset(4, 96, 0, 84, 22));
  RegisterPreset("sampler.hat", "Hat", SamplerPreset(24, 72, 12, 110, 12));
  RegisterPreset("sampler.chord", "Chord", SamplerPreset(12, 118, 0, 70, 18));
  RegisterPreset("sampler.bass", "Bass", SamplerPreset(0, 127, -7, 58, 30));
  RegisterPreset("sampler.vox", "Vox", SamplerPreset(8, 120, 0, 92, 8));

  (void)LoadPreset("sampler.kick");
}

}  // namespace synth