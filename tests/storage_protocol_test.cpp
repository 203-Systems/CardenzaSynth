// The app storage protocol (docs/app-storage-protocol.md): OS/Storage on the simulator's card and
// internal-flash records older firmware left (Devices/CardenzaSim/Storage.cpp), with the SD card taken out and put back.
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include "Devices/CardenzaSim/SimulatorInput.h"
#include "Devices/Device.h"
#include "OS/Storage/DeviceSettings.h"
#include "OS/Storage/Migration.h"
#include "OS/Storage/Settings.h"
#include "OS/Storage/Storage.h"

namespace fs = std::filesystem;

namespace {
std::uint8_t ledMaster = 0;
std::uint8_t backlight = 0;
std::uint8_t outputVolume = 0;
}  // namespace

// DeviceSettings applies values to these; the test only records them.
namespace Device {
namespace LED {
void SetGlobalBrightness(std::uint8_t level) { ledMaster = level; }
}
namespace Display {
void SetBacklight(std::uint8_t level) { backlight = level; }
}
namespace Audio {
void SetOutputVolume(std::uint8_t percent) { outputVolume = percent; }
}
}  // namespace Device

namespace {

using namespace os::storage;

fs::path Card() { return fs::path(Device::Storage::RootPath()); }
fs::path Internal() { return Card().parent_path() / "internal"; }

std::string ReadText(const fs::path& path)
{
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), {}};
}

void WriteText(const fs::path& path, const std::string& text)
{
  fs::create_directories(path.parent_path());
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file << text;
}

void ResetCard()
{
  CardenzaSim::Input::SetCardInserted(true);
  fs::remove_all(Card());
  fs::remove_all(Internal());
  ReloadAllSettings();
}

void TestAppPaths()
{
  const std::string root = Card().generic_string() + "/Cardenza";
  assert(Root() == root);
  assert(SystemSettingsPath() == root + "/System/settings.cfg");
  assert(SharedSamplesDirectory() == root + "/Shared/Samples");
  const AppFolder synth("Cardenza/Synth");
  assert(synth.Valid() && synth.Id() == "Cardenza/Synth");
  assert(synth.Root() == root + "/Apps/Cardenza/Synth");
  assert(synth.SettingsPath() == root + "/Apps/Cardenza/Synth/settings.cfg");
  assert(synth.Projects() == root + "/Apps/Cardenza/Synth/Projects");
  assert(synth.Presets("DX7") == root + "/Apps/Cardenza/Synth/Presets/DX7");
  assert(synth.Session() == root + "/Apps/Cardenza/Synth/Session");
  assert(synth.Folder("Wavetables") == root + "/Apps/Cardenza/Synth/Wavetables");
  assert(synth.Folder("..").empty() && synth.Folder("a/b").empty() && synth.Presets("").empty());
  for (const char* bad : {"", "Synth", "a/b/c", "../x", "a/..", "a b/c", "a/", "/a", "Cardenza/Synth!",
                          "Author/ThisNameIsLongerThan24Chars"})
  {
    assert(!IsValidAppId(bad));
    assert(!AppFolder(bad).Valid() && AppFolder(bad).Root().empty() && AppFolder(bad).Projects().empty());
  }
  assert(IsValidAppId("me/My-App_2"));

  assert(CardPresent());
  assert(EnsureAppLayout(synth, {"DX7", "Juno"}));
  for (const char* folder : {"System", "Shared/Samples", "Apps/Cardenza/Synth/Projects", "Apps/Cardenza/Synth/Presets/DX7",
                             "Apps/Cardenza/Synth/Presets/Juno", "Apps/Cardenza/Synth/Session"})
    assert(fs::is_directory(fs::path(root) / folder));
  assert(!EnsureAppLayout(AppFolder("bad id"), {}));
  assert(!EnsureAppLayout(synth, {"../escape"}));
  std::cout << "app paths passed\n";
}

