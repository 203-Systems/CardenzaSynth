#include "OS/Storage/Settings.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <sys/stat.h>

#include "OS/Storage/Storage.h"

namespace os::storage {

namespace {

constexpr std::string_view kSystemScope = "system";

bool IsKeyCharacter(char c)
{
  return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.';
}

std::string_view Trim(std::string_view text)
{
  while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.remove_prefix(1);
  while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) text.remove_suffix(1);
  return text;
}

bool EqualsIgnoreCase(std::string_view text, std::string_view lower)
{
  if (text.size() != lower.size()) return false;
  for (std::size_t index = 0; index < text.size(); ++index)
  {
    char c = text[index];
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    if (c != lower[index]) return false;
  }
  return true;
}

enum class FileState { Missing, Regular, Error };

FileState InspectSettingsFile(const std::string& path, std::size_t& size)
{
  struct stat info{};
  if (stat(path.c_str(), &info) != 0)
  {
    // Only ENOENT is a missing file. In particular, EIO/EACCES/ENOTDIR must never turn an
    // unreadable card or a conflicting path into an apparently empty settings file.
    return errno == ENOENT ? FileState::Missing : FileState::Error;
  }
#if defined(_WIN32)
  const bool regular = (info.st_mode & _S_IFMT) == _S_IFREG;
#else
  const bool regular = S_ISREG(info.st_mode);
#endif
  if (!regular || info.st_size < 0 || static_cast<unsigned long long>(info.st_size) > kMaxSettingsFileBytes)
    return FileState::Error;
  size = static_cast<std::size_t>(info.st_size);
  return FileState::Regular;
}

bool ReadCardSettings(const std::string& path, std::vector<SettingsEntry>& entries)
{
  if (path.empty() || !CardPresent()) return false;
  std::string source = path;
  std::size_t size = 0;
  FileState state = InspectSettingsFile(source, size);
  if (state == FileState::Missing)
  {
    source += ".bak";
    state = InspectSettingsFile(source, size);
  }
  if (state == FileState::Error) return false;
  if (state == FileState::Missing)
  {
    entries.clear();
    return true;
  }
  std::FILE* file = std::fopen(source.c_str(), "rb");
  if (file == nullptr) return false;
  std::string text(size, '\0');
  const std::size_t read = size == 0 ? 0 : std::fread(text.data(), 1, size, file);
  // Reject a short read, an I/O error, or a file that changed size after stat. Do not replace a
  // file from a partial snapshot. Malformed text lines still use the tolerant settings parser.
  const bool complete = read == size && std::fgetc(file) == EOF && std::ferror(file) == 0;
  const bool closed = std::fclose(file) == 0;
  if (!complete || !closed) return false;
  entries = ParseSettings(text);
  return true;
}

std::string ParentPath(const std::string& path)
{
  const std::size_t slash = path.find_last_of('/');
  return slash == std::string::npos ? std::string() : path.substr(0, slash);
}

}  // namespace

bool IsValidSettingsKey(std::string_view key)
{
  if (key.empty() || key.size() > kMaxSettingsKey || key.front() == '.' || key.front() == '_') return false;
  return std::all_of(key.begin(), key.end(), IsKeyCharacter);
}

bool IsValidSettingsValue(std::string_view value)
{
  if (value.size() > kMaxSettingsValue) return false;
  if (!value.empty() && (value.front() == ' ' || value.back() == ' ')) return false;
  return std::all_of(value.begin(), value.end(), [](char c) { return c >= 32 && c < 127; });
}

std::vector<SettingsEntry> ParseSettings(std::string_view text, std::size_t* rejectedLines)
{
  std::vector<SettingsEntry> entries;
  std::size_t rejected = 0;
  while (!text.empty())
  {
    const std::size_t end = text.find('\n');
    std::string_view line = text.substr(0, end);
    text = end == std::string_view::npos ? std::string_view() : text.substr(end + 1);
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    line = Trim(line);
    if (line.empty() || line.front() == '#') continue;
    const std::size_t equals = line.find('=');
    if (equals == std::string_view::npos)
    {
      ++rejected;
      continue;
    }
    const std::string_view key = Trim(line.substr(0, equals));
    const std::string_view value = Trim(line.substr(equals + 1));
    if (!IsValidSettingsKey(key) || !IsValidSettingsValue(value))
    {
      ++rejected;
      continue;
    }
    const auto existing = std::find_if(entries.begin(), entries.end(), [&](const SettingsEntry& entry) { return entry.first == key; });
    if (existing != entries.end()) existing->second = std::string(value);
    else entries.emplace_back(std::string(key), std::string(value));
  }
  if (rejectedLines != nullptr) *rejectedLines = rejected;
  return entries;
}

