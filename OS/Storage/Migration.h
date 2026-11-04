#pragma once

#include <cstddef>
#include <string>

#include "OS/Storage/Storage.h"

namespace os::storage {

struct MigrationResult {
  bool found = false;        // a pre-protocol folder was on the card
  std::size_t moved = 0;     // files now in their protocol folder
  std::size_t duplicates = 0;  // identical copies were already in place; the old copy was removed
  std::size_t conflicts = 0;   // a different file of the same name was in place; both are kept
  std::size_t failed = 0;      // could not be moved; left where they were
  bool Complete() const { return conflicts == 0 && failed == 0; }
};

// The card layout before the storage protocol: <card>/CardenzaSynth/{Project,Preset,Sample}.
std::string LegacyRoot();

// Moves the pre-protocol layout into protocol 1: Project/ -> <app>/Projects, Preset/<engine>/ ->
// <app>/Presets/<engine>/, Sample/ -> Shared/Samples (projects name samples by file name, so they
// keep resolving). Folders are renamed in one step when the target does not exist yet; otherwise
// files move one by one (rename, or copy + verify + remove when rename fails). Nothing is ever
// overwritten and a file that cannot be moved stays where it was. Running it again only finishes
// what is left, so it is idempotent. The result is recorded in the system settings
// (migrate.legacy = done | partial, migrate.left = files left in the old folder).
MigrationResult MigrateLegacyLayout(const AppFolder& app);

}  // namespace os::storage
