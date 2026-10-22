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
};

struct SequencerTrackUiState {
  bool enabled = true;
  bool muted = false;
  std::size_t activePattern = 0;
  std::size_t patternCount = 1;
  std::size_t populatedStepCount = 0;
};

struct SequencerUiState {
  bool visible = false;
  bool playing = false;
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
  std::array<SequencerTrackUiState, kSynthTrackCount> tracks{};
  std::uint16_t bpm = 120;
  bool trackEnabled = true;
  bool trackMuted = false;
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
