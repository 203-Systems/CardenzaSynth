#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// The Cardenza app storage protocol, version 1 (docs/app-storage-protocol.md). Every app, built in
// or user-made, keeps its files in its own folder on the SD card and reaches the card only through
// this API:
//
//   <card>/Cardenza/
//     System/settings.cfg      device settings (os::storage::SystemSettings)
//     Shared/Samples/          audio any app may read and write
//     Apps/<Author>/<Name>/    one folder per app id "Author/Name"
//       settings.cfg           the app's settings (os::storage::AppSettings)
//       Projects/ Presets/<group>/ Session/ and any folder of the app's own
//
// Folder names are 8.3-safe. The device layer (Device::Storage) only provides the card root, the
// mount state and a small internal-flash record store used while the card is missing.
namespace os::storage {

inline constexpr int kProtocolVersion = 1;

// <card>/Cardenza and its system folders.
std::string Root();
std::string SystemDirectory();
std::string SystemSettingsPath();
std::string SharedDirectory();
std::string SharedSamplesDirectory();
std::string AppsDirectory();

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

  std::string Root() const;          // Apps/<Author>/<Name>
  std::string SettingsPath() const;  // .../settings.cfg
  std::string Projects() const;      // .../Projects
  std::string Presets() const;       // .../Presets
  std::string Presets(std::string_view group) const;  // .../Presets/<group>
  std::string Session() const;       // .../Session (state handed over across an app switch)
  // An app-specific folder: one path element of [A-Za-z0-9_-.], not "." or "..".
  std::string Folder(std::string_view name) const;

private:
  std::string id_;
};

// Card state. CardPresent() asks the device to mount the card when it is not mounted yet (the
// device decides how often a missing card is probed).
bool CardPresent();
std::uint64_t CardCapacityBytes();
std::uint64_t CardFreeBytes();

// Creates Cardenza/, System/, Shared/, Shared/Samples/ and Apps/. False without a writable card.
bool EnsureSystemLayout();
// The system layout plus the app's settings folder, Projects/, Presets/, Session/ and one
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
// Windows cannot rename over a file. The parent folder must exist.
bool WriteFileAtomic(const std::string& path, std::string_view contents);
// Reads `path`, or <path>.bak when an interrupted write left only the backup. Fails for files over
// `maxBytes`.
bool ReadFileWithBackup(const std::string& path, std::string& contents, std::size_t maxBytes);

}  // namespace os::storage