void TestSettingsFormat()
{
  std::size_t rejected = 0;
  const auto entries = ParseSettings(
      "# comment\r\n"
      "  keys.scale = 3 \r\n"
      "\n"
      "no equals sign\n"
      "Upper=1\n"
      "bad key=1\n"
      "_hidden=1\n"
      "value.tab=a\tb\n"
      "future.key=kept as is\n"
      "keys.scale=4\n"
      "empty=\n"
      "url=http://x/#anchor\n",
      &rejected);
  assert(rejected == 5);
  assert(entries.size() == 4);
  assert(entries[0].first == "keys.scale" && entries[0].second == "4");  // first position, last value
  assert(entries[1].first == "future.key" && entries[1].second == "kept as is");
  assert(entries[2].first == "empty" && entries[2].second.empty());
  assert(entries[3].second == "http://x/#anchor");
  const std::string text = FormatSettings(entries, "line one\nline two");
  assert(text == "# line one\n# line two\nkeys.scale=4\nfuture.key=kept as is\nempty=\nurl=http://x/#anchor\n");
  assert(ParseSettings(text) == entries);
  assert(IsValidSettingsKey("arp.latch") && !IsValidSettingsKey("Arp") && !IsValidSettingsKey(".x") &&
         !IsValidSettingsKey(std::string(kMaxSettingsKey + 1, 'a')));
  assert(IsValidSettingsValue("Cardenza/Synth") && !IsValidSettingsValue(" x") && !IsValidSettingsValue("a\nb") &&
         !IsValidSettingsValue(std::string(kMaxSettingsValue + 1, 'a')));
  std::cout << "settings format passed\n";
}

void TestSettingsRoundTrip()
{
  ResetCard();
  const AppFolder app("Test/RoundTrip");
  Settings settings(app.Id(), app.SettingsPath());
  settings.Load();
  assert(settings.Entries().empty() && !settings.Dirty());
  assert(settings.GetInt("missing", 7) == 7 && settings.GetBool("missing", true) && settings.GetString("missing", "x") == "x");
  assert(settings.SetInt("volume", 42) && settings.SetBool("latch", true) && settings.SetString("last", "Cardenza/Synth"));
  assert(!settings.SetString("Bad Key", "1") && !settings.SetString("ok", "line\nbreak"));
  const std::uint32_t revision = settings.Revision();
  assert(settings.SetInt("volume", 42) && settings.Revision() == revision);  // unchanged value: not dirty again
  assert(settings.Dirty() && settings.Flush() && !settings.Dirty() && !settings.PendingCardSync());
  // The app folder is created on demand.
  assert(fs::is_regular_file(app.SettingsPath()));
  const std::string file = ReadText(app.SettingsPath());
  assert(file.find("# Cardenza settings: Test/RoundTrip (storage protocol 1)\n") == 0);
  assert(file.find("\nvolume=42\nlatch=1\nlast=Cardenza/Synth\n") != std::string::npos);
  assert(!fs::exists(app.SettingsPath() + ".tmp") && !fs::exists(app.SettingsPath() + ".bak"));

  // Unknown keys and hand edits survive a load / change / flush cycle; bad lines are dropped.
  WriteText(app.SettingsPath(), file + "from.newer.firmware=yes\nthis line is broken\nvolume = 150\n");
  Settings reloaded(app.Id(), app.SettingsPath());
  reloaded.Load();
  assert(reloaded.GetInt("volume", 5) == 150 && reloaded.GetInt("volume", 5, 0, 100) == 5);  // out of range -> fallback
  assert(reloaded.GetBool("latch", false) && reloaded.GetString("last") == "Cardenza/Synth");
  assert(reloaded.GetBool("from.newer.firmware", false));
  assert(reloaded.SetInt("volume", 99) && reloaded.Flush());
  const std::string rewritten = ReadText(app.SettingsPath());
  assert(rewritten.find("from.newer.firmware=yes\n") != std::string::npos && rewritten.find("broken") == std::string::npos &&
         rewritten.find("volume=99\n") != std::string::npos);
  assert(reloaded.Remove("latch") && reloaded.Flush() && ReadText(app.SettingsPath()).find("latch=") == std::string::npos);
  // Not numbers / not booleans fall back.
  assert(reloaded.SetString("word", "abc") && reloaded.GetInt("word", -1) == -1 && reloaded.GetBool("word", true));
  assert(reloaded.SetString("big", "99999999999") && reloaded.GetInt("big", -1) == -1);
  assert(reloaded.SetString("yes", "On") && reloaded.GetBool("yes", false));
  std::cout << "settings round trip passed\n";
}

