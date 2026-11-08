#include "Library/Synth/Core/Engines/Chop/ChopSynthEngine.h"

#include <algorithm>
#include <map>
#include <string>

#include "Library/Synth/Audio/SynthFx.h"

namespace synth {

namespace {

std::string SliceNumber(std::size_t slice)
{
  const std::size_t number = slice + 1;
  return number < 10 ? "0" + std::to_string(number) : std::to_string(number);
}

std::string FormatPercent(int value)
{
  // One unit is a twentieth of a percent, so the readout keeps two decimals where it has to and drops
  // the ones that would only be zeros: a step always shows, and whole percentages stay short the way
  // the other pages write them.
  const int hundredths = std::clamp(value, 0, kChopEndOfFile) * 10000 / kChopEndOfFile;
  if (hundredths % 100 == 0) return std::to_string(hundredths / 100) + "%";
  if (hundredths % 10 == 0) return std::to_string(hundredths / 100) + "." + std::to_string((hundredths / 10) % 10) + "%";
  return std::to_string(hundredths / 100) + "." + std::to_string((hundredths % 100) / 10) + std::to_string(hundredths % 10) +
         "%";
}

// How many slices a chop actually plays: eight or sixteen, whatever the preset was cut for.
std::size_t ClampSliceCount(std::size_t count)
{
  return std::max<std::size_t>(kChopMinSlices, std::min<std::size_t>(count, kChopSliceCount));
}

// The slice a point parameter belongs to and which end of it, or kChopSliceCount when it names
// something else.
std::size_t PointOf(const std::string& parameterId, bool& end)
{
  for (std::size_t slice = 0; slice < kChopSliceCount; ++slice)
  {
    if (parameterId == ChopStartParameterId(slice))
    {
      end = false;
      return slice;
    }
    if (parameterId == ChopEndParameterId(slice))
    {
      end = true;
      return slice;
    }
  }
  return kChopSliceCount;
}

}  // namespace

std::string ChopStartParameterId(std::size_t slice) { return "slice." + SliceNumber(slice) + ".start"; }
std::string ChopEndParameterId(std::size_t slice) { return "slice." + SliceNumber(slice) + ".end"; }
std::string ChopMuteParameterId(std::size_t slice) { return "slice." + SliceNumber(slice) + ".mute"; }
std::string ChopCutoffParameterId(std::size_t slice) { return "slice." + SliceNumber(slice) + ".cutoff"; }
std::string ChopReleaseParameterId(std::size_t slice) { return "slice." + SliceNumber(slice) + ".release"; }
std::string ChopStartLabel(std::size_t slice) { return "S" + std::to_string(slice + 1) + " Start"; }

std::string ChopPointParameterId(std::size_t slice, bool end)
{
  return end ? ChopEndParameterId(slice) : ChopStartParameterId(slice);
}

int ChopDefaultStart(std::size_t slice, std::size_t count)
{
  // kChopEndOfFile is the whole file, so `count` equal slices step by kChopEndOfFile / count and each
  // starts where the last one ended. Integer division leaves the last slice a step longer, which it
  // already was.
  const std::size_t slices = ClampSliceCount(count);
  const int span = kChopEndOfFile / static_cast<int>(slices);
  return static_cast<int>(std::min(slice, slices - 1)) * span;
}

// A default end is where the next slice begins, which is where a contiguous chop stops; the last one
// runs to the end of the file.
int ChopDefaultEnd(std::size_t slice, std::size_t count)
{
  const std::size_t slices = ClampSliceCount(count);
  return std::min(slice + 1, slices) >= slices ? kChopEndOfFile : ChopDefaultStart(slice + 1, slices);
}

std::pair<int, int> ChopDefaultSlice(std::size_t slice)
{
  return {ChopDefaultStart(slice, kChopSliceCount), ChopDefaultEnd(slice, kChopSliceCount)};
}

std::array<std::uint16_t, kChopPointCount> ChopChainAfterMove(
    const std::array<std::uint16_t, kChopPointCount>& current, std::size_t index, int value)
{
  std::array<std::uint16_t, kChopPointCount> points = current;
  if (index >= points.size()) return points;

  points[index] = static_cast<std::uint16_t>(std::clamp(value, 0, kChopEndOfFile));
  // Pushed forward: each point behind sits at least where the one in front of it does, and moves only
  // when that one reaches it. One already further along does not move at all.
  for (std::size_t later = index + 1; later < points.size(); ++later)
  {
    if (points[later] >= points[later - 1]) break;
    points[later] = points[later - 1];
  }
  // Pushed back: the same trolleys the other way - each point in front sits at most where the one
  // behind it does, so pulling a slice's start back carries the slice before it.
  for (std::size_t earlier = index; earlier-- > 0;)
  {
    if (points[earlier] <= points[earlier + 1]) break;
    points[earlier] = points[earlier + 1];
  }
  return points;
}

ChopSynthEngine::ChopSynthEngine()
    : PagedSynthEngine({SynthEngineKind::Chop, "chop", "Chop"})
{
  ReserveParameters(kChopSliceCount * 5 + 8);
  std::map<std::string, int> preset;
  for (std::size_t slice = 0; slice < kChopSliceCount; ++slice)
  {
    const int start = ChopDefaultStart(slice, kChopSliceCount);
    const int end = ChopDefaultEnd(slice, kChopSliceCount);
    RegisterParameter({ChopStartParameterId(slice), ChopStartLabel(slice), "Slices", SynthParameterKind::Continuous, 0,
                       kChopEndOfFile, 1, start, start});
    RegisterParameter({ChopEndParameterId(slice), "S End", "Slices", SynthParameterKind::Continuous, 0,
                       kChopEndOfFile, 1, end, end});
    // Off by default: a slice cuts nothing and is cut by nothing until a group is given to it.
    RegisterParameter({ChopMuteParameterId(slice), "Mute Grp", "Slices", SynthParameterKind::Choice, 0,
                       kChopMuteGroups, 1, 0, 0});
    // Per slice, because a slice is what sounds: the engine-wide cutoff and release below are still
    // there for projects and presets written before a slice had a tone of its own, and nothing plays
    // them any more.
    // Wide open and as short a fade as the mapping allows: a slice is heard as it was recorded until
    // one of the two is turned down or up.
    RegisterParameter({ChopCutoffParameterId(slice), "Cutoff", "Slices", SynthParameterKind::Continuous, 0, 127, 1,
                       127, 127});
    RegisterParameter({ChopReleaseParameterId(slice), "Release", "Slices", SynthParameterKind::Continuous, 0, 127, 1,
                       20, 20});
    preset[ChopStartParameterId(slice)] = start;
    preset[ChopEndParameterId(slice)] = end;
    preset[ChopMuteParameterId(slice)] = 0;
    preset[ChopCutoffParameterId(slice)] = 127;
    preset[ChopReleaseParameterId(slice)] = 20;
  }
  // The trim: coarse by design (a hundredth of the file a step), with the whole file in view on the
  // page that moves it, so it never needs the magnifier.
  RegisterParameter({kChopSampleStartId, "Trim In", "Sample", SynthParameterKind::Continuous, 0, kChopEndOfFile,
                     kChopEndOfFile / 100, 0, 0});
  RegisterParameter({kChopSampleEndId, "Trim Out", "Sample", SynthParameterKind::Continuous, 0, kChopEndOfFile,
                     kChopEndOfFile / 100, kChopEndOfFile, kChopEndOfFile});
  preset[kChopSampleStartId] = 0;
  preset[kChopSampleEndId] = kChopEndOfFile;
  RegisterParameter({"chop.count", "Slices", "Slices", SynthParameterKind::Choice, static_cast<int>(kChopMinSlices),
                     static_cast<int>(kChopSliceCount), static_cast<int>(kChopMinSlices), static_cast<int>(kChopSliceCount),
                     static_cast<int>(kChopSliceCount)});
  preset["chop.count"] = static_cast<int>(kChopSliceCount);
  // One slice at a time by default, the way a chop has always played. Raised, a new slice is added to
  // the ones still sounding instead of cutting them, which is what a chopped break played as a drum
  // kit wants: two hits of the same pad, or a hit over a ringing one.
  RegisterParameter({"chop.voices", "Voices", "Slices", SynthParameterKind::Choice, 1, kChopMaxVoices, 1, 1, 1});
  preset["chop.voices"] = 1;
  // The slices play at the file's own speed, so there is no root or tune here: what the pads change
  // is which part of the file sounds, never how fast it runs. The tone lives on the slices.
  RegisterParameter({"sample.gain", "Gain", "Sample", SynthParameterKind::Continuous, 0, 127, 1, 96, 96});
  RegisterParameter({"env.release", "Release", "Envelope", SynthParameterKind::Continuous, 0, 127, 1, 6, 6});
  RegisterParameter({"filter.cutoff", "Cutoff", "Filter", SynthParameterKind::Continuous, 0, 127, 1, 110, 110});
  RegisterParameter({"amp.level", "Level", "Amp", SynthParameterKind::Continuous, 0, 127, 1, 96, 96});
  preset["sample.gain"] = 96;
  preset["env.release"] = 6;
  preset["filter.cutoff"] = 110;
  preset["amp.level"] = 96;
  RegisterTrackFxParameters();

  RegisterPreset("chop.sixteen", "16 Slices", preset);

  // A whole-file preset that thins the grid out: eight slices of double length, so the same file
  // can be played as a coarser chop without editing sixteen start points. The eight slices the pads
  // cannot reach wait at the last one, so the ladder stays ascending in both.
  std::map<std::string, int> wide = preset;
  wide["chop.count"] = static_cast<int>(kChopMinSlices);
  for (std::size_t slice = 0; slice < kChopSliceCount; ++slice)
  {
    // The eight slices the pads cannot reach wait at the end of the file, so the chain of points stays
    // ascending whether the preset plays eight slices or sixteen.
    const bool used = slice < kChopMinSlices;
    wide[ChopStartParameterId(slice)] = used ? ChopDefaultStart(slice, kChopMinSlices) : kChopEndOfFile;
    wide[ChopEndParameterId(slice)] = used ? ChopDefaultEnd(slice, kChopMinSlices) : kChopEndOfFile;
  }
  RegisterPreset("chop.eight", "8 Slices", wide);

  (void)LoadPreset("chop.sixteen");
}

int ChopSynthEngine::TrimPoint(bool end) const
{
  const SynthParameterInfo* parameter = FindParameter(end ? kChopSampleEndId : kChopSampleStartId);
  return parameter != nullptr ? parameter->value : (end ? kChopEndOfFile : 0);
}

void ChopSynthEngine::ClampChainToTrim()
{
  const int low = TrimPoint(false);
  const int high = TrimPoint(true);
  for (std::size_t slice = 0; slice < SliceCount(); ++slice)
  {
    for (const bool end : {false, true})
    {
      const std::string id = ChopPointParameterId(slice, end);
      const SynthParameterInfo* parameter = FindParameter(id);
      if (parameter == nullptr) continue;
      const int value = std::clamp(parameter->value, low, high);
      if (value != parameter->value) PagedSynthEngine::SetParameterValue(id, value);
    }
  }
}

bool ChopSynthEngine::SetParameterValue(const std::string& parameterId, int value)
{
  if (parameterId == kChopSampleStartId || parameterId == kChopSampleEndId)
  {
    // The trim itself: it cannot pass its other side, and whatever it moves over comes with it.
    const bool end = parameterId == kChopSampleEndId;
    const int other = TrimPoint(!end);
    const int clamped = end ? std::clamp(value, other, kChopEndOfFile) : std::clamp(value, 0, other);
    if (!PagedSynthEngine::SetParameterValue(parameterId, clamped)) return false;
    ClampChainToTrim();
    return true;
  }

  bool end = false;
  const std::size_t slice = PointOf(parameterId, end);
  if (slice >= kChopSliceCount)
  {
    const bool count = parameterId == "chop.count";
    if (!PagedSynthEngine::SetParameterValue(parameterId, value)) return false;
    // Eight slices back to sixteen (or the other way) brings a row of points into play that may be
    // outside the trim, or out of order: the chain is rebuilt for the new count.
    if (count) NormalizeChain();
    return true;
  }
  if (slice >= SliceCount())
  {
    // A slice the preset does not play: its points are written as they come, nothing to push.
    return PagedSynthEngine::SetParameterValue(parameterId, value);
  }

  // The whole chain is written, not just the value that was asked for: a point carries its neighbours
  // with it, and each of those is its own parameter as far as the runtime and the page know.
  std::array<std::uint16_t, kChopPointCount> current{};
  for (std::size_t index = 0; index < kChopSliceCount; ++index)
  {
    for (const bool isEnd : {false, true})
    {
      const SynthParameterInfo* parameter = FindParameter(ChopPointParameterId(index, isEnd));
      current[ChopPointIndex(index, isEnd)] =
          static_cast<std::uint16_t>(std::clamp(parameter != nullptr ? parameter->value : 0, 0, kChopEndOfFile));
    }
  }

  // A point cannot leave the trim either: the chain stops against it the way it stops against another
  // point.
  const int low = TrimPoint(false);
  const int high = TrimPoint(true);
  const auto moved = ChopChainAfterMove(current, ChopPointIndex(slice, end), std::clamp(value, low, high));
  for (std::size_t index = 0; index < kChopSliceCount; ++index)
  {
    for (const bool isEnd : {false, true})
    {
      const std::size_t point = ChopPointIndex(index, isEnd);
      const int clamped = std::clamp(static_cast<int>(moved[point]), low, high);
      if (clamped == current[point]) continue;
      if (index >= SliceCount()) continue;
      if (!PagedSynthEngine::SetParameterValue(ChopPointParameterId(index, isEnd), clamped)) return false;
    }
  }
  // A move with nowhere to go changes nothing, and that is not an error.
  return true;
}

bool ChopSynthEngine::RestoreState(const SynthEngineState& state)
{
  if (!PagedSynthEngine::RestoreState(state)) return false;
  MigrateScale(state.version);
  // A project written before a slice had a tone of its own: every slice takes the engine-wide cutoff
  // it used to play through, so it sounds no different after the change.
  if (state.values.find(ChopCutoffParameterId(0)) == state.values.end())
  {
    const auto legacy = state.values.find("filter.cutoff");
    const int cutoff = std::clamp(legacy != state.values.end() ? legacy->second : 110, 0, 127);
    for (std::size_t slice = 0; slice < kChopSliceCount; ++slice)
      PagedSynthEngine::SetParameterValue(ChopCutoffParameterId(slice), cutoff);
  }
  // A project written before a slice had an end of its own: cut each where the next one begins.
  // A file could hold a trim the wrong way round: the chain would have nowhere to live.
  const int trimIn = TrimPoint(false);
  if (trimIn > TrimPoint(true)) PagedSynthEngine::SetParameterValue(kChopSampleEndId, trimIn);
  if (state.values.find(ChopEndParameterId(0)) == state.values.end()) DeriveEnds();
  NormalizeChain();
  return true;
}

void ChopSynthEngine::OnPresetLoaded(const std::string& presetId)
{
  PagedSynthEngine::OnPresetLoaded(presetId);
  NormalizeChain();
}

void ChopSynthEngine::MigrateScale(int version)
{
  // A file written before the positions were refined holds them on the scale of its day: 0..127 in the
  // first builds, 0..1000 after that. Which of the two is read off the values themselves (anything past
  // 127 can only be the middle scale), and they are widened in place so a project still cuts its file
  // where it did. Files this build writes say so by their version and are left alone.
  if (version <= 0 || version >= kChopScaleProjectVersion) return;
  int highest = std::max(TrimPoint(false), TrimPoint(true));
  for (const bool end : {false, true})
  {
    for (std::size_t slice = 0; slice < kChopSliceCount; ++slice)
    {
      const SynthParameterInfo* parameter = FindParameter(ChopPointParameterId(slice, end));
      if (parameter != nullptr) highest = std::max(highest, parameter->value);
    }
  }
  const int from = highest > kChopOriginalEndOfFile ? kChopPreviousEndOfFile : kChopOriginalEndOfFile;
  const auto widen = [from](int value) {
    return std::clamp((value * kChopEndOfFile + from / 2) / from, 0, kChopEndOfFile);
  };
  for (const bool end : {false, true})
  {
    for (std::size_t slice = 0; slice < kChopSliceCount; ++slice)
    {
      const SynthParameterInfo* parameter = FindParameter(ChopPointParameterId(slice, end));
      if (parameter == nullptr) continue;
      PagedSynthEngine::SetParameterValue(ChopPointParameterId(slice, end), widen(parameter->value));
    }
  }
  PagedSynthEngine::SetParameterValue(kChopSampleStartId, widen(TrimPoint(false)));
  PagedSynthEngine::SetParameterValue(kChopSampleEndId, widen(TrimPoint(true)));
}

std::size_t ChopSynthEngine::SliceCount() const
{
  const SynthParameterInfo* parameter = FindParameter("chop.count");
  if (parameter == nullptr) return kChopSliceCount;
  return ClampSliceCount(static_cast<std::size_t>(std::max(0, parameter->value)));
}

void ChopSynthEngine::NormalizeChain()
{
  // One pass over the row of points, slice by slice: whatever came out of a file, the chain reads left
  // to right afterwards. A point is only written when it has to move.
  const std::size_t slices = SliceCount();
  const int low = TrimPoint(false);
  const int high = TrimPoint(true);
  int previous = low;
  for (std::size_t slice = 0; slice < slices; ++slice)
  {
    for (const bool end : {false, true})
    {
      const std::string id = ChopPointParameterId(slice, end);
      const SynthParameterInfo* parameter = FindParameter(id);
      if (parameter == nullptr) continue;
      const int value = std::clamp(parameter->value, previous, high);
      if (value != parameter->value && !PagedSynthEngine::SetParameterValue(id, value)) continue;
      previous = value;
    }
  }
}

void ChopSynthEngine::DeriveEnds()
{
  // Where the next slice begins, as the engine did before a slice could have an end of its own. The
  // starts are already in order here, so this leaves both rows ascending.
  const std::size_t slices = SliceCount();
  for (std::size_t slice = 0; slice < slices; ++slice)
  {
    const SynthParameterInfo* next = slice + 1 < slices ? FindParameter(ChopStartParameterId(slice + 1)) : nullptr;
    const int end = next != nullptr ? next->value : kChopEndOfFile;
    PagedSynthEngine::SetParameterValue(ChopEndParameterId(slice), std::clamp(end, 0, kChopEndOfFile));
  }
}

std::string ChopSynthEngine::FormatParameterValue(const SynthParameterInfo& parameter) const
{
  if (fx::IsTrackParameter(parameter.id)) return PagedSynthEngine::FormatParameterValue(parameter);
  if (parameter.id == "chop.count" || parameter.id == "chop.voices") return std::to_string(parameter.value);
  if (parameter.id.size() > 5 && parameter.id.rfind(".mute") == parameter.id.size() - 5)
  {
    if (parameter.value <= 0) return "OFF";
    return "G" + std::to_string(std::clamp(parameter.value, 0, kChopMuteGroups));
  }
  const bool sliceTone = parameter.id.size() > 7 &&
                         (parameter.id.rfind(".cutoff") == parameter.id.size() - 7 ||
                          parameter.id.rfind(".release") == parameter.id.size() - 8);
  if (sliceTone || parameter.id == "sample.gain" || parameter.id == "amp.level" || parameter.id == "env.release" ||
      parameter.id == "filter.cutoff")
  {
    // These are still the engine's 0..127 knobs; only the slice positions use the fine scale.
    return std::to_string((std::clamp(parameter.value, 0, 127) * 100) / 127) + "%";
  }
  // Start and end read as a position in the file, which is what they are.
  return FormatPercent(parameter.value);
}

}  // namespace synth
