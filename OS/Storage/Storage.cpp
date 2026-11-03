#include "OS/Storage/Storage.h"

#include <algorithm>
#include <cstdio>

#if defined(ESP_PLATFORM)
#include <cerrno>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#else
#include <filesystem>
#include <system_error>
#endif

#include "Devices/Device.h"

namespace os::storage {

namespace {

constexpr std::string_view kRootFolder = "Cardenza";
constexpr std::string_view kSystemFolder = "System";
constexpr std::string_view kSharedFolder = "Shared";
constexpr std::string_view kSamplesFolder = "Samples";
constexpr std::string_view kAppsFolder = "Apps";
constexpr std::string_view kSettingsFile = "settings.cfg";
constexpr std::size_t kMaxIdPart = 24;
constexpr std::size_t kMaxFolderName = 32;

bool IsIdCharacter(char c)
{
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
}

bool IsValidFolderName(std::string_view name)
{
  if (name.empty() || name.size() > kMaxFolderName || name == "." || name == "..") return false;
  return std::all_of(name.begin(), name.end(), [](char c) { return IsIdCharacter(c) || c == '.'; });
}

std::string ParentPath(const std::string& path)
{
  const std::size_t slash = path.find_last_of('/');
  return slash == std::string::npos ? std::string() : path.substr(0, slash);
}

}  // namespace

std::string JoinPath(std::string_view directory, std::string_view name)
{
  std::string path(directory);
  if (!path.empty() && path.back() != '/') path.push_back('/');
  path.append(name);
  return path;
}

std::string Root()
{
  return JoinPath(Device::Storage::RootPath(), kRootFolder);
}

std::string SystemDirectory()
{
  return JoinPath(Root(), kSystemFolder);
}

std::string LogDirectory()
{
  return JoinPath(Root(), "Logs");
}

std::string SystemSettingsPath()
{
  return JoinPath(SystemDirectory(), kSettingsFile);
}

std::string SharedDirectory()
{
  return JoinPath(Root(), kSharedFolder);
}

std::string SharedSamplesDirectory()
{
  return JoinPath(SharedDirectory(), kSamplesFolder);
}

std::string AppsDirectory()
{
  return JoinPath(Root(), kAppsFolder);
}

bool IsValidAppId(std::string_view id)
{
  const std::size_t slash = id.find('/');
  if (slash == std::string_view::npos || id.find('/', slash + 1) != std::string_view::npos) return false;
  const std::string_view author = id.substr(0, slash);
  const std::string_view name = id.substr(slash + 1);
  const auto validPart = [](std::string_view part) {
    return !part.empty() && part.size() <= kMaxIdPart && std::all_of(part.begin(), part.end(), IsIdCharacter);
  };
  return validPart(author) && validPart(name);
}

AppFolder::AppFolder(std::string_view id)
{
  if (IsValidAppId(id)) id_ = std::string(id);
}

std::string AppFolder::Root() const
{
  return Valid() ? JoinPath(AppsDirectory(), id_) : std::string();
}

std::string AppFolder::SettingsPath() const
{
  return Valid() ? JoinPath(Root(), kSettingsFile) : std::string();
}

std::string AppFolder::Projects() const
{
  return Folder("Projects");
}

std::string AppFolder::Presets() const
{
  return Folder("Presets");
}

std::string AppFolder::Presets(std::string_view group) const
{
  return Valid() && IsValidFolderName(group) ? JoinPath(Presets(), group) : std::string();
}

std::string AppFolder::Session() const
{
  return Folder("Session");
}

std::string AppFolder::Folder(std::string_view name) const
{
  return Valid() && IsValidFolderName(name) ? JoinPath(Root(), name) : std::string();
}

bool CardPresent()
{
  return Device::Storage::Mount();
}

bool CardMounted()
{
  return Device::Storage::IsMounted();
}

bool ReadInternalRecord(std::string_view key, std::string& value)
{
  return Device::Storage::ReadInternal(key, value);
}

bool WriteInternalRecord(std::string_view key, std::string_view value)
{
  return Device::Storage::WriteInternal(key, value);
}

bool EraseInternalRecord(std::string_view key)
{
  return Device::Storage::EraseInternal(key);
}

std::vector<std::string> ListInternalRecords()
{
  return Device::Storage::ListInternal();
}

void ReportCardIoError()
{
  Device::Storage::CheckAfterIoError();
}

std::uint64_t CardCapacityBytes()
{
  return CardPresent() ? Device::Storage::CapacityBytes() : 0;
}

std::uint64_t CardFreeBytes()
{
  return CardPresent() ? Device::Storage::FreeBytes() : 0;
}

bool EnsureSystemLayout()
{
  if (!CardPresent()) return false;
  for (const std::string& path : {Root(), SystemDirectory(), SharedDirectory(), SharedSamplesDirectory(), AppsDirectory()})
    if (!CreateDirectories(path)) return false;
  return true;
}

bool EnsureAppLayout(const AppFolder& app, const std::vector<std::string>& presetGroups)
{
  if (!app.Valid() || !EnsureSystemLayout()) return false;
  for (const std::string& path : {app.Root(), app.Projects(), app.Presets(), app.Session()})
    if (!CreateDirectories(path)) return false;
  for (const std::string& group : presetGroups)
  {
    const std::string path = app.Presets(group);
    if (path.empty() || !CreateDirectories(path)) return false;
  }
  return true;
}

#if defined(ESP_PLATFORM)

bool Exists(const std::string& path)
{
  struct stat info{};
  return !path.empty() && stat(path.c_str(), &info) == 0;
}

bool IsDirectory(const std::string& path)
{
  struct stat info{};
  return !path.empty() && stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
}

bool IsFile(const std::string& path)
{
  struct stat info{};
  return !path.empty() && stat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode);
}

