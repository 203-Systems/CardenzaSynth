#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace synth {

enum class SynthEngineKind {
  None,
  Dx7,
  Juno,
  Sampler,
  Drums = 4,
  // One sample chopped into sixteen slices: the pads play parts of the file, not pitches.
  Chop = 5,
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

// Four tracks: what this instrument plays, and what its mixing and voice budget were measured
// against. Six was one over what the sample streaming could carry with any polyphony to spare.
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
// The widest page a track can put on screen at once. A lane track (drums, chop) shows thirty-two: its
// steps live on the 4x8 pad block, one key a step. A melodic track keeps the GRID's sixteen, because
// its pads are a keyboard. The storage page (PatternData) stays sixteen either way - this is only how
// much of the pattern one screen shows.
constexpr std::size_t kSequencerMaxViewSteps = kSequencerStepCount * 2;
constexpr std::size_t kSequencerMaxPatternCount = 16;
constexpr std::uint16_t kSequencerPulsesPerQuarter = 96;
constexpr std::uint16_t kSequencerPulsesPerStep = 24;
constexpr std::uint8_t kSequencerMicrostepCount = 6;
constexpr std::uint16_t kSequencerPulsesPerMicrostep = kSequencerPulsesPerStep / kSequencerMicrostepCount;
// ROLL retriggers one step's note inside that step: 1 is off, 2 is two 32nds, 3 three
// 48ths (16th triplets), 4 four 64ths. Every count divides kSequencerPulsesPerStep, so the
// sub-hits land on whole pulses of the 96 PPQ clock.
constexpr std::uint8_t kSequencerMaxRoll = 4;
constexpr std::size_t kSequencerRollCount = kSequencerMaxRoll;
constexpr std::array<const char*, kSequencerRollCount> kSequencerRollNames{{"OFF", "X2", "X3", "X4"}};

constexpr std::uint16_t SequencerRollSubHitPulses(std::uint8_t roll)
{
  return static_cast<std::uint16_t>(kSequencerPulsesPerStep / (roll < 2 ? 1 : (roll > kSequencerMaxRoll ? kSequencerMaxRoll : roll)));
}
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
static_assert(kSequencerPulsesPerStep % kSequencerMaxRoll == 0, "A roll's sub-hits must land on whole sequencer pulses.");

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