std::string FormatSettings(const std::vector<SettingsEntry>& entries, std::string_view header)
{
  std::string text;
  while (!header.empty())
  {
    const std::size_t end = header.find('\n');
    text.append("# ");
    text.append(header.substr(0, end));
    text.push_back('\n');
    header = end == std::string_view::npos ? std::string_view() : header.substr(end + 1);
  }
  for (const auto& entry : entries)
  {
    text.append(entry.first);
    text.push_back('=');
    text.append(entry.second);
    text.push_back('\n');
  }
  return text;
}

Settings::Settings(std::string scope, std::string path) : scope_(std::move(scope)), path_(std::move(path)) {}

std::string Settings::FileHeader() const
{
  return "Cardenza settings: " + scope_ + " (storage protocol " + std::to_string(kProtocolVersion) +
         ")\nkey=value per line; unknown keys are kept";
}

void Settings::Load()
{
  entries_.clear();
  edits_.clear();
  dirty_ = false;
  loaded_ = true;
  (void)ReadCardSettings(path_, entries_);
}

bool Settings::RefreshFromCard()
{
  std::vector<SettingsEntry> refreshed;
  if (!ReadCardSettings(path_, refreshed)) return false;
  ApplyEdits(refreshed);
  if (entries_ != refreshed) ++revision_;
  entries_ = std::move(refreshed);
  loaded_ = true;
  return true;
}

bool Settings::Has(std::string_view key) const
{
  return std::any_of(entries_.begin(), entries_.end(), [&](const SettingsEntry& entry) { return entry.first == key; });
}

std::string Settings::GetString(std::string_view key, std::string_view fallback) const
{
  const auto entry = std::find_if(entries_.begin(), entries_.end(), [&](const SettingsEntry& item) { return item.first == key; });
  return entry != entries_.end() ? entry->second : std::string(fallback);
}

int Settings::GetInt(std::string_view key, int fallback, int minimum, int maximum) const
{
  if (!Has(key)) return fallback;
  const std::string text = GetString(key);
  if (text.empty()) return fallback;
  char* end = nullptr;
  errno = 0;
  const long value = std::strtol(text.c_str(), &end, 10);
  if (errno != 0 || end == text.c_str() || *end != '\0' || value < minimum || value > maximum) return fallback;
  return static_cast<int>(value);
}

bool Settings::GetBool(std::string_view key, bool fallback) const
{
  if (!Has(key)) return fallback;
  const std::string text = GetString(key);
  if (text == "1" || EqualsIgnoreCase(text, "true") || EqualsIgnoreCase(text, "on") || EqualsIgnoreCase(text, "yes")) return true;
  if (text == "0" || EqualsIgnoreCase(text, "false") || EqualsIgnoreCase(text, "off") || EqualsIgnoreCase(text, "no")) return false;
  return fallback;
}

void Settings::Put(std::string_view key, std::string_view value)
{
  const auto entry = std::find_if(entries_.begin(), entries_.end(), [&](const SettingsEntry& item) { return item.first == key; });
  if (entry != entries_.end()) entry->second = std::string(value);
  else entries_.emplace_back(std::string(key), std::string(value));
}

void Settings::RecordEdit(std::string_view key, std::string_view value, bool removed)
{
  const auto edit = std::find_if(edits_.begin(), edits_.end(), [&](const Edit& item) { return item.key == key; });
  if (edit != edits_.end())
  {
    edit->value = std::string(value);
    edit->removed = removed;
  }
  else edits_.push_back({std::string(key), std::string(value), removed});
  dirty_ = true;
}

void Settings::ApplyEdits(std::vector<SettingsEntry>& entries) const
{
  for (const Edit& edit : edits_)
  {
    const auto entry = std::find_if(entries.begin(), entries.end(), [&](const SettingsEntry& item) { return item.first == edit.key; });
    if (edit.removed)
    {
      if (entry != entries.end()) entries.erase(entry);
    }
    else if (entry != entries.end()) entry->second = edit.value;
    else entries.emplace_back(edit.key, edit.value);
  }
}

