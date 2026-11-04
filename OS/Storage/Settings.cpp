#include "OS/Storage/Settings.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>

#include "Devices/Device.h"
#include "OS/Storage/Storage.h"

namespace os::storage {

namespace {

constexpr std::string_view kSystemScope = "system";
// First line of an internal-flash record: names the store it belongs to.
constexpr std::string_view kInternalMarker = "#! ";

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

// FNV-1a, so an app id maps to a short internal-flash key (NVS keys are at most 15 characters).
std::uint32_t Hash(std::string_view text)
{
  std::uint32_t hash = 2166136261u;
  for (const char c : text)
  {
    hash ^= static_cast<std::uint8_t>(c);
    hash *= 16777619u;
  }
  return hash;
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

std::string Settings::InternalKey() const
{
  if (scope_ == kSystemScope) return "sys";
  char key[16];
  std::snprintf(key, sizeof(key), "app%08lx", static_cast<unsigned long>(Hash(scope_)));
  return key;
}

std::string Settings::FileHeader() const
{
  return "Cardenza settings: " + scope_ + " (storage protocol " + std::to_string(kProtocolVersion) +
         ")\nkey=value per line; unknown keys are kept";
}

void Settings::Load()
{
  entries_.clear();
  dirty_ = false;
  internalOnly_ = false;
  loaded_ = true;
  if (path_.empty()) return;
  std::string text;
  if (CardPresent() && ReadFileWithBackup(path_, text, kMaxSettingsFileBytes)) entries_ = ParseSettings(text);
  std::string record;
  const std::string marker = std::string(kInternalMarker) + scope_ + "\n";
  if (Device::Storage::ReadInternal(InternalKey(), record) && record.compare(0, marker.size(), marker) == 0)
  {
    for (const auto& entry : ParseSettings(std::string_view(record).substr(marker.size()))) Put(entry.first, entry.second);
    internalOnly_ = true;
  }
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

bool Settings::SetString(std::string_view key, std::string_view value)
{
  if (!IsValidSettingsKey(key) || !IsValidSettingsValue(value)) return false;
  if (!loaded_) Load();
  const auto entry = std::find_if(entries_.begin(), entries_.end(), [&](const SettingsEntry& item) { return item.first == key; });
  if (entry != entries_.end() && entry->second == value) return true;
  Put(key, value);
  dirty_ = true;
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
  if (!loaded_) Load();
  const auto entry = std::find_if(entries_.begin(), entries_.end(), [&](const SettingsEntry& item) { return item.first == key; });
  if (entry == entries_.end()) return false;
  entries_.erase(entry);
  dirty_ = true;
  ++revision_;
  return true;
}

bool Settings::Flush()
{
  if (!loaded_ || path_.empty()) return !dirty_;
  if (!dirty_ && !internalOnly_) return true;
  const std::string text = FormatSettings(entries_, FileHeader());
  if (CardPresent() && CreateDirectories(ParentPath(path_)) && WriteFileAtomic(path_, text))
  {
    if (internalOnly_) (void)Device::Storage::EraseInternal(InternalKey());
    dirty_ = false;
    internalOnly_ = false;
    return true;
  }
  if (!dirty_) return true;  // Still safe in internal flash; the card is not back yet.
  // No card (or it cannot be written): keep the values on the device until it is back.
  if (Device::Storage::WriteInternal(InternalKey(), std::string(kInternalMarker) + scope_ + "\n" + text))
  {
    dirty_ = false;
    internalOnly_ = true;
    return true;
  }
  return false;
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

bool AnySettingsPendingCardSync()
{
  auto& stores = Stores();
  if (stores.system && stores.system->PendingCardSync()) return true;
  return std::any_of(stores.apps.begin(), stores.apps.end(), [](const auto& entry) { return entry.second->PendingCardSync(); });
}

void ReloadAllSettings()
{
  auto& stores = Stores();
  if (stores.system) stores.system->Load();
  for (auto& entry : stores.apps) entry.second->Load();
}

}  // namespace os::storage
