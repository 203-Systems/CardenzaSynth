// App lifecycle on the real system: boot on a card from before the storage protocol, change device
// settings, play settings and the session in the groovebox, switch to another app and back, and
// check that nothing was lost and where everything was stored.
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "Applications/Application.h"
#include "Applications/Synth/GrooveApp.h"
#include "Devices/CardenzaSim/SimulatorInput.h"
#include "Devices/Device.h"
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

// A second app to switch to; a system app, so it is never remembered as the boot app.
class BlankApp : public app::Application {
public:
  void Setup(const std::vector<std::string>&) override {}
  void Tick() override {}
  void RenderFrame() override {}
};

constexpr const char* kBlankId = "Test/Blank";

void SwitchTo(const std::string& id)
{
  assert(os::system::SwitchApplication(id));
  Frames();
  assert(os::system::ActiveApplicationId() == id);
}

}  // namespace

int main()
{
  const auto root = fs::temp_directory_path() /
                    ("cardenza-lifecycle-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(root);
  fs::current_path(root);
  const fs::path card = root / "sdcard";
  const fs::path cardenza = card / "Cardenza";
  const fs::path synthFolder = cardenza / "Apps" / "Cardenza" / "Synth";
  const fs::path systemSettings = cardenza / "System" / "settings.cfg";

  // A card written by firmware before the storage protocol.
  WriteText(card / "CardenzaSynth" / "Sample" / "kick.wav", "RIFF not really a wave");
  WriteText(card / "CardenzaSynth" / "Project" / "OLD SONG.ccproj", "CARDENZA_PROJECT 4\n");

  app::ApplicationInfo blank;
  blank.id = kBlankId;
  blank.name = "Blank";
  blank.author = "Test";
  blank.isSystem = true;
  blank.factory = &app::MakeApplication<BlankApp>;
  assert(app::RegisterApplication(blank));
  os::system::UseClock(&FakeClock);
  assert(os::system::Begin());
  assert(os::system::ActiveApplicationId() == "Cardenza/Synth");
  auto* groove = &Groove();

  // Boot moved the old layout into place and wrote the system settings.
  assert(fs::is_regular_file(cardenza / "Shared" / "Samples" / "kick.wav"));
  assert(fs::is_regular_file(synthFolder / "Projects" / "OLD SONG.ccproj"));
  assert(!fs::exists(card / "CardenzaSynth"));
  const auto projects = groove->ListProjects();
  assert(std::find(projects.begin(), projects.end(), "OLD SONG") != projects.end());
  const auto samples = groove->ListSamples();
  assert(std::find(samples.begin(), samples.end(), "kick.wav") != samples.end());
  std::string text = ReadText(systemSettings);
  assert(Contains(text, "protocol=1\n") && Contains(text, "migrate.legacy=done\n") && Contains(text, "boot.app=Cardenza/Synth\n"));
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
  if (fs::exists(synthFolder / "settings.cfg"))
  {
    text = ReadText(synthFolder / "settings.cfg");
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

  // Card out: device settings are kept in internal flash and copied back when the card returns.
  CardenzaSim::Input::SetCardInserted(false);
  groove->SetMasterVolume(30);
  Frames(70);
  assert(fs::is_regular_file(root / "internal" / "sys.rec") && Contains(ReadText(systemSettings), "sys.volume=60\n"));
  CardenzaSim::Input::SetCardInserted(true);
  Frames(650);  // the system retries the card every ten seconds
  assert(!fs::exists(root / "internal" / "sys.rec") && Contains(ReadText(systemSettings), "sys.volume=30\n"));

  // An orderly shutdown suspends the app: the session is waiting for the next start.
  os::system::Shutdown();
  assert(os::system::ActiveApplication() == nullptr);
  assert(Contains(ReadText(synthFolder / "Session" / "session.cfg"), "pending=1\n"));

  fs::current_path(fs::temp_directory_path());
  fs::remove_all(root);
  std::cout << "App lifecycle passed\n";
  return 0;
}
