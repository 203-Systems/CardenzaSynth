#pragma once

#include <cstdint>
#include <string>

#include "Applications/Synth/Runtime/ProjectStorage.h"

namespace synth {

struct SessionInfo {
  bool dirty = false;
  std::string name;
  // Counts every autosave: at a start the newer of two copies has the higher number (0: written
  // before there was one).
  std::uint32_t generation = 0;
};

// The one autosave, on the card (the app's Session folder): a project file and a small text with the
// generation, the name and whether it was unsaved. Internal flash is never written (the user's
// decision, 2026-11-05: it is the Launcher's and every firmware's, and a write stops both cores for
// some 45 ms, a click and a row of keys flashing): without a card nothing outlives the power. Older
// firmware kept the autosave in internal flash, in small binary records (one per part of a project);
// that copy is read once, moved to the card and erased.
class SessionStore {
public:
  // Thread-safe: the app writes it on the device's background task.
  static bool SaveCard(const SessionInfo& info, const ProjectData& project);
  // Only the text: false when there is no card copy.
  static bool ReadCardInfo(SessionInfo& info);
  static bool LoadCard(SessionInfo& info, ProjectData& project);

  // An older firmware's autosave in internal flash: whether there is one (read or not), reading it, and
  // erasing every record of it (an erase stalls both cores: at a quiet moment, once the card has it).
  static bool InFlash();
  static bool LoadFlash(SessionInfo& info, ProjectData& project);
  static void EraseFlash();

  // Counts the starts begun with this autosave and not finished (on the card), so a state that crashes
  // the start is not tried forever. False: skip it this time.
  static bool BeginRestore();
  static void FinishRestore();
};

}  // namespace synth