long long FileSize(const std::string& path)
{
  struct stat info{};
  if (path.empty() || stat(path.c_str(), &info) != 0 || !S_ISREG(info.st_mode)) return -1;
  return static_cast<long long>(info.st_size);
}

bool CreateDirectories(const std::string& path)
{
  if (path.empty()) return false;
  if (IsDirectory(path)) return true;
  // The card root is a VFS mount point that FAT cannot stat or create: start below it.
  const std::string root = Device::Storage::RootPath();
  std::size_t position = path.compare(0, root.size(), root) == 0 ? root.size() : 0;
  while (position < path.size())
  {
    std::size_t next = path.find('/', position + 1);
    if (next == std::string::npos) next = path.size();
    const std::string prefix = path.substr(0, next);
    position = next;
    if (prefix.empty() || prefix == "/" || IsDirectory(prefix)) continue;
    if (mkdir(prefix.c_str(), 0775) != 0 && !IsDirectory(prefix)) return false;
  }
  return IsDirectory(path);
}

bool RemoveEmptyDirectory(const std::string& path)
{
  return IsDirectory(path) && rmdir(path.c_str()) == 0;
}

std::vector<DirectoryEntry> ListDirectory(const std::string& path)
{
  std::vector<DirectoryEntry> entries;
  DIR* directory = path.empty() ? nullptr : opendir(path.c_str());
  if (directory == nullptr) return entries;
  while (const dirent* entry = readdir(directory))
  {
    const std::string name = entry->d_name;
    if (name.empty() || name == "." || name == "..") continue;
    entries.push_back({name, IsDirectory(JoinPath(path, name))});
  }
  closedir(directory);
  std::sort(entries.begin(), entries.end(), [](const DirectoryEntry& a, const DirectoryEntry& b) { return a.name < b.name; });
  return entries;
}

#else

namespace fs = std::filesystem;

bool Exists(const std::string& path)
{
  std::error_code error;
  return !path.empty() && fs::exists(fs::u8path(path), error);
}

bool IsDirectory(const std::string& path)
{
  std::error_code error;
  return !path.empty() && fs::is_directory(fs::u8path(path), error);
}

