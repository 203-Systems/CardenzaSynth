// Protocol 3: the 0.1.0 release's layout and SD-only settings, in this firmware's folder <card>/Apps/CardenzaSynth
// (CardenzaHAL's card layout); protocol 2's <card>/CardenzaSynth moves there at the start, merged when both are
// there; the layouts before stay as they were. The simulator card is taken out and put back without resetting
// the settings cache, and last a run starts without one.
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "Devices/CardenzaSim/SimulatorInput.h"
#include "Devices/Device.h"
#include "OS/Storage/DeviceSettings.h"
#include "OS/Storage/Settings.h"
#include "OS/Storage/Storage.h"

namespace fs = std::filesystem;

namespace {
// What DeviceSettings carried over to the board (Device::Settings::CarryOver), and how often.
Device::Settings::OldValues carried;
int carryOvers = 0;
}  // namespace

namespace Device {
namespace Settings {
void CarryOver(const OldValues& old)
{
  carried = old;
  ++carryOvers;
}
}  // namespace Settings
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
  const std::string root = Card().generic_string() + "/Apps/CardenzaSynth";
  assert(Root() == root && Root() == Device::Storage::AppRoot());
  assert(LegacyRoot() == Card().generic_string() + "/CardenzaSynth");
  assert(kProtocolVersion == 3);
  assert(SystemDirectory() == root + "/Settings");
  assert(SystemSettingsPath() == root + "/Settings/system.cfg");
  assert(SharedDirectory() == root && SharedSamplesDirectory() == root + "/Samples");
  assert(LogDirectory() == root + "/Logs" && AppsDirectory() == root + "/Apps");
  const AppFolder synth("Cardenza/Synth");
  assert(synth.Valid() && synth.Id() == "Cardenza/Synth");
  assert(synth.Root() == root);
  assert(synth.SettingsPath() == root + "/Settings/synth.cfg");
  assert(synth.Projects() == root + "/Projects");
  assert(synth.Presets("DX7") == root + "/Presets/DX7");
  assert(synth.Session() == root + "/Session");
  assert(synth.Folder("Wavetables") == root + "/Wavetables");
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
  for (const char* folder : {"Settings", "Samples", "Projects", "Presets/DX7", "Presets/Juno", "Session"})
    assert(fs::is_directory(fs::path(root) / folder));
  assert(!fs::exists(fs::path(root) / "Apps") && !fs::exists(Card() / "Cardenza"));
  const AppFolder other("Other/Tool");
  assert(other.Root() == root + "/Apps/Other/Tool");
  assert(other.SettingsPath() == root + "/Apps/Other/Tool/Settings/app.cfg");
  assert(EnsureAppLayout(other, {}));
  assert(other.Projects() != synth.Projects() && other.Session() != synth.Session());
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
  assert(settings.Dirty() && settings.Flush() && !settings.Dirty());
  // The app folder is created on demand.
  assert(fs::is_regular_file(app.SettingsPath()));
  const std::string file = ReadText(app.SettingsPath());
  assert(file.find("# Cardenza settings: Test/RoundTrip (storage protocol 3)\n") == 0);
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
  const std::string path = (fs::path(SystemDirectory()) / "atomic.cfg").generic_string();
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
  assert(!WriteFileAtomic((fs::path(Root()) / "NoSuchFolder" / "x.cfg").generic_string(), "x"));

  // A settings file that only exists as a backup still loads, and the next flush restores it.
  const std::string settingsPath = SystemSettingsPath();
  assert(WriteFileAtomic(settingsPath, "protocol=2\nsys.volume=33\n"));
  fs::rename(settingsPath, settingsPath + ".bak");
  ReloadAllSettings();
  Settings& system = SystemSettings();
  assert(system.GetInt("sys.volume", 0) == 33);
  assert(system.SetInt("sys.led", 9) && FlushAllSettings());
  assert(fs::is_regular_file(settingsPath) && !fs::exists(settingsPath + ".bak"));
  assert(ReadText(settingsPath).find("sys.volume=33\nsys.led=9\n") != std::string::npos);
  std::cout << "atomic write and backup recovery passed\n";
}

