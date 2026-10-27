// App lifecycle with canonical SD-only storage: device/play settings, session handoff and card
// insertion. Earlier card layouts remain untouched and are never imported implicitly.
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

#include "Applications/Application.h"
#include "Applications/Synth/GrooveApp.h"
#include "Devices/CardenzaSim/SimulatorInput.h"
#include "Devices/Device.h"
#include "OS/Storage/DeviceSettings.h"
#include "OS/Storage/Settings.h"
#include "OS/Storage/Storage.h"
#include "OS/System/System.h"

namespace fs = std::filesystem;

namespace {

std::uint32_t gNow = 10000;
std::uint32_t FakeClock() { return gNow; }

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

bool Contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

void Frames(int count = 1)
{
  for (int index = 0; index < count; ++index)
  {
    gNow += 16;
    os::system::RunFrame();
  }
}

void Key(std::size_t row, std::size_t column, bool pressed)
{
  CardenzaSim::Input::SetMatrixKey(row, column, pressed);
  Frames();
}

void Tap(std::size_t row, std::size_t column)
{
  Key(row, column, true);
  Key(row, column, false);
}

apps::GrooveApp& Groove()
{
  auto* groove = dynamic_cast<apps::GrooveApp*>(os::system::ActiveApplication());
  assert(groove != nullptr);
  groove->UseClock(&FakeClock);
  return *groove;
}

void SetRollSelection(apps::GrooveApp& groove, bool multi)
{
  auto ui = groove.Ui().Settings();
  ui.keyboard.multiSelect = multi;
  groove.Ui().Restore(ui, groove.Ui().State().projectName);
}

// A second app to switch to; a system app, so it is never remembered as the boot app.
class BlankApp : public app::Application {
public:
  void Setup(const std::vector<std::string>&) override {}
  void Tick() override {}
  void RenderFrame() override {}
};

constexpr const char* kBlankId = "Test/Blank";

void RegisterBlankApp()
{
  app::ApplicationInfo blank;
  blank.id = kBlankId;
  blank.name = "Blank";
  blank.author = "Test";
  blank.isSystem = true;
  blank.factory = &app::MakeApplication<BlankApp>;
  assert(app::RegisterApplication(blank));
}

void SwitchTo(const std::string& id)
{
  assert(os::system::SwitchApplication(id));
  Frames();
  assert(os::system::ActiveApplicationId() == id);
}

void TestLateCardBoot(const fs::path& root)
{
  const fs::path product = root / "sdcard" / "CardenzaSynth";
  const fs::path systemPath = product / "Settings" / "system.cfg";
  const fs::path synthPath = product / "Settings" / "synth.cfg";
  const std::string oldSystem = "protocol=2\nsys.volume=47\nsys.led=5\nsys.screen=111\nfuture.system=keep\n";
  const std::string oldSynth = "keys.octave=2\nkeys.scale=3\narp.rate=4\nfuture.synth=keep\n";
  WriteText(systemPath, oldSystem);
  WriteText(synthPath, oldSynth);
  CardenzaSim::Input::SetCardInserted(false);
  os::system::UseClock(&FakeClock);
  assert(os::system::Begin());
  auto& groove = Groove();
  assert(!groove.QuerySystem().storageReady);
  assert(ReadText(systemPath) == oldSystem && ReadText(synthPath) == oldSynth);

  groove.SetMasterVolume(30);
  SetRollSelection(groove, true);
  Frames(70);
  assert(os::storage::AnySettingsDirty());
  assert(ReadText(systemPath) == oldSystem && ReadText(synthPath) == oldSynth);
  assert(!fs::exists(root / "internal"));

  // Existing keys were never loaded at startup. The real periodic retry must merge them without
  // an explicit ReloadAllSettings or replacing untouched card values with app startup defaults.
  CardenzaSim::Input::SetCardInserted(true);
  gNow += 6000;
  Frames(70);
  assert(groove.QuerySystem().storageReady && !os::storage::AnySettingsDirty());
  assert(groove.MasterVolume() == 30 && groove.LedBrightness() == 5 && groove.Backlight() == 111);
  std::string text = ReadText(systemPath);
  assert(Contains(text, "sys.volume=30\n") && Contains(text, "sys.led=5\n") && Contains(text, "sys.screen=111\n") &&
         Contains(text, "future.system=keep\n") && Contains(text, "protocol=2\n"));
  text = ReadText(synthPath);
  assert(Contains(text, "keys.select=1\n") && Contains(text, "arp.rate=4\n") && Contains(text, "keys.octave=2\n") &&
         Contains(text, "keys.scale=3\n") && Contains(text, "future.synth=keep\n"));

  // Removal/reinsertion updates the status while preserving another unsaved edit and a card edit.
  CardenzaSim::Input::SetCardInserted(false);
  Frames();
  assert(!groove.QuerySystem().storageReady);
  groove.SetMasterVolume(35);
  Frames(70);
  assert(os::storage::AnySettingsDirty() && Contains(ReadText(systemPath), "sys.volume=30\n"));
  WriteText(systemPath, ReadText(systemPath) + "future.reinsert=kept too\n");
  CardenzaSim::Input::SetCardInserted(true);
  gNow += 6000;
  Frames(70);
  assert(groove.QuerySystem().storageReady && !os::storage::AnySettingsDirty());
  assert(Contains(ReadText(systemPath), "sys.volume=35\n") && Contains(ReadText(systemPath), "future.reinsert=kept too\n"));
  assert(!fs::exists(root / "internal") && !fs::exists(root / "sdcard" / "Cardenza"));
  assert(!fs::exists(product / "Apps" / "Cardenza" / "Synth"));

  // One unreadable store must not stop healthy stores from being saved on later retries.
  const std::string savedSynth = ReadText(synthPath);
  fs::remove(synthPath);
  fs::create_directory(synthPath);
  SetRollSelection(groove, false);
  groove.SetMasterVolume(36);
  Frames(70);
  assert(os::storage::AppSettings("Cardenza/Synth").Dirty() && fs::is_directory(synthPath));
  assert(Contains(ReadText(systemPath), "sys.volume=36\n"));
  // The first failure makes the system re-prepare storage. Its aggregate refresh result must not
  // globally block a new system edit just because the app's settings path is still a directory.
  groove.SetMasterVolume(37);
  gNow += 6000;
  Frames(70);
  assert(Contains(ReadText(systemPath), "sys.volume=37\n"));
  assert(!os::storage::SystemSettings().Dirty() && os::storage::AppSettings("Cardenza/Synth").Dirty());
  assert(fs::is_directory(synthPath));
  fs::remove(synthPath);
  WriteText(synthPath, savedSynth);
  gNow += 6000;
  Frames(70);
  assert(!os::storage::AnySettingsDirty());
  assert(Contains(ReadText(synthPath), "keys.select=0\n") && Contains(ReadText(synthPath), "future.synth=keep\n"));
  assert(Contains(ReadText(systemPath), "sys.volume=37\n") && !fs::exists(root / "internal"));
  os::system::Shutdown();
  assert(os::system::ActiveApplication() == nullptr && !fs::exists(root / "internal"));
  std::cout << "Late-card lifecycle passed\n";
}

void RemoveCleanCardAndFailRetry()
{
  Frames(70);
  assert(!os::storage::AnySettingsDirty());
  CardenzaSim::Input::SetCardInserted(false);
  Frames();
  gNow += 6000;
  Frames();
  assert(!os::storage::CardMounted() && !os::storage::AnySettingsDirty());
}

using LegacyFiles = std::vector<std::pair<fs::path, std::string>>;

LegacyFiles InsertLegacyCard(const fs::path& root, const std::string& marker)
{
  assert(!os::storage::CardMounted());
  const fs::path card = root / "sdcard";
  // The test's device root was checked to be inside this temporary directory before boot.
  fs::remove_all(card);
  const fs::path original = card / "CardenzaSynth";
  const fs::path previous = card / "Cardenza";
  const fs::path oldApp = previous / "Apps" / "Cardenza" / "Synth";
  LegacyFiles files{
      {original / "Project" / "ORIGINAL.ccproj", "original project " + marker},
      {original / "Preset" / "DX7" / "original.ccpreset", "original preset " + marker},
      {original / "Sample" / "original.wav", "original sample " + marker},
      {previous / "System" / "settings.cfg",
       "protocol=1\nsys.volume=65\nsys.led=6\nsys.screen=146\nfuture.system=" + marker + "\n"},
      {previous / "System" / "settings.cfg.bak", "protocol=1\nsys.volume=66\n"},
      {oldApp / "settings.cfg", "keys.octave=2\nkeys.scale=3\nfuture.synth=" + marker + "\n"},
      {oldApp / "settings.cfg.tmp", "uncommitted " + marker},
      {oldApp / "Projects" / "PREVIOUS.ccproj", "previous project " + marker},
      {oldApp / "Session" / "session.cfg", "pending=1\nname=PREVIOUS\nfuture.session=" + marker + "\n"},
      {previous / "Shared" / "Samples" / "previous.wav", "previous sample " + marker},
  };
  for (const auto& file : files) WriteText(file.first, file.second);
  assert(!fs::exists(original / "Settings"));
  CardenzaSim::Input::SetCardInserted(true);
  return files;
}

void CheckLegacyFilesUntouched(const LegacyFiles& files)
{
  for (const auto& file : files)
    assert(fs::is_regular_file(file.first) && ReadText(file.first) == file.second);
}

void CheckCanonicalSettingsIgnoreLegacy(const fs::path& root, const LegacyFiles& files, int editedVolume)
{
  CheckLegacyFilesUntouched(files);
  const fs::path product = root / "sdcard" / "CardenzaSynth";
  const std::string text = ReadText(product / "Settings" / "system.cfg");
  assert(Contains(text, "sys.volume=" + std::to_string(editedVolume) + "\n"));
  assert(!Contains(text, "future.system") && !Contains(text, "protocol=1\n"));
  os::storage::Settings system("check", (product / "Settings" / "system.cfg").generic_string());
  system.Load();
  assert(system.GetInt("sys.led", os::storage::kDefaultKeyLedBrightness) == os::storage::kDefaultKeyLedBrightness);
  assert(system.GetInt("sys.screen", os::storage::kDefaultScreenBacklight) == os::storage::kDefaultScreenBacklight);
  os::storage::Settings synth("check", (product / "Settings" / "synth.cfg").generic_string());
  synth.Load();
  assert(synth.GetInt("keys.octave", 0) == 0 && synth.GetInt("keys.scale", 0) == 0 && !synth.Has("future.synth"));
  assert(!fs::exists(product / "Projects" / "ORIGINAL.ccproj") && !fs::exists(product / "Projects" / "PREVIOUS.ccproj"));
  assert(!fs::exists(product / "Samples" / "original.wav") && !fs::exists(product / "Samples" / "previous.wav"));
  assert(!fs::exists(product / "Presets" / "DX7" / "original.ccpreset"));
  assert(!Contains(ReadText(product / "Session" / "session.cfg"), "PREVIOUS"));
  assert(!fs::exists(root / "internal"));
}

void TestLegacyCardIsNotImported(const fs::path& root)
{
  RegisterBlankApp();
  os::system::UseClock(&FakeClock);
  assert(os::system::Begin());
  auto* groove = &Groove();
  assert(groove->QuerySystem().storageReady);

  // Insert an older card before the next scheduled probe. Ordinary write-behind may create new
  // canonical files, but it must never read, move, rename or rewrite either earlier layout.
  RemoveCleanCardAndFailRetry();
  const std::uint32_t failedProbeAt = gNow;
  LegacyFiles files = InsertLegacyCard(root, "write-behind");
  groove->SetMasterVolume(28);
  SetRollSelection(*groove, true);
  Frames(70);
  assert(gNow - failedProbeAt < 5000);
  CheckLegacyFilesUntouched(files);
  gNow += 6000;
  Frames(70);
  CheckCanonicalSettingsIgnoreLegacy(root, files, 28);
  assert(Contains(ReadText(root / "sdcard" / "CardenzaSynth" / "Settings" / "synth.cfg"), "keys.select=1\n"));
  assert(!os::storage::AnySettingsDirty() && groove->QuerySystem().storageReady);
  assert(groove->MasterVolume() == 28 && groove->LedBrightness() == os::storage::kDefaultKeyLedBrightness &&
         groove->Backlight() == os::storage::kDefaultScreenBacklight);

  // Suspend and shutdown may save current RAM state only into the canonical layout.
  RemoveCleanCardAndFailRetry();
  files = InsertLegacyCard(root, "switch");
  groove->SetMasterVolume(29);
  SwitchTo(kBlankId);
  CheckCanonicalSettingsIgnoreLegacy(root, files, 29);
  assert(!os::storage::AnySettingsDirty());
  SwitchTo("Cardenza/Synth");
  groove = &Groove();

  RemoveCleanCardAndFailRetry();
  files = InsertLegacyCard(root, "shutdown");
  groove->SetMasterVolume(30);
  os::system::Shutdown();
  assert(os::system::ActiveApplication() == nullptr);
  CheckCanonicalSettingsIgnoreLegacy(root, files, 30);
  assert(!os::storage::AnySettingsDirty());
  std::cout << "Legacy-card files remain untouched through write/switch/shutdown\n";
}

}  // namespace