void TestAtomicWriteRecovery()
{
  ResetCard();
  assert(EnsureSystemLayout());
  const std::string path = (Card() / "Cardenza" / "System" / "atomic.cfg").generic_string();
  assert(WriteFileAtomic(path, "first\n") && ReadText(path) == "first\n");
  assert(WriteFileAtomic(path, "second\n") && ReadText(path) == "second\n");
  assert(!fs::exists(path + ".bak") && !fs::exists(path + ".tmp"));
  // Interrupted between "old -> .bak" and ".tmp -> new": only the backup and a temporary are left.
  fs::rename(path, path + ".bak");
  WriteText(path + ".tmp", "half writ");
  std::string text;
  assert(ReadFileWithBackup(path, text, 1024) && text == "second\n");
  assert(!ReadFileWithBackup(path, text, 3));  // too large
  // The next write replaces the leftovers and removes the stale backup.
  assert(WriteFileAtomic(path, "third\n") && ReadText(path) == "third\n");
  assert(!fs::exists(path + ".bak") && !fs::exists(path + ".tmp"));
  assert(!WriteFileAtomic((Card() / "Cardenza" / "NoSuchFolder" / "x.cfg").generic_string(), "x"));

  // A settings file that only exists as a backup still loads, and the next flush restores it.
  const std::string settingsPath = SystemSettingsPath();
  assert(WriteFileAtomic(settingsPath, "protocol=1\nsys.volume=33\n"));
  fs::rename(settingsPath, settingsPath + ".bak");
  ReloadAllSettings();
  Settings& system = SystemSettings();
  assert(system.GetInt("sys.volume", 0) == 33);
  assert(system.SetInt("sys.led", 9) && FlushAllSettings());
  assert(fs::is_regular_file(settingsPath) && !fs::exists(settingsPath + ".bak"));
  assert(ReadText(settingsPath).find("sys.volume=33\nsys.led=9\n") != std::string::npos);
  std::cout << "atomic write and backup recovery passed\n";
}

void TestMissingCardFallback()
{
  ResetCard();
  Settings& system = SystemSettings();
  assert(system.SetInt("sys.volume", 50) && FlushAllSettings());
  const std::string settingsPath = SystemSettingsPath();
  assert(ReadText(settingsPath).find("sys.volume=50\n") != std::string::npos);
  const auto internalEmpty = [] { return !fs::exists(Internal()) || fs::is_empty(Internal()); };

  // Card out: the changes wait in RAM. Internal flash is never written.
  CardenzaSim::Input::SetCardInserted(false);
  assert(!CardPresent() && CardFreeBytes() == 0 && !EnsureSystemLayout());
  assert(system.SetInt("sys.volume", 70) && system.SetString("boot.app", "Cardenza/Synth"));
  assert(AnySettingsDirty() && !FlushAllSettings());
  assert(AnySettingsDirty() && !AnySettingsPendingCardSync() && system.GetInt("sys.volume", 0) == 70);
  assert(internalEmpty());
  assert(ReadText(settingsPath).find("sys.volume=50\n") != std::string::npos);  // card untouched
  Settings& app = AppSettings("Test/Fallback");
  assert(app.SetInt("keys.octave", 2) && !app.Flush() && app.Dirty());

  // Card back: the next flush writes what waited.
  CardenzaSim::Input::SetCardInserted(true);
  assert(FlushAllSettings() && !AnySettingsDirty());
  const std::string synced = ReadText(settingsPath);
  assert(synced.find("sys.volume=70\n") != std::string::npos && synced.find("boot.app=Cardenza/Synth\n") != std::string::npos);
  assert(ReadText(AppFolder("Test/Fallback").SettingsPath()).find("keys.octave=2\n") != std::string::npos);
  assert(internalEmpty());

  // Older firmware kept settings in internal flash while the card was out: such a record wins over the
  // card file once, goes to the card at the next flush and is erased. Without a card it waits.
  WriteText(Internal() / "sys.rec", "#! system\nsys.volume=12\n");
  CardenzaSim::Input::SetCardInserted(false);
  ReloadAllSettings();
  assert(system.GetInt("sys.volume", 0) == 12 && system.PendingCardSync() && !system.Dirty());
  assert(FlushAllSettings() && system.PendingCardSync() && fs::exists(Internal() / "sys.rec"));
  CardenzaSim::Input::SetCardInserted(true);
  assert(FlushAllSettings() && !AnySettingsPendingCardSync() && !fs::exists(Internal() / "sys.rec"));
  // Loaded without the card, written with it: what only the card's file had stays.
  const std::string moved = ReadText(settingsPath);
  assert(moved.find("sys.volume=12\n") != std::string::npos && moved.find("boot.app=Cardenza/Synth\n") != std::string::npos);
  // The same for a change made while the card was out since the start.
  CardenzaSim::Input::SetCardInserted(false);
  ReloadAllSettings();
  assert(system.SetInt("sys.led", 4) && !FlushAllSettings());
  CardenzaSim::Input::SetCardInserted(true);
  assert(FlushAllSettings());
  const std::string merged = ReadText(settingsPath);
  assert(merged.find("sys.led=4\n") != std::string::npos && merged.find("sys.volume=12\n") != std::string::npos &&
         merged.find("boot.app=Cardenza/Synth\n") != std::string::npos);
  // A record for another store (same key, other scope) is ignored.
  WriteText(Internal() / "sys.rec", "#! someone-else\nsys.volume=1\n");
  ReloadAllSettings();
  assert(system.GetInt("sys.volume", 0) == 12 && !system.PendingCardSync());
  fs::remove(Internal() / "sys.rec");
  ReleaseAppSettings("Test/Fallback");
  std::cout << "missing card: settings wait in RAM, an old internal record moves to the card\n";
}