void TestMissingCardRamOnly()
{
  ResetCard();
  Settings& system = SystemSettings();
  assert(system.SetInt("sys.volume", 50) && system.SetString("future.key", "retained") && FlushAllSettings());
  const std::string settingsPath = SystemSettingsPath();
  assert(ReadText(settingsPath).find("sys.volume=50\n") != std::string::npos);

  // Card out: changes stay dirty in RAM; there is no internal persistence or successful save.
  CardenzaSim::Input::SetCardInserted(false);
  assert(!CardPresent() && !CardMounted() && CardFreeBytes() == 0 && !EnsureSystemLayout());
  assert(system.SetInt("sys.volume", 70) && system.SetString("boot.app", "Cardenza/Synth"));
  assert(AnySettingsDirty() && !FlushAllSettings() && system.Dirty());
  assert(!fs::exists(Internal()));
  assert(ReadText(settingsPath).find("sys.volume=50\n") != std::string::npos);  // card untouched
  // A fresh store while the card is absent uses defaults, proving unsaved edits were RAM-only.
  Settings restarted("system", settingsPath);
  restarted.Load();
  assert(restarted.GetInt("sys.volume", 85) == 85 && !restarted.Has("boot.app"));
  assert(!FlushAllSettings() && AnySettingsDirty());
  Settings& app = AppSettings("Test/RamOnly");
  assert(app.SetInt("keys.octave", 2) && !app.Flush() && app.Dirty());
  const std::string appPath = AppFolder("Test/RamOnly").SettingsPath();
  // Model existing files on a card inserted later, including keys never loaded into this cache.
  WriteText(appPath, "keys.octave=0\nkeys.scale=3\nfuture.app=keep\n");
  WriteText(settingsPath, ReadText(settingsPath) + "future.inserted=keep too\n");

  // No ReloadAllSettings: insertion must preserve RAM edits and merge untouched card keys.
  CardenzaSim::Input::SetCardInserted(true);
  assert(RefreshAllSettingsFromCard() && system.Dirty() && app.Dirty());
  assert(system.GetInt("sys.volume", 0) == 70 && system.GetString("future.key") == "retained" &&
         system.GetString("future.inserted") == "keep too");
  assert(app.GetInt("keys.octave", 0) == 2 && app.GetInt("keys.scale", 0) == 3 && app.GetString("future.app") == "keep");
  assert(FlushAllSettings() && !AnySettingsDirty());
  const std::string synced = ReadText(settingsPath);
  assert(synced.find("sys.volume=70\n") != std::string::npos && synced.find("boot.app=Cardenza/Synth\n") != std::string::npos &&
         synced.find("future.inserted=keep too\n") != std::string::npos);
  assert(ReadText(appPath).find("keys.octave=2\n") != std::string::npos && ReadText(appPath).find("future.app=keep\n") != std::string::npos);
  assert(!fs::exists(Internal()));
  ReleaseAppSettings("Test/RamOnly");
  std::cout << "missing card RAM-only settings and late insertion passed\n";
}

void TestDeviceSettings()
{
  // The output volume, key lights and screen are the board's since 2026-11-23 (CardenzaHAL's system
  // settings): the app keeps none, and carries its old values over once, then forgets them.
  ResetCard();
  carryOvers = 0;
  ApplyDeviceSettings();
  assert(carryOvers == 0);  // nothing to carry
  WriteText(SystemSettingsPath(), "protocol=2\nsys.volume=55\nsys.volume.rev=3\nsys.led=9\nsys.screen=128\nsys.log=0\n");
  ReloadAllSettings();
  ApplyDeviceSettings();
  assert(carryOvers == 1 && carried.volume == 55 && carried.keyLights == 9 && carried.backlight == 128);
  assert(!LogEnabled());  // what the app still keeps is read as before
  assert(!SystemSettings().Has("sys.volume") && !SystemSettings().Has("sys.volume.rev") && !SystemSettings().Has("sys.led") &&
         !SystemSettings().Has("sys.screen"));
  assert(FlushAllSettings());
  const std::string text = ReadText(SystemSettingsPath());
  assert(text.find("sys.volume") == std::string::npos && text.find("sys.led") == std::string::npos &&
         text.find("sys.screen") == std::string::npos && text.find("sys.log=0\n") != std::string::npos);
  // Once: the next start carries nothing.
  ReloadAllSettings();
  ApplyDeviceSettings();
  assert(carryOvers == 1);
  // A volume of an older meaning (revision before 3) was reset to the default anyway: not carried.
  WriteText(SystemSettingsPath(), "protocol=2\nsys.volume=20\nsys.volume.rev=2\n");
  ReloadAllSettings();
  ApplyDeviceSettings();
  assert(carryOvers == 2 && carried.volume == -1 && carried.keyLights == -1 && carried.backlight == -1);
  assert(!SystemSettings().Has("sys.volume") && !SystemSettings().Has("sys.volume.rev"));
  std::cout << "device settings passed\n";
}

