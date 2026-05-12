#pragma once

#include <cstddef>
#include <cstdint>

namespace synth {

enum class SynthEngineKind {
  None,
  Dx7,
  Juno,
  Sampler,
};

enum class OverlayPage {
  None,
  Tone,
  Parameter,
  Layout,
  Other,
};

enum class UiMode {
  Home,
  ContextMenu,
};

enum class UiModifier {
  None,
  Sound,
  Parameter,
  Layout,
  Other,
};

constexpr std::size_t kSynthTrackCount = 4;
constexpr std::size_t kKeyboardMatrixRows = 4;
constexpr std::size_t kKeyboardMatrixColumns = 14;
constexpr std::size_t kHomeLeftBarColumn = 0;
constexpr std::size_t kHomeRightBarColumn = kKeyboardMatrixColumns - 1;
constexpr std::size_t kHomePerformanceFirstColumn = kHomeLeftBarColumn + 1;
constexpr std::size_t kHomePerformanceColumnCount = 8;
constexpr std::size_t kHomePerformanceLastColumn = kHomePerformanceFirstColumn + kHomePerformanceColumnCount - 1;
constexpr std::uint8_t kHomePerformanceBaseMidiNote = 48;
constexpr std::size_t kContextMenuRows = 4;
constexpr std::size_t kContextMenuColumns = 2;
constexpr std::size_t kContextMenuCellCount = kContextMenuRows * kContextMenuColumns;
constexpr std::size_t kContextMenuFirstColumn = kKeyboardMatrixColumns - kContextMenuColumns;
constexpr std::size_t kParamMatrixRows = 3;
constexpr std::size_t kParamMatrixColumns = 4;
constexpr std::size_t kParamMatrixCellCount = kParamMatrixRows * kParamMatrixColumns;

static_assert(kHomePerformanceColumnCount == 8, "Home performance zone should expose an 8-column keyboard.");

constexpr std::size_t HomePerformanceOctaveIndex(std::size_t row)
{
  return (kKeyboardMatrixRows - 1) - row;
}

constexpr std::uint8_t HomePerformanceMidiNote(std::size_t row, std::size_t col)
{
  return static_cast<std::uint8_t>(kHomePerformanceBaseMidiNote +
                                   HomePerformanceOctaveIndex(row) * kHomePerformanceColumnCount +
                                   (col - kHomePerformanceFirstColumn));
}

}  // namespace synth
