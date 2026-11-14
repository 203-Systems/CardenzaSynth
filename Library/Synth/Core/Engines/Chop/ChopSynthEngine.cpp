#include "Library/Synth/Core/Engines/Chop/ChopSynthEngine.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>

#include "Library/Synth/Audio/SynthFx.h"

namespace synth {

namespace {

constexpr std::array<const char*, 5> kFieldNames{{"start", "end", "fade", "cutoff", "mute"}};
constexpr std::array<const char*, 5> kFieldLabels{{"Start", "End", "Fade", "Cutoff", "Mute"}};
// The fade a slice gets unless it is given another: the shortest, a millisecond against a click.
constexpr int kDefaultFade = 0;

std::map<std::string, int> EvenPreset(std::size_t slices)
{
  std::map<std::string, int> preset;
  const ChopPoints points = ChopEvenPoints(0, kChopPositionMax, slices);
  for (std::size_t slice = 0; slice < kChopSliceCount; ++slice)
  {
    preset[ChopParameterId(slice, ChopField::Start)] = points[ChopPointIndex(slice, false)];
    preset[ChopParameterId(slice, ChopField::End)] = points[ChopPointIndex(slice, true)];
  }
  preset[kChopSlicesId] = slices > kChopFewSlices ? 1 : 0;
  return preset;
}

}  // namespace

ChopPoints ChopChainAfterMove(const ChopPoints& current, std::size_t index, int value)
{
  ChopPoints points = current;
  if (index >= points.size()) return points;
  points[index] = std::clamp(value, 0, kChopPositionMax);
  // Moved forward, it carries the points ahead of it that it reaches; moved back, those behind.
  for (std::size_t later = index + 1; later < points.size() && points[later] < points[later - 1]; ++later) points[later] = points[later - 1];
  for (std::size_t earlier = index; earlier-- > 0 && points[earlier] > points[earlier + 1];) points[earlier] = points[earlier + 1];
  return points;
}

ChopPoints ChopEvenPoints(int first, int last, std::size_t slices)
{
  first = std::clamp(first, 0, kChopPositionMax);
  last = std::clamp(last, first, kChopPositionMax);
  slices = std::clamp<std::size_t>(slices, 1, kChopSliceCount);
  ChopPoints points{};
  const long long span = last - first;
  for (std::size_t slice = 0; slice < kChopSliceCount; ++slice)
  {
    const bool used = slice < slices;
    points[ChopPointIndex(slice, false)] = used ? first + static_cast<int>(span * static_cast<long long>(slice) / static_cast<long long>(slices)) : last;
    points[ChopPointIndex(slice, true)] = used ? first + static_cast<int>(span * static_cast<long long>(slice + 1) / static_cast<long long>(slices)) : last;
  }
  return points;
}

std::string ChopParameterId(std::size_t slice, ChopField field)
{
  char id[24];
  std::snprintf(id, sizeof(id), "slice.%02u.%s", static_cast<unsigned>(slice + 1), kFieldNames[static_cast<std::size_t>(field)]);
  return id;
}

namespace {

bool ParseField(std::string_view name, ChopField& field)
{
  for (std::size_t index = 0; index < kFieldNames.size(); ++index)
    if (name == kFieldNames[index])
    {
      field = static_cast<ChopField>(index);
      return true;
    }
  return false;
}

}  // namespace

bool ParseChopParameterId(std::string_view id, std::size_t& slice, ChopField& field)
{
  // "slice." two digits "." field
  if (id.size() < 10 || id.substr(0, 6) != "slice." || id[8] != '.') return false;
  const char tens = id[6];
  const char ones = id[7];
  if (tens < '0' || tens > '9' || ones < '0' || ones > '9') return false;
  const std::size_t number = static_cast<std::size_t>((tens - '0') * 10 + (ones - '0'));
  if (number < 1 || number > kChopSliceCount || !ParseField(id.substr(9), field)) return false;
  slice = number - 1;
  return true;
}

bool ParseChopFieldAlias(std::string_view id, ChopField& field)
{
  return id.size() > 6 && id.substr(0, 6) == "slice." && ParseField(id.substr(6), field);
}

float ChopFadeSeconds(int fade)
{
  return 0.001f * std::pow(2000.0f, static_cast<float>(std::clamp(fade, 0, 127)) / 127.0f);
}

ChopSynthEngine::ChopSynthEngine()
    : PagedSynthEngine({SynthEngineKind::Chop, "chop", "Chop"})
{
  ReserveParameters(kChopSliceCount * kFieldNames.size() + 3);
  const ChopPoints even = ChopEvenPoints(0, kChopPositionMax, kChopSliceCount);
  for (std::size_t slice = 0; slice < kChopSliceCount; ++slice)
  {
    const int start = even[ChopPointIndex(slice, false)];
    const int end = even[ChopPointIndex(slice, true)];
    // Labels name the field only: the page shows the slice, and the labels stay short.
    RegisterParameter({ChopParameterId(slice, ChopField::Start), kFieldLabels[0], "Slices", SynthParameterKind::Continuous, 0, kChopPositionMax, 1,
                       start, start});
    RegisterParameter({ChopParameterId(slice, ChopField::End), kFieldLabels[1], "Slices", SynthParameterKind::Continuous, 0, kChopPositionMax, 1,
                       end, end});
    RegisterParameter({ChopParameterId(slice, ChopField::Fade), kFieldLabels[2], "Slices", SynthParameterKind::Continuous, 0, 127, 1,
                       kDefaultFade, kDefaultFade});
    RegisterParameter({ChopParameterId(slice, ChopField::Cutoff), kFieldLabels[3], "Slices", SynthParameterKind::Continuous, 0, 127, 1,
                       kChopCutoffOpen, kChopCutoffOpen});
    RegisterParameter({ChopParameterId(slice, ChopField::Mute), kFieldLabels[4], "Slices", SynthParameterKind::Choice, 0, kChopMuteGroups, 1, 0, 0});
  }
  RegisterParameter({kChopSlicesId, "Slices", "Chop", SynthParameterKind::Choice, 0, 1, 1, 1, 1});
  // One slice at a time, the way a chopped break is played by hand: the next one cuts it.
  RegisterParameter({kChopVoicesId, "Voices", "Chop", SynthParameterKind::Choice, 1, kChopMaxVoices, 1, 1, 1});
  RegisterParameter({kChopGainId, "Gain", "Chop", SynthParameterKind::Continuous, 0, 127, 1, 96, 96});
  RegisterTrackFxParameters();

  RegisterPreset("chop.16", "16 Slices", EvenPreset(kChopSliceCount));
  RegisterPreset("chop.8", "8 Slices", EvenPreset(kChopFewSlices));
  (void)LoadPreset("chop.16");
}

ChopPoints ChopSynthEngine::Points() const
{
  ChopPoints points{};
  for (std::size_t slice = 0; slice < kChopSliceCount; ++slice)
    for (const bool end : {false, true})
    {
      const SynthParameterInfo* parameter = FindParameter(ChopParameterId(slice, end ? ChopField::End : ChopField::Start));
      points[ChopPointIndex(slice, end)] = parameter != nullptr ? parameter->value : 0;
    }
  return points;
}

void ChopSynthEngine::ApplyPoints(const ChopPoints& points)
{
  const ChopPoints current = Points();
  for (std::size_t slice = 0; slice < kChopSliceCount; ++slice)
    for (const bool end : {false, true})
    {
      const std::size_t index = ChopPointIndex(slice, end);
      if (points[index] != current[index])
        (void)PagedSynthEngine::SetParameterValue(ChopParameterId(slice, end ? ChopField::End : ChopField::Start), points[index]);
    }
}

bool ChopSynthEngine::SetParameterValue(const std::string& parameterId, int value)
{
  std::size_t slice = 0;
  ChopField field = ChopField::Start;
  if (!ParseChopParameterId(parameterId, slice, field) || (field != ChopField::Start && field != ChopField::End))
    return PagedSynthEngine::SetParameterValue(parameterId, value);
  ApplyPoints(ChopChainAfterMove(Points(), ChopPointIndex(slice, field == ChopField::End), value));
  return true;
}

bool ChopSynthEngine::RestoreState(const SynthEngineState& state)
{
  if (!PagedSynthEngine::RestoreState(state)) return false;
  // Points written by this firmware are in order already; a file edited by hand may not be.
  ChopPoints points = Points();
  for (std::size_t index = 1; index < points.size(); ++index) points[index] = std::max(points[index], points[index - 1]);
  ApplyPoints(points);
  return true;
}

bool ChopSynthEngine::ShouldSyncParameterOnPresetLoad(const SynthParameterInfo& parameter) const
{
  return ShouldSyncParameterToRuntime(parameter);
}

std::string ChopSynthEngine::FormatParameterValue(const SynthParameterInfo& parameter) const
{
  if (fx::IsTrackParameter(parameter.id)) return PagedSynthEngine::FormatParameterValue(parameter);
  if (parameter.id == kChopSlicesId) return parameter.value != 0 ? "16" : "8";
  if (parameter.id == kChopVoicesId) return std::to_string(parameter.value);
  if (parameter.id == kChopGainId) return std::to_string(std::clamp(parameter.value, 0, 127) * 100 / 127) + "%";
  std::size_t slice = 0;
  ChopField field = ChopField::Start;
  if (!ParseChopParameterId(parameter.id, slice, field)) return PagedSynthEngine::FormatParameterValue(parameter);
  char text[32];
  switch (field)
  {
    case ChopField::Start:
    case ChopField::End:
      // Of the file; the page says the time when it knows the sample.
      std::snprintf(text, sizeof(text), "%d.%02d%%", parameter.value / 100, parameter.value % 100);
      return text;
    case ChopField::Fade:
    {
      const float seconds = ChopFadeSeconds(parameter.value);
      const long hundredths = std::lround(seconds * 100.0f);
      if (seconds < 0.1f) std::snprintf(text, sizeof(text), "%ldMS", std::lround(seconds * 1000.0f));
      else std::snprintf(text, sizeof(text), "%ld.%02ldS", hundredths / 100, hundredths % 100);
      return text;
    }
    case ChopField::Cutoff:
      if (parameter.value >= kChopCutoffOpen) return "OPEN";
      return std::to_string(parameter.value * 100 / 127) + "%";
    case ChopField::Mute:
      return parameter.value <= 0 ? "OFF" : "G" + std::to_string(parameter.value);
  }
  return std::to_string(parameter.value);
}

}  // namespace synth