void TestScreenTheme()
{
  // sys.theme: a device setting stored by name, DARK unless someone picks another.
  ResetCard();
  ApplyDeviceSettings();
  assert(ScreenTheme() == Theme::Dark && kDefaultTheme == Theme::Dark && !SystemSettings().Has("sys.theme"));
  SetScreenTheme(Theme::Light);
  assert(ScreenTheme() == Theme::Light);
  assert(FlushAllSettings() && ReadText(SystemSettingsPath()).find("sys.theme=light\n") != std::string::npos);
  // It comes back from the card at the next start.
  ReloadAllSettings();
  ApplyDeviceSettings();
  assert(ScreenTheme() == Theme::Light);
  SetScreenTheme(Theme::Retro);
  assert(FlushAllSettings() && ReadText(SystemSettingsPath()).find("sys.theme=retro\n") != std::string::npos);
  // Read tolerantly: a name in any case; anything else (a number, an unknown name) is DARK.
  const auto readBack = [](const std::string& value) {
    WriteText(SystemSettingsPath(), "sys.theme=" + value + "\n");
    ReloadAllSettings();
    ApplyDeviceSettings();
    return ScreenTheme();
  };
  assert(readBack("Light") == Theme::Light && readBack("RETRO") == Theme::Retro && readBack("dark") == Theme::Dark);
  assert(readBack("sepia") == Theme::Dark && readBack("2") == Theme::Dark && readBack("") == Theme::Dark);
  // A value outside the list is stored as the default.
  SetScreenTheme(static_cast<Theme>(7));
  assert(ScreenTheme() == Theme::Dark && SystemSettings().GetString("sys.theme") == "dark");
  std::cout << "screen theme passed\n";
}

// A card protocol 2 wrote (the 0.1.0 release up to this firmware): <card>/CardenzaSynth moves to
// <card>/Apps/CardenzaSynth at the start, in one rename, and is read there; every byte stays as it was. The
// folders of the firmware before 0.1.0 inside it (Project, Preset, Sample) move with it, unread; protocol 1's
// <card>/Cardenza stays where it is.
void TestMoveFromProtocol2()
{
  ResetCard();
  const AppFolder synth("Cardenza/Synth");
  const fs::path old = Card() / "CardenzaSynth";
  const fs::path previous = Card() / "Cardenza";
  const std::vector<std::pair<std::string, std::string>> release{
      {"Settings/system.cfg", "protocol=2\nsys.theme=light\nfuture.system=kept\n"},
      {"Settings/synth.cfg", "keys.select=1\n"},
      {"Projects/DEMO-2.ccproj", "CARDENZA_PROJECT 6\n"},
      {"Presets/DX7/user.ccpreset", "CARDENZA_PRESETS 1 \"dx7\"\n"},
      {"Samples/kick.wav", "RIFF kick"},
      {"Session/autosave.ccproj", "CARDENZA_PROJECT 7\n"},
      {"Session/autosave.txt", "generation=4\nname=DEMO-2\n"},
      {"Session/session.cfg", "pending=1\n"},  // 0.1.0's own: not read
      {"Logs/log.txt", "==== started\n"},
      {"Project/ORIGINAL.ccproj", "original project bytes"},  // before 0.1.0: not read
      {"Preset/DX7/original.ccpreset", "original preset bytes"},
      {"Sample/original.wav", "original sample bytes"},
      {"notes.txt", "user notes"},
  };
  for (const auto& [name, text] : release) WriteText(old / name, text);
  const std::vector<std::pair<fs::path, std::string>> between{
      {previous / "System" / "settings.cfg", "protocol=1\nsys.volume=61\n"},
      {previous / "Apps" / "Cardenza" / "Synth" / "Projects" / "PREVIOUS.ccproj", "previous project bytes"},
      {previous / "Shared" / "Samples" / "previous.wav", "previous sample bytes"},
  };
  for (const auto& [path, text] : between) WriteText(path, text);

  assert(DecideCardAtStart() && !CardMissingThisRun());
  assert(!fs::exists(old) && fs::is_directory(Root()));
  for (const auto& [name, text] : release) assert(ReadText(fs::path(Root()) / name) == text);
  for (const auto& [path, text] : between) assert(ReadText(path) == text);
  ReloadAllSettings();
  ApplyDeviceSettings();
  assert(ScreenTheme() == Theme::Light && SystemSettings().GetString("future.system") == "kept");
  assert(AppSettings(synth.Id()).GetInt("keys.select", 0) == 1);
  const auto projects = ListDirectory(synth.Projects());
  assert(projects.size() == 1 && projects[0].name == "DEMO-2.ccproj");
  assert(ListDirectory(SharedSamplesDirectory()).size() == 1);
  // The ordinary APIs do not look into the older folders that moved with it.
  assert(EnsureAppLayout(synth, {"DX7", "Juno"}) && ListDirectory(synth.Presets("Juno")).empty());
  // Done: the next start finds nothing to move and changes nothing.
  const auto files = [](const fs::path& root) {
    std::vector<std::string> names;
    for (const auto& entry : fs::recursive_directory_iterator(root)) names.push_back(entry.path().generic_string());
    std::sort(names.begin(), names.end());
    return names;
  };
  const auto before = files(Card());
  assert(DecideCardAtStart() && files(Card()) == before);
  ReleaseAppSettings(synth.Id());
  assert(!fs::exists(Internal()));
  std::cout << "protocol 2's folder moved, every byte kept\n";
}