void TestDeviceSettings()
{
  ResetCard();
  ApplyDeviceSettings();
  assert(OutputVolume() == kDefaultOutputVolume && KeyLedBrightness() == kDefaultKeyLedBrightness &&
         ScreenBacklight() == kDefaultScreenBacklight);
  assert(ledMaster == kDefaultKeyLedBrightness && backlight == kDefaultScreenBacklight);
  SetOutputVolume(140);  // clamped
  SetKeyLedBrightness(9);
  SetScreenBacklight(3);  // never fully dark
  assert(OutputVolume() == 100 && outputVolume == 100 && KeyLedBrightness() == 9 && ledMaster == 9);
  assert(ScreenBacklight() == kMinScreenBacklight && backlight == kMinScreenBacklight);
  SetOutputVolume(50);
  assert(outputVolume == 100 - 50 * 42 / 100);  // the perceptual curve
  SetOutputVolume(0);
  assert(outputVolume == 0);
  SetOutputVolume(60);
  assert(FlushAllSettings());
  const std::string text = ReadText(SystemSettingsPath());
  assert(text.find("sys.volume=60\n") != std::string::npos && text.find("sys.led=9\n") != std::string::npos &&
         text.find("sys.screen=10\n") != std::string::npos);
  // Boot: the values come back from the card and go to the device.
  ledMaster = backlight = outputVolume = 0;
  ReloadAllSettings();
  ApplyDeviceSettings();
  assert(OutputVolume() == 60 && ledMaster == 9 && backlight == kMinScreenBacklight && outputVolume == 100 - 40 * 42 / 100);
  std::cout << "device settings passed\n";
}

std::size_t CountFiles(const fs::path& root)
{
  std::size_t count = 0;
  for (const auto& entry : fs::recursive_directory_iterator(root))
    count += entry.is_regular_file() ? 1 : 0;
  return count;
}

