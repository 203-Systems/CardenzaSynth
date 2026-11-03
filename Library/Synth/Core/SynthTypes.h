#pragma once

#include <cstddef>
#include <cstdint>

namespace synth {

enum class SynthEngineKind {
  None,
  Dx7,
  Juno,
  Sampler,
  Drums = 4,
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
// The track a fresh start, a new project and a loaded one open on: T1, the drum machine.
constexpr std::size_t kDefaultActiveTrack = 0;
constexpr std::size_t kKeyboardMatrixRows = 4;
constexpr std::size_t kKeyboardMatrixColumns = 14;
constexpr std::size_t kHomeLeftBarColumn = 0;
constexpr std::size_t kHomeRightBarColumn = kKeyboardMatrixColumns - 1;
constexpr std::size_t kHomePerformanceFirstColumn = kHomeLeftBarColumn + 1;
constexpr std::size_t kHomePerformanceColumnCount = 8;
constexpr std::size_t kHomePerformanceLastColumn = kHomePerformanceFirstColumn + kHomePerformanceColumnCount - 1;
constexpr std::size_t kSequencerStepFirstColumn = kHomePerformanceLastColumn + 1;
constexpr std::size_t kSequencerStepColumnCount = 4;
constexpr std::size_t kSequencerStepLastColumn = kSequencerStepFirstColumn + kSequencerStepColumnCount - 1;
// Steps on the GRID: one page of a pattern, and the default pattern length.
constexpr std::size_t kSequencerStepCount = kKeyboardMatrixRows * kSequencerStepColumnCount;
// A pattern is up to this long, shown and stored one page at a time.
constexpr std::size_t kSequencerMaxSteps = 256;
constexpr std::size_t kSequencerMaxStepPages = kSequencerMaxSteps / kSequencerStepCount;
constexpr std::size_t kSequencerMaxPatternCount = 16;
constexpr std::uint16_t kSequencerPulsesPerQuarter = 96;
constexpr std::uint16_t kSequencerPulsesPerStep = 24;
constexpr std::uint8_t kSequencerMicrostepCount = 6;
constexpr std::uint16_t kSequencerPulsesPerMicrostep = kSequencerPulsesPerStep / kSequencerMicrostepCount;
constexpr std::uint8_t kHomePerformanceBaseMidiNote = 48;
constexpr std::size_t kContextMenuRows = 4;
constexpr std::size_t kContextMenuColumns = 2;
constexpr std::size_t kContextMenuCellCount = kContextMenuRows * kContextMenuColumns;
constexpr std::size_t kContextMenuFirstColumn = kKeyboardMatrixColumns - kContextMenuColumns;
constexpr std::size_t kParamMatrixRows = 3;
constexpr std::size_t kParamMatrixColumns = 4;
constexpr std::size_t kParamMatrixCellCount = kParamMatrixRows * kParamMatrixColumns;

static_assert(kHomePerformanceColumnCount == 8, "Home performance zone should expose an 8-column keyboard.");
static_assert(kSequencerStepFirstColumn == 9, "Home sequencer should start at keyboard column 9.");
static_assert(kSequencerStepLastColumn == 12, "Home sequencer should end before the right bar.");
static_assert(kSequencerMaxPatternCount >= 1, "Sequencer needs at least one pattern.");
static_assert(kSequencerPulsesPerStep % kSequencerMicrostepCount == 0, "Microsteps must map to whole sequencer pulses.");

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

constexpr bool IsSequencerStepKey(std::size_t row, std::size_t col)
{
  return row < kKeyboardMatrixRows && col >= kSequencerStepFirstColumn && col <= kSequencerStepLastColumn;
}

constexpr std::size_t SequencerStepIndex(std::size_t row, std::size_t col)
{
  return row * kSequencerStepColumnCount + (col - kSequencerStepFirstColumn);
}

constexpr std::size_t SequencerStepRow(std::size_t stepIndex)
{
  return stepIndex / kSequencerStepColumnCount;
}

constexpr std::size_t SequencerStepColumn(std::size_t stepIndex)
{
  return kSequencerStepFirstColumn + (stepIndex % kSequencerStepColumnCount);
}

}  // namespace synth