// Back on an older version after the move, the user saved there; this version again: what the new folder
// lacks moves in, what both have stays in the old folder (never overwritten), and both are read as before.
void TestMergeAfterDowngrade()
{
  ResetCard();
  const AppFolder synth("Cardenza/Synth");
  const fs::path old = Card() / "CardenzaSynth";
  const fs::path root = Device::Storage::AppRoot();
  WriteText(root / "Settings" / "system.cfg", "protocol=3\nsys.theme=retro\n");
  WriteText(root / "Projects" / "SONG 01.ccproj", "CARDENZA_PROJECT 8 new\n");
  WriteText(root / "Session" / "autosave.ccproj", "CARDENZA_PROJECT 8 newer firmware\n");
  // What the older version made and saved: its folder again, a project, its own settings and autosave.
  WriteText(old / "Settings" / "system.cfg", "protocol=2\nsys.theme=light\n");
  WriteText(old / "Projects" / "SONG 01.ccproj", "CARDENZA_PROJECT 7 old\n");
  WriteText(old / "Projects" / "DOWNGRADED.ccproj", "CARDENZA_PROJECT 7 made on the older version\n");
  WriteText(old / "Session" / "autosave.ccproj", "CARDENZA_PROJECT 7 older firmware\n");
  WriteText(old / "Samples" / "take.wav", "RIFF take");
  fs::create_directories(old / "Presets" / "Juno");

  assert(DecideCardAtStart());
  assert(ReadText(root / "Projects" / "DOWNGRADED.ccproj") == "CARDENZA_PROJECT 7 made on the older version\n");
  assert(ReadText(root / "Samples" / "take.wav") == "RIFF take" && fs::is_directory(root / "Presets" / "Juno"));
  // Never overwritten: the new folder keeps its own; the old one keeps the three it could not move.
  assert(ReadText(root / "Settings" / "system.cfg") == "protocol=3\nsys.theme=retro\n");
  assert(ReadText(root / "Projects" / "SONG 01.ccproj") == "CARDENZA_PROJECT 8 new\n");
  assert(ReadText(root / "Session" / "autosave.ccproj") == "CARDENZA_PROJECT 8 newer firmware\n");
  assert(ReadText(old / "Settings" / "system.cfg") == "protocol=2\nsys.theme=light\n");
  assert(ReadText(old / "Projects" / "SONG 01.ccproj") == "CARDENZA_PROJECT 7 old\n");
  assert(ReadText(old / "Session" / "autosave.ccproj") == "CARDENZA_PROJECT 7 older firmware\n");
  assert(!fs::exists(old / "Samples") && !fs::exists(old / "Presets") && !fs::exists(old / "Projects" / "DOWNGRADED.ccproj"));
  ReloadAllSettings();
  ApplyDeviceSettings();
  assert(ScreenTheme() == Theme::Retro);
  assert(ListDirectory(synth.Projects()).size() == 2);
  // Once the user moved those three by hand, the old folder goes at the next start.
  fs::remove(old / "Settings" / "system.cfg");
  fs::rename(old / "Projects" / "SONG 01.ccproj", root / "Projects" / "SONG 01 (old).ccproj");
  fs::remove(old / "Session" / "autosave.ccproj");
  assert(DecideCardAtStart() && !fs::exists(old));
  std::cout << "merge after a downgrade passed\n";
}