void TestMigration()
{
  ResetCard();
  const AppFolder synth("Cardenza/Synth");
  const fs::path legacy = Card() / "CardenzaSynth";
  assert(LegacyRoot() == legacy.generic_string());
  // Nothing to do on a card without the old layout.
  MigrationResult result = MigrateLegacyLayout(synth);
  assert(!result.found && result.moved == 0);

  WriteText(legacy / "Project" / "SONG 1.ccproj", "project one");
  WriteText(legacy / "Project" / "SONG 2.ccproj.bak", "recoverable backup");
  WriteText(legacy / "Preset" / "DX7" / "user.ccpreset", "dx7 bank");
  WriteText(legacy / "Preset" / "Juno" / "user.ccpreset", "juno bank");
  WriteText(legacy / "Sample" / "kick.wav", "RIFF kick");
  WriteText(legacy / "Sample" / "Loops" / "beat.wav", "RIFF beat");
  WriteText(legacy / "notes.txt", "the user's own file");
  result = MigrateLegacyLayout(synth);
  assert(result.found && result.moved == 6 && result.Complete());
  assert(ReadText(fs::path(synth.Projects()) / "SONG 1.ccproj") == "project one");
  assert(ReadText(fs::path(synth.Projects()) / "SONG 2.ccproj.bak") == "recoverable backup");
  assert(ReadText(fs::path(synth.Presets("DX7")) / "user.ccpreset") == "dx7 bank");
  assert(ReadText(fs::path(synth.Presets("Juno")) / "user.ccpreset") == "juno bank");
  // Samples keep their names, so projects that name them still find them.
  assert(ReadText(fs::path(SharedSamplesDirectory()) / "kick.wav") == "RIFF kick");
  assert(ReadText(fs::path(SharedSamplesDirectory()) / "Loops" / "beat.wav") == "RIFF beat");
  // Moved folders are gone; the folder itself stays for the user's own file.
  assert(!fs::exists(legacy / "Project") && !fs::exists(legacy / "Preset") && !fs::exists(legacy / "Sample"));
  assert(ReadText(legacy / "notes.txt") == "the user's own file");
  assert(SystemSettings().GetString("migrate.legacy") == "done" && SystemSettings().GetInt("migrate.left", -1) == 0);
  assert(ReadText(SystemSettingsPath()).find("migrate.legacy=done\n") != std::string::npos);

  // Idempotent: running again changes nothing.
  const std::size_t files = CountFiles(Card() / "Cardenza");
  result = MigrateLegacyLayout(synth);
  assert(result.moved == 0 && result.conflicts == 0 && result.failed == 0 && CountFiles(Card() / "Cardenza") == files);

  // Old files copied back onto a card that already has the new layout: they merge file by file,
  // an identical copy is dropped and a different file of the same name never overwrites.
  WriteText(legacy / "Sample" / "kick.wav", "RIFF kick");                // identical
  WriteText(legacy / "Sample" / "snare.wav", "RIFF snare");              // new
  WriteText(legacy / "Project" / "SONG 1.ccproj", "a different song");   // conflict
  result = MigrateLegacyLayout(synth);
  assert(result.moved == 1 && result.duplicates == 1 && result.conflicts == 1 && !result.Complete());
  assert(ReadText(fs::path(SharedSamplesDirectory()) / "snare.wav") == "RIFF snare");
  assert(ReadText(fs::path(synth.Projects()) / "SONG 1.ccproj") == "project one");  // not overwritten
  assert(ReadText(legacy / "Project" / "SONG 1.ccproj") == "a different song");     // kept for the user
  assert(!fs::exists(legacy / "Sample"));
  assert(SystemSettings().GetString("migrate.legacy") == "partial" && SystemSettings().GetInt("migrate.left", -1) == 1);
  // Once the user resolves it, the next start finishes the job.
  fs::rename(legacy / "Project" / "SONG 1.ccproj", legacy / "Project" / "SONG 1 OLD.ccproj");
  result = MigrateLegacyLayout(synth);
  assert(result.moved == 1 && result.Complete() && SystemSettings().GetString("migrate.legacy") == "done");
  assert(ReadText(fs::path(synth.Projects()) / "SONG 1 OLD.ccproj") == "a different song");

  // Nothing happens without a card.
  WriteText(legacy / "Sample" / "late.wav", "late");
  CardenzaSim::Input::SetCardInserted(false);
  result = MigrateLegacyLayout(synth);
  assert(!result.found && fs::exists(legacy / "Sample" / "late.wav"));
  CardenzaSim::Input::SetCardInserted(true);
  assert(MigrateLegacyLayout(synth).moved == 1);
  std::cout << "legacy migration passed\n";
}

}  // namespace

int main()
{
  // The simulator fixes its card and internal folders under the working directory on first use.
  const auto root = fs::temp_directory_path() /
                    ("cardenza-storage-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(root);
  fs::current_path(root);
  assert(fs::path(Device::Storage::RootPath()).filename() == "sdcard");

  TestAppPaths();
  TestSettingsFormat();
  TestSettingsRoundTrip();
  TestAtomicWriteRecovery();
  TestMissingCardFallback();
  TestDeviceSettings();
  TestMigration();

  fs::current_path(fs::temp_directory_path());
  fs::remove_all(root);
  std::cout << "Storage protocol passed\n";
  return 0;
}
