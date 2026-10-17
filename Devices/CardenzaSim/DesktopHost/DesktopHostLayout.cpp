#include "DesktopHostLayout.h"

#include <sstream>

#include "Library/Synth/Core/SynthTypes.h"

namespace CardenzaSim::DesktopHost {
namespace {

struct KeyLegend {
  const char* primary;
  const char* secondary;
};

constexpr std::array<std::array<KeyLegend, kKeyboardColumns>, kKeyboardRows> kKeyLegends = {{
    {{{"esc", ""}, {"1", "!"}, {"2", "@"}, {"3", "#"}, {"4", "$"}, {"5", "%"}, {"6", "^"},
      {"7", "&"}, {"8", "*"}, {"9", "("}, {"0", ")"}, {"-", "_"}, {"=", "+"}, {"del", ""}}},
    {{{"tab", ""}, {"Q", ""}, {"W", ""}, {"E", ""}, {"R", ""}, {"T", ""}, {"Y", ""},
      {"U", ""}, {"I", ""}, {"O", ""}, {"P", ""}, {"[", "{"}, {"]", "}"}, {"\\", "|"}}},
    {{{"fn", ""}, {"Aa", ""}, {"A", ""}, {"S", ""}, {"D", ""}, {"F", ""}, {"G", ""},
      {"H", ""}, {"J", ""}, {"K", ""}, {"L", ""}, {";", ":"}, {"'", "\""}, {"ok", "enter"}}},
    {{{"ctrl", ""}, {"opt", ""}, {"alt", ""}, {"Z", ""}, {"X", ""}, {"C", ""}, {"V", ""},
      {"B", ""}, {"N", ""}, {"M", ""}, {",", "<"}, {".", ">"}, {"/", "?"}, {"space", ""}}},
}};

std::string NoteName(int midi)
{
  static constexpr const char* kNoteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  const int octave = midi / 12 - 1;
  std::ostringstream stream;
  stream << kNoteNames[midi % 12] << octave;
  return stream.str();
}

KeyVisual BuildKeyVisual(std::size_t index)
{
  KeyVisual key;
  key.row = static_cast<int>(index / kKeyboardColumns);
  key.column = static_cast<int>(index % kKeyboardColumns);
  const bool isPerformanceKey = key.column >= static_cast<int>(synth::kHomePerformanceFirstColumn) &&
                                key.column <= static_cast<int>(synth::kHomePerformanceLastColumn);
  key.midi = isPerformanceKey ? static_cast<int>(synth::HomePerformanceMidiNote(static_cast<std::size_t>(key.row),
                                                                                 static_cast<std::size_t>(key.column)))
                              : 48 + static_cast<int>(index);
  const auto& legend = kKeyLegends[key.row][key.column];
  key.note = isPerformanceKey ? NoteName(key.midi) : std::string{};
  key.primaryLabel = legend.primary;
  key.secondaryLabel = legend.secondary;
  return key;
}

}  // namespace

std::array<KeyVisual, kKeyboardRows * kKeyboardColumns> BuildKeyboard()
{
  std::array<KeyVisual, kKeyboardRows * kKeyboardColumns> keys{};
  for (std::size_t index = 0; index < keys.size(); ++index)
  {
    keys[index] = BuildKeyVisual(index);
  }
  return keys;
}

MatrixPosition PositionForIndex(std::size_t index)
{
  return MatrixPosition{index / kKeyboardColumns, index % kKeyboardColumns};
}

RECT MakeRect(int left, int top, int width, int height)
{
  RECT rect{};
  rect.left = left;
  rect.top = top;
  rect.right = left + width;
  rect.bottom = top + height;
  return rect;
}

bool PointInRect(const RECT& rect, POINT point)
{
  return point.x >= rect.left && point.x < rect.right && point.y >= rect.top && point.y < rect.bottom;
}

}  // namespace CardenzaSim::DesktopHost
