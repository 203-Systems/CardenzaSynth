#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "Applications/Synth/Runtime/ProjectStorage.h"

namespace synth {

struct SessionInfo {
  bool dirty = false;
  std::string name;
  // Counts every autosave, in flash or on the card: at a start the one with the higher number is the
  // newer (0: written before there was one).
  std::uint32_t generation = 0;
};

// The one autosave: the working state in small records in internal flash, there with or without a
// card, for a power cut at any moment. Each part of a project has its own record (general settings,
// song, each track's sound and sequence, each pattern), in a compact binary form, and only a part that
// changed is written: a typical edit costs some hundred bytes. A sound only keeps what differs from
// its preset. Each record is replaced as a whole, so a cut leaves the old one or the new one.
// Flash writes can stall both cores for a moment, so the caller picks a quiet moment to call Save.
class SessionStore {
public:
  enum class SaveResult { Ok, Partial, Failed };

  // Reads the autosave back. False when there is none, or it does not make sense.
  bool Load(SessionInfo& info, ProjectData& project);
  // Partial: the project is larger than the room there is, and the patterns the track are not playing
  // were the first to stay out.
  SaveResult Save(const SessionInfo& info, const ProjectData& project);

  // Counts the starts begun with this autosave and not finished, so a state that crashes the start is
  // not tried forever. False: skip it this time.
  static bool BeginRestore();
  static void FinishRestore();
  static void EraseUnknownRecords();

  static constexpr std::size_t kSectionCount = 2 + 2 * kSynthTrackCount + kSynthTrackCount * kSequencerMaxPatternCount;

  // The autosave while playing: on the card (the app's Session folder) instead of the internal flash,
  // whose writes stop both cores for some 45 ms, a click and a late beat. A project file and a small
  // text with the generation, the name and whether it was unsaved. Thread-safe: SaveCard runs on the
  // device's background task.
  static bool SaveCard(const SessionInfo& info, const ProjectData& project);
  // Only the text: false when there is no card copy.
  static bool ReadCardInfo(SessionInfo& info);
  // Whether the flash holds an autosave at all (read or not).
  static bool InFlash();
  static bool LoadCard(SessionInfo& info, ProjectData& project);

private:
  bool Store(std::size_t section, const std::string& bytes);

  // What each record holds now, 0 = no record. Nothing is written twice.
  std::array<std::uint32_t, kSectionCount> stored_{};
};

}  // namespace synth
