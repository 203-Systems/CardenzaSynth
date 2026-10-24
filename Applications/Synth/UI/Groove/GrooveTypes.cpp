#include "Applications/Synth/UI/Groove/GrooveTypes.h"

#include <algorithm>
#include <string>

namespace groove {

namespace {

std::size_t ScaleNoteCount(std::uint16_t mask)
{
  std::size_t count = 0;
  for (std::size_t bit = 0; bit < 12; ++bit) count += (mask >> bit) & 1u;
  return std::max<std::size_t>(1, count);
}

int ScaleDegreeOffset(std::uint16_t mask, std::size_t degree)
{
  const std::size_t count = ScaleNoteCount(mask);
  std::size_t wanted = degree % count;
  for (int semitone = 0; semitone < 12; ++semitone)
  {
    if (((mask >> semitone) & 1u) == 0) continue;
    if (wanted == 0) return semitone;
    --wanted;
  }
  return 0;
}

}  // namespace

bool PadNote(const KeyboardSettings& settings, synth::SynthEngineKind kind, std::size_t row, std::size_t column, int& note)
{
  if (row >= kRows || column < kPadFirstColumn || column >= kPadFirstColumn + kPadColumns) return false;
  const std::size_t x = column - kPadFirstColumn;
  if (kind == synth::SynthEngineKind::Drums)
  {
    note = kDrumNotes[x];
    return true;
  }
  const std::size_t y = kRows - 1 - row;  // bottom row is lowest
  const int octaveShift = std::clamp(settings.octave, -3, 3) * 12;
  const int base = static_cast<int>(synth::kHomePerformanceBaseMidiNote) + octaveShift;

  switch (settings.layout)
  {
    case KeyboardLayout::Piano:
    {
      static constexpr std::array<int, 7> kWhite{{0, 2, 4, 5, 7, 9, 11}};
      static constexpr std::array<int, 7> kBlack{{-1, 1, 3, -1, 6, 8, 10}};
      const std::size_t key = x % kWhite.size();
      const int octave = static_cast<int>(y / 2 + x / kWhite.size()) * 12;
      const int offset = (y % 2) == 0 ? kWhite[key] : kBlack[key];
      if (offset < 0) return false;
      note = base + octave + offset;
      break;
    }
    case KeyboardLayout::Scale:
    {
      const std::uint16_t mask = kScales[std::min(settings.scale, kScales.size() - 1)].mask;
      const std::size_t count = ScaleNoteCount(mask);
      note = base + settings.root + static_cast<int>(y) * 12 + static_cast<int>(x / count) * 12 + ScaleDegreeOffset(mask, x);
      break;
    }
    case KeyboardLayout::Chromatic:
    default:
      note = base + settings.root + static_cast<int>(y * kPadColumns + x);
      break;
  }
  return note >= 0 && note <= 127;
}

std::string_view EngineShortName(synth::SynthEngineKind kind)
{
  switch (kind)
  {
    case synth::SynthEngineKind::Dx7: return "DX7";
    case synth::SynthEngineKind::Juno: return "JUNO";
    case synth::SynthEngineKind::Sampler: return "SAMPLER";
    case synth::SynthEngineKind::Drums: return "DRUMS";
    case synth::SynthEngineKind::None:
    default: return "---";
  }
}

std::string_view EngineCode(synth::SynthEngineKind kind)
{
  switch (kind)
  {
    case synth::SynthEngineKind::Dx7: return "DX7";
    case synth::SynthEngineKind::Juno: return "JUN";
    case synth::SynthEngineKind::Sampler: return "SMP";
    case synth::SynthEngineKind::Drums: return "DRM";
    case synth::SynthEngineKind::None:
    default: return "---";
  }
}

std::string_view DrumName(int note)
{
  for (std::size_t index = 0; index < kDrumNotes.size(); ++index)
    if (kDrumNotes[index] == note) return kDrumNames[index];
  return {};
}

std::string NoteName(int note)
{
  if (note < 0 || note > 127) return "--";
  return std::string(kNoteNames[static_cast<std::size_t>(note % 12)]) + std::to_string(note / 12 - 1);
}

}  // namespace groove