int main(int argc, char** argv)
{
  const auto root = fs::temp_directory_path() /
                    ("cardenza-lifecycle-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(root);
  fs::current_path(root);
  if (!fs::equivalent(fs::path(Device::Storage::RootPath()).parent_path(), root))
  {
    std::cerr << "Lifecycle tests require an isolated card under their temporary working directory\n";
    return 1;
  }
  if (argc == 2 && std::string(argv[1]) == "--late-card")
  {
    TestLateCardBoot(root);
    fs::current_path(fs::temp_directory_path());
    fs::remove_all(root);
    return 0;
  }
  if (argc == 2 && std::string(argv[1]) == "--legacy-card")
  {
    TestLegacyCardIsNotImported(root);
    fs::current_path(fs::temp_directory_path());
    fs::remove_all(root);
    return 0;
  }
  assert(argc == 1);
  const fs::path card = root / "sdcard";
  const fs::path cardenza = card / "CardenzaSynth";
  const fs::path synthFolder = cardenza;
  const fs::path systemSettings = cardenza / "Settings" / "system.cfg";

  // Existing documents already use the canonical layout.
  WriteText(cardenza / "Samples" / "kick.wav", "RIFF not really a wave");
  WriteText(cardenza / "Projects" / "EXISTING SONG.ccproj", "CARDENZA_PROJECT 4\n");

  RegisterBlankApp();
  os::system::UseClock(&FakeClock);
  assert(os::system::Begin());
  assert(os::system::ActiveApplicationId() == "Cardenza/Synth");
  auto* groove = &Groove();

  // Boot reads the canonical layout and writes the system settings.
  assert(fs::is_regular_file(cardenza / "Samples" / "kick.wav"));
  assert(fs::is_regular_file(synthFolder / "Projects" / "EXISTING SONG.ccproj"));
  assert(!fs::exists(cardenza / "Sample") && !fs::exists(cardenza / "Project") && !fs::exists(card / "Cardenza"));
  const auto projects = groove->ListProjects();
  assert(std::find(projects.begin(), projects.end(), "EXISTING SONG") != projects.end());
  const auto samples = groove->ListSamples();
  assert(std::find(samples.begin(), samples.end(), "kick.wav") != samples.end());
  std::string text = ReadText(systemSettings);
  assert(Contains(text, "protocol=2\n") && Contains(text, "boot.app=Cardenza/Synth\n"));
  for (const char* folder : {"Projects", "Presets/DX7", "Presets/Drums", "Session"}) assert(fs::is_directory(synthFolder / folder));

  // Device settings, play settings and the session.
  groove->SetMasterVolume(60);
  groove->SetLedBrightness(7);
  groove->SetBacklight(150);
  assert(Device::LED::GlobalBrightness() == 7 && Device::Display::Backlight() == 150);
  groove->Sequencer().SetTempo(133);
  const std::uint8_t note = 64;
  groove->Sequencer().PressStep(3, &note, 1);
  groove->Sequencer().ReleaseStep(3);
  synth::ArpSettings arp;
  arp.mode = synth::ArpMode::Up;
  arp.rate = 2;
  arp.gate = 70;
  groove->Sequencer().SetArpSettings(synth::kDefaultActiveTrack, arp);  // T2's arpeggiator
  Key(0, 0, true);  // ESC + GRID column 1: SETUP
  Tap(0, 9);
  Key(0, 0, false);
  assert(groove->Ui().State().scene == groove::Scene::Setup);
  while (groove->Ui().CurrentPage() != groove::kSetupKeys) Tap(3, 0);  // CTRL to KEYS
  Tap(0, 12);  // OCTAVE
  Tap(0, 13);  // ADD
  Tap(0, 0);   // ESC: back
  assert(groove->Ui().State().keyboard.octave == 1 && groove->Ui().State().scene != groove::Scene::Setup);
  // Fn + S on an unnamed project saves it right away as the first free SONG n (no name field).
  Key(2, 0, true);
  Tap(2, 3);
  Key(2, 0, false);
  assert(groove->Ui().State().projectName == "SONG 01");
  assert(fs::is_regular_file(synthFolder / "Projects" / "SONG 01.ccproj"));
  // Changes after the save live only in the session.
  groove->SetTrackLevel(1, 42);

  // Settings are written a second after the last change.
  Frames(70);
  text = ReadText(systemSettings);
  assert(Contains(text, "sys.volume=60\n") && Contains(text, "sys.led=7\n") && Contains(text, "sys.screen=150\n"));
  // The play settings are project data (format 6): the keyboard and each track's chord and arp.
  if (fs::exists(synthFolder / "Settings" / "synth.cfg"))
  {
    text = ReadText(synthFolder / "Settings" / "synth.cfg");
    assert(!Contains(text, "arp.") && !Contains(text, "keys.octave"));
  }
  text = ReadText(synthFolder / "Projects" / "SONG 01.ccproj");
  assert(text.find("CARDENZA_PROJECT 6\n") == 0 && Contains(text, "\nUI 1 1 0 0 0 ") && Contains(text, "\nKEYS 0 110\n"));
  assert(Contains(text, "\nPLAYFX 1 0 0 1 2 1 70 0\n") && Contains(text, "\nPLAYFX 0 0 0 0 3 1 50 0\n"));

  // A short HOME press is still ALL STOP in the groovebox.
  const std::uint32_t panic = groove->Ui().State().panicAt;
  CardenzaSim::Input::SetHomeButton(true);
  Frames();
  CardenzaSim::Input::SetHomeButton(false);
  Frames();
  assert(os::system::ActiveApplicationId() == "Cardenza/Synth" && groove->Ui().State().panicAt != panic);

  // Holding HOME stays with the app: this firmware has no launcher of its own.
  CardenzaSim::Input::SetHomeButton(true);
  Frames(80);
  CardenzaSim::Input::SetHomeButton(false);
  Frames();
  assert(os::system::ActiveApplicationId() == "Cardenza/Synth");
  groove = &Groove();

  // Another app: the groovebox is suspended (session saved) and destroyed.
  SwitchTo(kBlankId);
  text = ReadText(synthFolder / "Session" / "session.cfg");
  assert(Contains(text, "pending=1\n") && Contains(text, "name=SONG 01\n"));
  assert(fs::is_regular_file(synthFolder / "Session" / "session.ccproj"));
  // A system app does not change the boot app.
  assert(Contains(ReadText(systemSettings), "boot.app=Cardenza/Synth\n"));

  // Back to the groovebox, as it was.
  SwitchTo("Cardenza/Synth");
  groove = &Groove();
  assert(groove->Sequencer().Tempo() == 133 && groove->Sequencer().BuildUiState(synth::kDefaultActiveTrack).steps[3].hasEvents);
  assert(groove->TrackLevel(1) == 42 && groove->Ui().State().projectName == "SONG 01");
  assert(groove->MasterVolume() == 60 && groove->LedBrightness() == 7 && groove->Backlight() == 150);
  assert(groove->Ui().State().keyboard.octave == 1 && groove->Sequencer().GetArpSettings(synth::kDefaultActiveTrack) == arp);
  assert(groove->Sequencer().GetArpSettings(0) == synth::ArpSettings{});
  assert(Contains(ReadText(synthFolder / "Session" / "session.cfg"), "pending=0\n"));

  // Loading a project brings back its play settings; device settings stay. NEW starts from the
  // default play settings.
  std::string message;
  groove::ProjectUiSettings ui;
  groove->Sequencer().SetArpSettings(synth::kDefaultActiveTrack, synth::ArpSettings{});
  assert(groove->LoadProject("SONG 01", ui, message));
  assert(ui.keyboard.octave == 1 && groove->TrackLevel(1) == 100 && groove->Sequencer().Tempo() == 133);
  assert(groove->MasterVolume() == 60 && groove->LedBrightness() == 7 && groove->Backlight() == 150);
  assert(groove->Sequencer().GetArpSettings(synth::kDefaultActiveTrack) == arp);
  assert(groove->NewProject(ui, message));
  assert(ui.keyboard.octave == 0 && ui.activeTrack == synth::kDefaultActiveTrack && groove->MasterVolume() == 60);
  assert(groove->Sequencer().GetArpSettings(synth::kDefaultActiveTrack) == synth::ArpSettings{});

  assert(!os::system::SwitchApplication("Nobody/Here"));

  // Card out: edited device settings remain dirty in RAM until the periodic SD retry succeeds.
  CardenzaSim::Input::SetCardInserted(false);
  groove->SetMasterVolume(30);
  Frames(70);
  assert(!groove->QuerySystem().storageReady && os::storage::AnySettingsDirty());
  assert(!fs::exists(root / "internal") && Contains(ReadText(systemSettings), "sys.volume=60\n"));
  CardenzaSim::Input::SetCardInserted(true);
  gNow += 6000;
  Frames(70);
  assert(groove->QuerySystem().storageReady && !os::storage::AnySettingsDirty());
  assert(!fs::exists(root / "internal") && Contains(ReadText(systemSettings), "sys.volume=30\n"));

  // An orderly shutdown suspends the app: the session is waiting for the next start.
  os::system::Shutdown();
  assert(os::system::ActiveApplication() == nullptr);
  assert(Contains(ReadText(synthFolder / "Session" / "session.cfg"), "pending=1\n"));
  assert(!fs::exists(root / "internal"));

  fs::current_path(fs::temp_directory_path());
  fs::remove_all(root);
  std::cout << "App lifecycle passed\n";
  return 0;
}