// A run that starts without a card makes no card operation until the next start, a card put in
// meanwhile included: the system decides it once (os::system::Begin calls DecideCardAtStart).
void TestRunWithoutCard()
{
  ResetCard();
  assert(EnsureSystemLayout());
  const std::string settingsPath = SystemSettingsPath();
  WriteText(settingsPath, "protocol=3\nsys.volume=47\n");
  // Protocol 2's folder too: a run without a card does not move it.
  const fs::path old = fs::path(LegacyRoot()) / "Projects" / "OLD.ccproj";
  WriteText(old, "CARDENZA_PROJECT 6\n");
  CardenzaSim::Input::SetCardInserted(false);
  assert(!DecideCardAtStart() && CardMissingThisRun());
  CardenzaSim::Input::SetCardInserted(true);
  assert(!CardPresent() && !CardMounted() && CardCapacityBytes() == 0 && !EnsureSystemLayout());
  assert(!Exists(settingsPath) && !IsDirectory(Root()) && ListDirectory(SystemDirectory()).empty());
  std::string text;
  assert(!ReadFileWithBackup(settingsPath, text, 1024) && !WriteFileAtomic(settingsPath, "x=1\n"));
  assert(!CreateDirectories(JoinPath(Root(), "New")) && !RemoveFile(settingsPath));
  const AppFolder synth("Cardenza/Synth");
  assert(!EnsureAppLayout(synth, {"DX7"}));
  // The settings start from their defaults and stay in RAM.
  ReloadAllSettings();
  Settings& system = SystemSettings();
  assert(system.GetInt("sys.volume", 85) == 85 && system.SetInt("sys.volume", 30));
  assert(!RefreshAllSettingsFromCard() && !FlushAllSettings() && AnySettingsDirty() && system.GetInt("sys.volume", 0) == 30);
  ReportCardIoError();
  assert(ReadText(settingsPath) == "protocol=3\nsys.volume=47\n" && ReadText(old) == "CARDENZA_PROJECT 6\n");
  assert(!fs::exists(fs::path(Root()) / "New") && !fs::exists(settingsPath + ".tmp") && !fs::exists(synth.Presets("DX7")));
  // The next start with the card reads it again, and moves the old folder in.
  assert(DecideCardAtStart() && !CardMissingThisRun());
  assert(!fs::exists(LegacyRoot()) && ReadText(fs::path(synth.Projects()) / "OLD.ccproj") == "CARDENZA_PROJECT 6\n");
  ReloadAllSettings();
  assert(SystemSettings().GetInt("sys.volume", 0) == 47);
  std::cout << "run without a card passed\n";
}

}  // namespace

int main()
{
  // The simulator fixes its card folder under the working directory on first use.
  const auto root = fs::temp_directory_path() /
                    ("cardenza-storage-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(root);
  fs::current_path(root);
  assert(fs::path(Device::Storage::RootPath()).filename() == "sdcard");
  if (!fs::equivalent(Card().parent_path(), root))
  {
    std::cerr << "Storage tests require an isolated card under their temporary working directory\n";
    return 1;
  }

  TestAppPaths();
  TestSettingsFormat();
  TestSettingsRoundTrip();
  TestAtomicWriteRecovery();
  TestMissingCardRamOnly();
  TestDeviceSettings();
  TestScreenTheme();
  TestMoveFromProtocol2();
  TestMergeAfterDowngrade();
  TestRunWithoutCard();
  assert(!fs::exists(Internal()));

  fs::current_path(fs::temp_directory_path());
  fs::remove_all(root);
  std::cout << "Storage protocol passed\n";
  return 0;
}
