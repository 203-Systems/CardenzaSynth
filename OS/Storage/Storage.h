#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// The storage protocol, version 2 (docs/app-storage-protocol.md): the layout the 0.1.0 release
// wrote, on the SD card only. Nothing is kept in internal flash. Apps reach the card only through
// this API:
//
//   <card>/CardenzaSynth/
//     Settings/system.cfg     device settings (os::storage::SystemSettings)
//     Settings/synth.cfg      the groovebox's settings (os::storage::AppSettings("Cardenza/Synth"))
//     Projects/ Presets/<engine>/ Session/   the groovebox's documents and working state
//     Samples/                audio any app may read and write
//     Logs/                   the console log (OS/System/Log.h)
//     Apps/<Author>/<Name>/   any other app, created when it needs it
//
// The primary app (Cardenza/Synth) uses the product root itself. FAT long names are required. The
// device layer (Device::Storage) only provides the card root, mounting and capacity.
namespace os::storage {

inline constexpr int kProtocolVersion = 2;
inline constexpr std::string_view kPrimaryAppId = "Cardenza/Synth";

// <card>/CardenzaSynth and its folders. SystemDirectory is Settings/; SharedDirectory is the root.
std::string Root();
std::string SystemDirectory();
std::string SystemSettingsPath();
std::string SharedDirectory();
std::string SharedSamplesDirectory();
std::string AppsDirectory();
// The console log (OS/System/Log.h): log.txt, and the one before as log.1.txt.
std::string LogDirectory();

// App ids are "Author/Name"; both parts are 1..24 characters of [A-Za-z0-9_-] because they become
// folder names.
bool IsValidAppId(std::string_view id);

// The folders of one app. Paths are computed without touching the card.
class AppFolder {
public:
  AppFolder() = default;
  // An invalid id gives an invalid folder whose paths are all empty.
  explicit AppFolder(std::string_view id);

  bool Valid() const { return !id_.empty(); }
  const std::string& Id() const { return id_; }

  std::string Root() const;          // the product root for the primary app; Apps/<Author>/<Name> otherwise
  std::string SettingsPath() const;  // Settings/synth.cfg for the primary app; .../Settings/app.cfg otherwise
  std::string Projects() const;      // .../Projects
  std::string Presets() const;       // .../Presets
  std::string Presets(std::string_view group) const;  // .../Presets/<group>
  std::string Session() const;       // .../Session (the working state, kept across app switches and restarts)
  // An app-specific folder: one path element of [A-Za-z0-9_-.], not "." or "..".
  std::string Folder(std::string_view name) const;

private:
  std::string id_;
};

// Card state. CardPresent() asks the device to mount the card when it is not mounted yet (the
// device decides how often a missing card is probed).
bool CardPresent();
// The same without asking the device to mount it: cheap enough to call often.
bool CardMounted();
// An app reading from the card in real time (a sample played from its file) says so: what writes to
// the card only to keep a record (the log, the autosave) waits meanwhile. A card writing is slow to
// read from, for seconds at a time, and the reader waiting for it starved the UI (the stress test,
// 2026-11-04).
void SetCardStreaming(bool streaming);
bool CardStreaming();
// After a file operation failed (file closed): lets the device notice a card that stopped responding.
// A card that still answers (a full card, a bad file) stays mounted.
void ReportCardIoError();
std::uint64_t CardCapacityBytes();
std::uint64_t CardFreeBytes();

// Creates CardenzaSynth/, Settings/ and Samples/. False without a writable card.
bool EnsureSystemLayout();
// The system layout plus the app's root, its Settings/, Projects/, Presets/, Session/ and one
// Presets/<group>/ per entry of `presetGroups`.
bool EnsureAppLayout(const AppFolder& app, const std::vector<std::string>& presetGroups = {});

// Portable file helpers: FAT through the ESP-IDF VFS on the device, the host file system in the
// simulator.
std::string JoinPath(std::string_view directory, std::string_view name);
bool Exists(const std::string& path);
bool IsDirectory(const std::string& path);
bool IsFile(const std::string& path);
// Size of a regular file, or -1.
long long FileSize(const std::string& path);
// mkdir -p.
bool CreateDirectories(const std::string& path);
// Removes an empty directory.
bool RemoveEmptyDirectory(const std::string& path);

struct DirectoryEntry {
  std::string name;
  bool directory = false;
};
// Entries of `path` without "." and "..", sorted by name. Empty when the folder is missing.
std::vector<DirectoryEntry> ListDirectory(const std::string& path);

// Writes `contents` to `path` without ever leaving it half written: <path>.tmp is written and
// closed first; an existing file stays as <path>.bak until the new one is in place, because FAT and
// Windows cannot rename over a file. The parent folder must exist. A failure is reported to the
// device (ReportCardIoError).
bool WriteFileAtomic(const std::string& path, std::string_view contents);
// Reads `path`, or <path>.bak when an interrupted write left only the backup. Fails for files over
// `maxBytes`.
bool ReadFileWithBackup(const std::string& path, std::string& contents, std::size_t maxBytes);
// Removes a file. False when it could not be removed (a missing file is not an error).
bool RemoveFile(const std::string& path);

}  // namespace os::storage
