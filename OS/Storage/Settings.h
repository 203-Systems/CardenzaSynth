#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Settings files of the app storage protocol (docs/app-storage-protocol.md): text, one `key=value`
// per line, `#` comments. Keys are 1..48 characters of [a-z0-9_.] starting with a letter or digit;
// values are printable ASCII up to 200 characters without leading or trailing spaces. Reading is
// tolerant: bad lines are skipped, unknown keys are kept and written back. Files are replaced
// atomically (WriteFileAtomic).
namespace os::storage {

using SettingsEntry = std::pair<std::string, std::string>;

inline constexpr std::size_t kMaxSettingsKey = 48;
inline constexpr std::size_t kMaxSettingsValue = 200;
inline constexpr std::size_t kMaxSettingsFileBytes = 16 * 1024;

bool IsValidSettingsKey(std::string_view key);
bool IsValidSettingsValue(std::string_view value);
// Entries in file order; a repeated key keeps its first position and its last value.
std::vector<SettingsEntry> ParseSettings(std::string_view text, std::size_t* rejectedLines = nullptr);
// `header` is written as comment lines before the entries.
std::string FormatSettings(const std::vector<SettingsEntry>& entries, std::string_view header);

// One settings store: a RAM cache of a settings file. Values are changed in RAM (Dirty) and written
// to the card by Flush(); without a card they wait in RAM until it is back (lost if the power goes
// first). Internal flash is never written (Device::Storage): a record older firmware kept there is
// read once, wins over the card file and is copied to the card and erased by the first Flush() the
// card takes.
class Settings {
public:
  // `scope` is "system" or an app id and names the old internal-flash record; `path` is the card file.
  Settings(std::string scope, std::string path);

  const std::string& Scope() const { return scope_; }
  const std::string& Path() const { return path_; }

  // Reads the card file (or the backup an interrupted write left), then the internal record, which
  // wins key by key: it only exists while it is newer than the card. Discards unsaved changes.
  void Load();
  bool Loaded() const { return loaded_; }

  bool Has(std::string_view key) const;
  std::string GetString(std::string_view key, std::string_view fallback = {}) const;
  // `fallback` when the key is missing, not a number or outside [minimum, maximum].
  int GetInt(std::string_view key, int fallback, int minimum = -2147483647 - 1, int maximum = 2147483647) const;
  // Accepts 1/0, true/false, on/off, yes/no.
  bool GetBool(std::string_view key, bool fallback) const;

  // False (and nothing changes) for an invalid key or value.
  bool SetString(std::string_view key, std::string_view value);
  bool SetInt(std::string_view key, int value);
  bool SetBool(std::string_view key, bool value);
  bool Remove(std::string_view key);

  const std::vector<SettingsEntry>& Entries() const { return entries_; }
  // Changes that are only in RAM.
  bool Dirty() const { return dirty_; }
  // The newest values are in an old internal-flash record and not yet on the card.
  bool PendingCardSync() const { return internalOnly_; }
  // Increases on every change; lets the system write settings a moment after the last change.
  std::uint32_t Revision() const { return revision_; }

  // Writes RAM changes and an old internal-flash state to the card. True when nothing is left only in
  // RAM afterwards; false without a card (the changes stay, Dirty).
  bool Flush();

private:
  std::string InternalKey() const;
  std::string FileHeader() const;
  void Put(std::string_view key, std::string_view value);
  void MergeCardFile();

  std::string scope_;
  std::string path_;
  std::vector<SettingsEntry> entries_;
  bool loaded_ = false;
  bool dirty_ = false;
  bool internalOnly_ = false;
  bool cardRead_ = false;  // the card's file is in entries_ (false: loaded without the card)
  std::uint32_t revision_ = 0;
};

// The device settings (System/settings.cfg). Loaded on first use.
Settings& SystemSettings();
// The settings of an app (Apps/<Author>/<Name>/settings.cfg). Loaded on first use; an invalid id
// gives a store that never writes.
Settings& AppSettings(std::string_view appId);
// Drops an app's cached settings when nothing of them is left only in RAM (after the app is gone).
void ReleaseAppSettings(std::string_view appId);
// Flushes every loaded store. True when nothing is left only in RAM.
bool FlushAllSettings();
// For the system's write-behind: the sum of every loaded store's Revision(), whether any store has
// changes only in RAM, and whether any waits in an old internal-flash record for the card.
std::uint32_t SettingsRevision();
bool AnySettingsDirty();
bool AnySettingsPendingCardSync();
// Reloads every cached store from the card (tests, card swap).
void ReloadAllSettings();

}  // namespace os::storage
