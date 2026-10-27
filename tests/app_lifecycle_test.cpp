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
  groove->Sequencer().SetArpSettings(arp);
  Tap(2, 0);   // FN: SETUP
  assert(groove->Ui().SetupOpen());
  Tap(0, 12);  // OCTAVE
  Tap(0, 13);  // ADD
  Tap(2, 0);
  assert(groove->Ui().State().keyboard.octave == 1 && !groove->Ui().SetupOpen());
  // Fn + S on an unnamed project, ENTER takes the suggested name.
  Key(2, 0, true);
  Tap(2, 3);
  Key(2, 0, false);
  Tap(2, 13);
  assert(groove->Ui().State().projectName == "SONG");
  assert(fs::is_regular_file(synthFolder / "Projects" / "SONG.ccproj"));
  // Changes after the save live only in the session.
  groove->SetTrackLevel(1, 42);

  // Settings are written a second after the last change.
  Frames(70);
  text = ReadText(systemSettings);
  assert(Contains(text, "sys.volume=60\n") && Contains(text, "sys.led=7\n") && Contains(text, "sys.screen=150\n"));
  text = ReadText(synthFolder / "settings.cfg");
  assert(Contains(text, "keys.octave=1\n") && Contains(text, "arp.mode=1\n") && Contains(text, "arp.rate=2\n") &&
         Contains(text, "arp.gate=70\n"));
  // The project file carries no play settings and keeps writing format 4.
  text = ReadText(synthFolder / "Projects" / "SONG.ccproj");
  assert(text.find("CARDENZA_PROJECT 4\n") == 0 && !Contains(text, "PLAY"));

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
  assert(Contains(text, "pending=1\n") && Contains(text, "name=SONG\n"));
  assert(fs::is_regular_file(synthFolder / "Session" / "session.ccproj"));
  // A system app does not change the boot app.
  assert(Contains(ReadText(systemSettings), "boot.app=Cardenza/Synth\n"));

  // Back to the groovebox, as it was.
  SwitchTo("Cardenza/Synth");
  groove = &Groove();
  assert(groove->Sequencer().Tempo() == 133 && groove->Sequencer().BuildUiState(synth::kDefaultActiveTrack).steps[3].hasEvents);
  assert(groove->TrackLevel(1) == 42 && groove->Ui().State().projectName == "SONG");
  assert(groove->MasterVolume() == 60 && groove->LedBrightness() == 7 && groove->Backlight() == 150);
  assert(groove->Ui().State().keyboard.octave == 1 && groove->Sequencer().GetArpSettings() == arp);
  assert(Contains(ReadText(synthFolder / "Session" / "session.cfg"), "pending=0\n"));

  // Loading or creating a project leaves device and play settings alone.
  std::string message;
  groove::ProjectUiSettings ui;
  assert(groove->LoadProject("SONG", ui, message));
  assert(ui.keyboard.octave == 1 && groove->TrackLevel(1) == 100 && groove->Sequencer().Tempo() == 133);
  assert(groove->MasterVolume() == 60 && groove->LedBrightness() == 7 && groove->Backlight() == 150);
  assert(groove->Sequencer().GetArpSettings() == arp);
  assert(groove->NewProject(ui, message));
  assert(ui.keyboard.octave == 1 && ui.activeTrack == synth::kDefaultActiveTrack && groove->MasterVolume() == 60 && groove->Sequencer().GetArpSettings() == arp);

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
