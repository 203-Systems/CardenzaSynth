#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "Library/Synth/Core/SynthTypes.h"

namespace synth {

enum class SequencerEditLane : std::uint8_t {
  Note,
  Length,
  Velocity,
  Micro,
};

// Song (arrangement) mode: an ordered list of rows, each naming one pattern per track and a
// length in bars. One bar is 16 sixteenth steps of the global clock, whatever a track's own
// step count.
inline constexpr std::size_t kSongMaxRows = 64;
inline constexpr std::uint8_t kSongMaxRepeats = 16;
inline constexpr std::size_t kSongBarSteps = 16;
inline constexpr std::uint32_t kSongBarPulses = static_cast<std::uint32_t>(kSongBarSteps) * kSequencerPulsesPerStep;

static_assert(kSynthTrackCount == 4, "SongRow lists one default pattern per track.");

struct SongRow {
  std::array<std::int8_t, kSynthTrackCount> patterns{{0, 0, 0, 0}};  // -1 = track silent in this row, else pattern index
  std::uint8_t repeats = 1;  // length in bars, 1..kSongMaxRepeats
};

inline bool operator==(const SongRow& left, const SongRow& right)
{
  return left.patterns == right.patterns && left.repeats == right.repeats;
}

inline bool operator!=(const SongRow& left, const SongRow& right)
{
  return !(left == right);
}

inline const char* SequencerEditLaneLabel(SequencerEditLane lane)
{
  switch (lane)
  {
    case SequencerEditLane::Note:
      return "NOTE";
    case SequencerEditLane::Length:
      return "LEN";
    case SequencerEditLane::Velocity:
      return "VEL";
    case SequencerEditLane::Micro:
      return "TIME";
    default:
      return "SEQ";
  }
}

struct SequencerStepUiState {
  bool hasEvents = false;
  int lowestNote = -1;
  std::size_t noteCount = 0;
  bool held = false;
  bool playing = false;
  bool scoped = false;
  std::uint32_t contentFingerprint = 0;
  float editFlash = 0.0f;
  // Highest velocity among the step's active notes; 0 when the step is empty.
  std::uint8_t velocity = 0;
};

struct SequencerTrackUiState {
  bool enabled = true;
  bool muted = false;
  std::size_t activePattern = 0;
  std::size_t patternCount = 1;
  std::size_t populatedStepCount = 0;
  // Step of this track's last processed pulse; each track loops at its own length.
  std::size_t currentStep = 0;
  std::size_t stepCount = kSequencerStepCount;
  // Pattern that becomes active at this track's next loop boundary;
  // kSequencerMaxPatternCount means nothing is queued.
  std::size_t queuedPattern = kSequencerMaxPatternCount;
  // The playing song row leaves this track out (-1); its mute/enable flags are untouched.
  bool songSilent = false;
};

struct SequencerUiState {
  bool visible = false;
  bool playing = false;
  bool recordArmed = false;
  bool hasHeldSteps = false;
  bool selectionLatched = false;
  std::size_t activeTrack = 0;
  std::size_t heldStepCount = 0;
  std::size_t primaryHeldStep = 0;
  std::size_t noteCount = 0;
  int scopedNote = -1;
  std::string noteNames;
  std::size_t populatedStepCount = 0;
  std::array<bool, kSequencerMaxPatternCount> patternHasEvents{};
  // Steps holding at least one note, per pattern of the shown track (0 past its pattern count).
  std::array<std::uint8_t, kSequencerMaxPatternCount> patternStepCounts{};
  std::array<SequencerTrackUiState, kSynthTrackCount> tracks{};
  // Song mode. songMode is the switch; songPlaying is true while playback follows the song
  // (a switch flipped while playing takes over at the next bar line).
  bool songMode = false;
  bool songPlaying = false;
  bool songLoop = true;
  std::size_t songLength = 0;
  // Row being played; while not following the song, the row the song will start from.
  std::size_t songRow = 0;
  // Bar within the playing row, and how far through the whole row playback is (0..1).
  std::size_t songBar = 0;
  float songRowProgress = 0.0f;
  // Row a pending jump lands on at the next bar line; kSongMaxRows when none is pending.
  std::size_t songCueRow = kSongMaxRows;
  std::uint16_t bpm = 120;
  std::uint8_t swing = 50;
  bool trackEnabled = true;
  bool trackMuted = false;
  // Current step and progress of the active track, measured against that track's step count.
  std::size_t currentStep = 0;
  std::size_t patternCount = 1;
  std::size_t activePattern = 0;
  float patternProgress = 0.0f;
  SequencerEditLane editLane = SequencerEditLane::Note;
  std::string scopeLabel = "ALL";
  std::string valueLabel;
  std::array<bool, kKeyboardMatrixRows> editLanePressed{};
  std::size_t editValueCount = 0;
  std::size_t editValueIndex = 0;
  bool editValueMixed = false;
  bool shiftActive = false;
  bool patternSelectorVisible = false;
  bool clearMode = false;
  bool copyMode = false;
  bool quantizeMode = false;
  std::array<bool, kKeyboardMatrixRows> shiftActionPressed{};
  std::array<bool, kSequencerMaxPatternCount> patternPressed{};
  bool hasCopySource = false;
  bool copySourceIsPattern = false;
  std::size_t copySourceIndex = 0;
  std::array<SequencerStepUiState, kSequencerStepCount> steps{};
};

}  // namespace synth