bool Settings::SetString(std::string_view key, std::string_view value)
{
  if (!IsValidSettingsKey(key) || !IsValidSettingsValue(value)) return false;
  if (!loaded_) Load();
  const auto entry = std::find_if(entries_.begin(), entries_.end(), [&](const SettingsEntry& item) { return item.first == key; });
  if (entry != entries_.end() && entry->second == value) return true;
  RecordEdit(key, value, false);
  Put(key, value);
  ++revision_;
  return true;
}

bool Settings::SetInt(std::string_view key, int value)
{
  return SetString(key, std::to_string(value));
}

bool Settings::SetBool(std::string_view key, bool value)
{
  return SetString(key, value ? "1" : "0");
}

bool Settings::Remove(std::string_view key)
{
  if (!IsValidSettingsKey(key)) return false;
  if (!loaded_) Load();
  const auto entry = std::find_if(entries_.begin(), entries_.end(), [&](const SettingsEntry& item) { return item.first == key; });
  const auto edit = std::find_if(edits_.begin(), edits_.end(), [&](const Edit& item) { return item.key == key; });
  if (entry == entries_.end() && edit != edits_.end() && edit->removed) return true;
  RecordEdit(key, {}, true);
  if (entry != entries_.end()) entries_.erase(entry);
  ++revision_;
  return true;
}

bool Settings::Flush()
{
  if (!dirty_) return true;
  std::vector<SettingsEntry> merged;
  if (!ReadCardSettings(path_, merged)) return false;
  ApplyEdits(merged);
  const std::string text = FormatSettings(merged, FileHeader());
  if (text.size() > kMaxSettingsFileBytes || !CreateDirectories(ParentPath(path_)) || !WriteFileAtomic(path_, text)) return false;
  if (entries_ != merged) ++revision_;
  entries_ = std::move(merged);
  edits_.clear();
  dirty_ = false;
  return true;
}

namespace {

struct Registry {
  std::unique_ptr<Settings> system;
  std::map<std::string, std::unique_ptr<Settings>, std::less<>> apps;
};

Registry& Stores()
{
  static Registry registry;
  return registry;
}

}  // namespace

Settings& SystemSettings()
{
  auto& stores = Stores();
  if (!stores.system) stores.system = std::make_unique<Settings>(std::string(kSystemScope), SystemSettingsPath());
  if (!stores.system->Loaded()) stores.system->Load();
  return *stores.system;
}

Settings& AppSettings(std::string_view appId)
{
  auto& stores = Stores();
  auto entry = stores.apps.find(appId);
  if (entry == stores.apps.end())
  {
    const AppFolder folder(appId);
    entry = stores.apps.emplace(std::string(appId), std::make_unique<Settings>(std::string(appId), folder.SettingsPath())).first;
  }
  if (!entry->second->Loaded()) entry->second->Load();
  return *entry->second;
}

void ReleaseAppSettings(std::string_view appId)
{
  auto& stores = Stores();
  const auto entry = stores.apps.find(appId);
  if (entry != stores.apps.end() && !entry->second->Dirty()) stores.apps.erase(entry);
}

bool FlushAllSettings()
{
  auto& stores = Stores();
  bool saved = true;
  if (stores.system) saved = stores.system->Flush() && saved;
  for (auto& entry : stores.apps) saved = entry.second->Flush() && saved;
  return saved;
}

std::uint32_t SettingsRevision()
{
  auto& stores = Stores();
  std::uint32_t revision = stores.system ? stores.system->Revision() : 0;
  for (const auto& entry : stores.apps) revision += entry.second->Revision();
  return revision;
}

bool AnySettingsDirty()
{
  auto& stores = Stores();
  if (stores.system && stores.system->Dirty()) return true;
  return std::any_of(stores.apps.begin(), stores.apps.end(), [](const auto& entry) { return entry.second->Dirty(); });
}

bool RefreshAllSettingsFromCard()
{
  auto& stores = Stores();
  bool refreshed = true;
  if (stores.system) refreshed = stores.system->RefreshFromCard() && refreshed;
  for (auto& entry : stores.apps) refreshed = entry.second->RefreshFromCard() && refreshed;
  return refreshed;
}

void ReloadAllSettings()
{
  auto& stores = Stores();
  if (stores.system) stores.system->Load();
  for (auto& entry : stores.apps) entry.second->Load();
}

}  // namespace os::storage
