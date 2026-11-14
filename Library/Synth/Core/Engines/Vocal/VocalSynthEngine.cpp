#include "Library/Synth/Core/Engines/Vocal/VocalSynthEngine.h"

#include <algorithm>
#include <map>
#include <string>

#include "Library/Synth/Audio/SynthAudioRuntime.h"

namespace synth {

namespace {

// How the voice sounds: what a preset holds.
std::map<std::string, int> VocalPreset(int level, int attack, int release, int glide, int vibrato, int rate, int delay, int mode)
{
  std::map<std::string, int> preset;
  preset["amp.level"] = level;
  preset["env.attack"] = attack;
  preset["env.release"] = release;
  preset["voice.glide"] = glide;
  preset["vib.depth"] = vibrato;
  preset["vib.rate"] = rate;
  preset["vib.delay"] = delay;
  preset["voice.tune"] = 0;
  preset["voice.mode"] = mode;
  return preset;
}

std::string FormatTime(int value)
{
  const unsigned ms = vocal::TimeMs(value);
  return ms >= 1000 ? std::to_string(ms / 1000) + "S" : std::to_string(ms) + "MS";
}

}  // namespace

VocalSynthEngine::VocalSynthEngine()
    : PagedSynthEngine({SynthEngineKind::Vocal, "vocal", "Vocal"}), pack_(vocal::Pack::Builtin())
{
  ReserveParameters(10);
  RegisterParameter({"amp.level", "Level", "Amp", SynthParameterKind::Continuous, 0, 127, 1, 96, 96});
  RegisterParameter({"env.attack", "Attack", "Amp", SynthParameterKind::Continuous, 0, 127, 1, 8, 8});
  RegisterParameter({"env.release", "Release", "Amp", SynthParameterKind::Continuous, 0, 127, 1, 22, 22});
  RegisterParameter({"voice.glide", "Glide", "Sing", SynthParameterKind::Continuous, 0, 127, 1, 25, 25});
  RegisterParameter({"vib.depth", "Vibrato", "Sing", SynthParameterKind::Continuous, 0, 127, 1, 0, 0});
  RegisterParameter({"vib.rate", "Vibrato Rate", "Sing", SynthParameterKind::Continuous, 0, 127, 1, 64, 64});
  RegisterParameter({"vib.delay", "Vibrato Delay", "Sing", SynthParameterKind::Continuous, 0, 127, 1, 30, 30});
  RegisterParameter({"voice.tune", "Tune", "Sing", SynthParameterKind::Continuous, -24, 24, 1, 0, 0});
  RegisterParameter({"voice.mode", "Mode", "Sing", SynthParameterKind::Choice, 0, 1, 1, 0, 0});
  RegisterParameter({"voice.word", "Word", "Sing", SynthParameterKind::Choice, 0, static_cast<int>(vocal::kMaxEntries) - 1, 1, 0, 0});
  RegisterTrackFxParameters();

  //                                              level atk rel glide vib rate delay mode
  RegisterPreset("vocal.plain", "Plain", VocalPreset(96, 8, 22, 25, 0, 64, 30, 0));
  RegisterPreset("vocal.vibrato", "Vibrato", VocalPreset(96, 10, 30, 30, 34, 60, 28, 0));
  RegisterPreset("vocal.diva", "Diva", VocalPreset(100, 14, 44, 44, 52, 56, 40, 0));
  RegisterPreset("vocal.robot", "Robot", VocalPreset(96, 4, 12, 70, 0, 64, 0, 0));
  RegisterPreset("vocal.short", "Staccato", VocalPreset(104, 2, 8, 0, 0, 64, 0, 0));
  RegisterPreset("vocal.choir", "Choir", VocalPreset(84, 40, 64, 0, 22, 50, 20, 1));
  RegisterPreset("vocal.pad", "Soft Pad", VocalPreset(80, 72, 90, 0, 14, 44, 60, 1));

  (void)LoadPreset("vocal.plain");
  SetWord(static_cast<int>(pack_->DefaultEntry()));
}

void VocalSynthEngine::SetPack(vocal::PackRef pack, std::string fileName)
{
  if (!pack)
  {
    pack = vocal::Pack::Builtin();
    fileName.clear();
  }
  pack_ = std::move(pack);
  packFileName_ = std::move(fileName);
  if (Word() >= static_cast<int>(pack_->EntryCount())) SetWord(static_cast<int>(pack_->DefaultEntry()));
  if (EngineIndex() != static_cast<std::size_t>(-1)) audio::SetVocalPack(EngineIndex(), pack_);
}

int VocalSynthEngine::Word() const
{
  SynthParameterInfo info;
  return GetParameterInfo("voice.word", info) ? info.value : 0;
}

void VocalSynthEngine::SetWord(int word)
{
  (void)SetParameterValue("voice.word", std::clamp(word, 0, static_cast<int>(pack_->EntryCount()) - 1));
}

std::string VocalSynthEngine::WordName(int word) const
{
  return word >= 0 && word < static_cast<int>(pack_->EntryCount()) ? std::string(pack_->EntryNameAt(static_cast<unsigned>(word))) : std::string("?");
}

bool VocalSynthEngine::AdjustParameterValue(const std::string& parameterId, int delta)
{
  if (parameterId != "voice.word") return PagedSynthEngine::AdjustParameterValue(parameterId, delta);
  // The parameter's range is the largest a pack may have; this pack ends sooner.
  return SetParameterValue(parameterId, std::clamp(Word() + delta, 0, static_cast<int>(pack_->EntryCount()) - 1));
}

std::string VocalSynthEngine::FormatParameterValue(const SynthParameterInfo& parameter) const
{
  if (parameter.id == "voice.word") return WordName(parameter.value);
  if (parameter.id == "env.attack" || parameter.id == "env.release") return FormatTime(parameter.value);
  if (parameter.id == "voice.glide") return parameter.value == 0 ? "OFF" : FormatTime(parameter.value);
  if (parameter.id == "vib.depth") return parameter.value == 0 ? "OFF" : std::to_string(parameter.value);
  if (parameter.id == "vib.delay") return std::to_string(vocal::VibratoDelayMs(parameter.value)) + "MS";
  if (parameter.id == "voice.tune") return parameter.value > 0 ? "+" + std::to_string(parameter.value) : std::to_string(parameter.value);
  if (parameter.id == "voice.mode") return parameter.value == 0 ? "MONO" : "POLY";
  if (parameter.id == "amp.level") return std::to_string((std::clamp(parameter.value, 0, 127) * 100) / 127) + "%";
  return PagedSynthEngine::FormatParameterValue(parameter);
}

bool VocalSynthEngine::ShouldSyncParameterOnPresetLoad(const SynthParameterInfo& parameter) const
{
  // The runtime knows no vocal presets: it takes every value, as the sampler's does.
  (void)parameter;
  return true;
}

bool VocalSynthEngine::PresetCarries(const SynthParameterInfo& parameter) const
{
  // A word is a place in one singer's list: it belongs to the track and its pack, not to a preset.
  return parameter.id != "voice.word";
}

void VocalSynthEngine::OnRuntimeSynced()
{
  // A track that just became a vocal one starts without a pack in the runtime.
  audio::SetVocalPack(EngineIndex(), pack_);
}

}  // namespace synth