bool IsFile(const std::string& path)
{
  std::error_code error;
  return !path.empty() && fs::is_regular_file(fs::u8path(path), error);
}

long long FileSize(const std::string& path)
{
  std::error_code error;
  if (!IsFile(path)) return -1;
  const auto size = fs::file_size(fs::u8path(path), error);
  return error ? -1 : static_cast<long long>(size);
}

bool CreateDirectories(const std::string& path)
{
  if (path.empty()) return false;
  std::error_code error;
  fs::create_directories(fs::u8path(path), error);
  return IsDirectory(path);
}

bool RemoveEmptyDirectory(const std::string& path)
{
  std::error_code error;
  // fs::remove refuses a non-empty directory.
  return IsDirectory(path) && fs::remove(fs::u8path(path), error) && !error;
}

std::vector<DirectoryEntry> ListDirectory(const std::string& path)
{
  std::vector<DirectoryEntry> entries;
  if (path.empty()) return entries;
  std::error_code error;
  for (fs::directory_iterator it(fs::u8path(path), error), end; !error && it != end; it.increment(error))
  {
    std::error_code typeError;
    entries.push_back({it->path().filename().u8string(), it->is_directory(typeError)});
  }
  std::sort(entries.begin(), entries.end(), [](const DirectoryEntry& a, const DirectoryEntry& b) { return a.name < b.name; });
  return entries;
}

#endif

static bool WriteFileAtomicOnce(const std::string& path, std::string_view contents)
{
  if (path.empty() || !IsDirectory(ParentPath(path))) return false;
  const std::string temporary = path + ".tmp";
  const std::string backup = path + ".bak";
  std::FILE* file = std::fopen(temporary.c_str(), "wb");
  if (file == nullptr) return false;
  bool written = contents.empty() || std::fwrite(contents.data(), 1, contents.size(), file) == contents.size();
  written = std::fflush(file) == 0 && written;
#if defined(ESP_PLATFORM)
  written = fsync(fileno(file)) == 0 && written;
#endif
  written = std::fclose(file) == 0 && written;
  if (!written)
  {
    std::remove(temporary.c_str());
    return false;
  }
  // FAT and Windows cannot rename over a file: keep the old one as the backup until the new one
  // is in place, so an interruption leaves either the old file, the backup or the new file.
  const bool existing = IsFile(path);
  if (existing)
  {
    if (IsFile(backup) && std::remove(backup.c_str()) != 0)
    {
      std::remove(temporary.c_str());
      return false;
    }
    if (std::rename(path.c_str(), backup.c_str()) != 0)
    {
      std::remove(temporary.c_str());
      return false;
    }
  }
  if (std::rename(temporary.c_str(), path.c_str()) != 0)
  {
    if (existing) std::rename(backup.c_str(), path.c_str());
    std::remove(temporary.c_str());
    return false;
  }
  // The new file replaces both the old one and a backup left by an earlier interruption.
  if (IsFile(backup)) std::remove(backup.c_str());
  return true;
}

bool WriteFileAtomic(const std::string& path, std::string_view contents)
{
  const bool written = WriteFileAtomicOnce(path, contents);
  if (!written) ReportCardIoError();
  return written;
}

bool ReadFileWithBackup(const std::string& path, std::string& contents, std::size_t maxBytes)
{
  const std::string source = IsFile(path) ? path : path + ".bak";
  const long long size = FileSize(source);
  if (size < 0 || static_cast<unsigned long long>(size) > maxBytes) return false;
  std::FILE* file = std::fopen(source.c_str(), "rb");
  if (file == nullptr) return false;
  std::string text(static_cast<std::size_t>(size), '\0');
  const std::size_t read = text.empty() ? 0 : std::fread(&text[0], 1, text.size(), file);
  std::fclose(file);
  if (read != text.size()) return false;
  contents = std::move(text);
  return true;
}

}  // namespace os::storage
