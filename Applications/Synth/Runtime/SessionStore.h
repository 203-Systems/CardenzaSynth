#pragma once

#include <cstdint>
#include <string>

#include "Applications/Synth/Runtime/ProjectStorage.h"

namespace synth {

struct SessionInfo {
  bool dirty = false;
  std::string name;
  // Counts the autosaves written (in the log lines); 0 is never written, so a text without it is not
  // one.
  std::uint32_t generation = 0;
};

// The one autosave: the working state on the SD card, in the app's Session folder (nothing is kept in
// internal flash). autosave.ccproj is the project in the project file format, replaced atomically
// (SaveProjectFileAt: a cut leaves the old file or the new one); autosave.txt beside it says the
// generation, the name and whether it was unsaved. The project is written first, so a cut between the
// two leaves the new project under the name of the one before. Without a card there is none.
// Save is called from the device's background task as well as from the system loop: the caller lets
// one write at a time, has made the folder (storage::EnsureLayout) and reports a failure
// (os::storage::ReportCardIoError) from the system loop.
class SessionStore {
public:
  static bool Save(const SessionInfo& info, const ProjectData& project);
  // Only the text: false when there is no autosave.
  static bool ReadInfo(SessionInfo& info);
  static bool Load(SessionInfo& info, ProjectData& project);

  // Counts the starts begun with the autosave and not finished (a small file beside it), so a state
  // that crashes the start is not tried forever. False: skip it this time.
  static bool BeginRestore();
  static void FinishRestore();
};

}  // namespace synth
