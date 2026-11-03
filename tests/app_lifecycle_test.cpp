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
#include <map>
#include <string>
#include <vector>

#include "Applications/Application.h"
#include "Applications/Synth/GrooveApp.h"
#include "Applications/Synth/Runtime/Sampler/SamplerWav.h"
#include "Devices/CardenzaSim/SimulatorInput.h"
#include "Devices/Device.h"
#include "OS/Storage/DeviceSettings.h"
#include "OS/Storage/Storage.h"
#include "OS/System/Log.h"
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

// What a power cut would leave on the card right now, and that state being what the next start finds.
void CopyFolder(const fs::path& from, const fs::path& to)
{
  fs::remove_all(to);
  fs::create_directories(to.parent_path());
  fs::copy(from, to, fs::copy_options::recursive);
}

// When each record in a folder was last written.
std::map<std::string, fs::file_time_type> WriteTimes(const fs::path& folder)
{
  std::map<std::string, fs::file_time_type> times;
  for (const auto& entry : fs::directory_iterator(folder)) times[entry.path().filename().string()] = entry.last_write_time();
  return times;
}

std::uintmax_t AutosaveBytes(const fs::path& folder)
{
  std::uintmax_t bytes = 0;
  for (const auto& entry : fs::directory_iterator(folder))
    if (entry.path().filename().string().rfind("as_", 0) == 0) bytes += entry.file_size();
  return bytes;
}

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
  // And a volume saved before the output got louder: everyone starts again from the default, once.
  WriteText(systemSettings, "protocol=1\nsys.volume=100\n");
  // Records of an earlier firmware that nothing reads any more, and one that is not the autosave's.
  WriteText(root / "internal" / "as_p0_99.rec", "stale");
  WriteText(root / "internal" / "as_old.rec", "stale");
  WriteText(root / "internal" / "launcher.rec", "theirs");

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
  assert(groove->MasterVolume() == 50 && os::storage::kDefaultOutputVolume == 50);
  // The clock jumps minutes at a time here: standby would take the next key for waking up.
  assert(groove->StandbyMinutes() == os::storage::kDefaultStandbyMinutes);
  groove->SetStandbyMinutes(0);
  assert(!fs::exists(root / "internal" / "as_p0_99.rec") && !fs::exists(root / "internal" / "as_old.rec"));
  assert(ReadText(root / "internal" / "launcher.rec") == "theirs");

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

  // The console log (OS/System/Log.h): on by default, written to Cardenza/Logs every few seconds.
  const fs::path logFile = cardenza / "Logs" / "log.txt";
  assert(os::storage::LogEnabled() && os::log::FilePath() == logFile.generic_string());
  gNow += os::log::kWriteIntervalMs;
  Frames(1);
  Device::Log::Write("test: first line");
  Frames(1);
  assert(!Contains(ReadText(logFile), "test: first line"));  // not every frame
  gNow += os::log::kWriteIntervalMs;
  Frames(1);
  assert(Contains(ReadText(logFile), "test: first line\n") && Device::Log::Pending() == 0);
  // Off: nothing goes to the card, the newest lines wait in the ring and come when it is on again.
  groove->SetLogging(false);
  assert(!os::storage::LogEnabled());
  Device::Log::Write("test: while off");
  gNow += os::log::kWriteIntervalMs;
  Frames(1);
  assert(!Contains(ReadText(logFile), "while off") && !os::log::Flush());
  groove->SetLogging(true);
  assert(os::log::Flush() && Contains(ReadText(logFile), "test: while off\n"));
  // More than the ring holds before a write: the file says what is missing.
  for (int line = 0; line < 2000; ++line) Device::Log::Write(std::string(60, 'x'));
  assert(os::log::Flush() && Contains(ReadText(logFile), "bytes lost here]"));
  // Past its size the file becomes log.1.txt and a new one starts.
  for (int round = 0; round < 12 && !fs::exists(cardenza / "Logs" / "log.1.txt"); ++round)
  {
    for (int line = 0; line < 900; ++line) Device::Log::Write(std::string(60, 'y'));
    // One write takes a bounded amount: the rest waits for the next.
    while (Device::Log::Pending() > 0) assert(os::log::Flush());
  }
  assert(fs::exists(cardenza / "Logs" / "log.1.txt") && fs::file_size(cardenza / "Logs" / "log.1.txt") > static_cast<std::uintmax_t>(os::log::kMaxFileBytes));
  Device::Log::Write("test: after the rotation");
  assert(os::log::Flush() && Contains(ReadText(logFile), "after the rotation") &&
         fs::file_size(logFile) < static_cast<std::uintmax_t>(os::log::kMaxFileBytes));

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
  groove->Sequencer().SetArpSettings(synth::kDefaultActiveTrack, arp);  // T1's arpeggiator
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
  // The play settings are project data (since format 6): the keyboard and each track's chord and arp.
  if (fs::exists(synthFolder / "settings.cfg"))
  {
    text = ReadText(synthFolder / "settings.cfg");
    assert(!Contains(text, "arp.") && !Contains(text, "keys.octave"));
  }
  text = ReadText(synthFolder / "Projects" / "SONG 01.ccproj");
  assert(text.find("CARDENZA_PROJECT 7\n") == 0 && Contains(text, "\nUI 0 1 0 0 0 ") && Contains(text, "\nKEYS 0 110\n"));
  assert(Contains(text, "\nPLAYFX 0 0 0 1 2 1 70 0\n") && Contains(text, "\nPLAYFX 1 0 0 0 3 1 50 0\n"));

  // A short HOME press does nothing in the groovebox: the key is unassigned for now.
  const std::uint32_t panic = groove->Ui().State().panicAt;
  CardenzaSim::Input::SetHomeButton(true);
  Frames();
  CardenzaSim::Input::SetHomeButton(false);
  Frames();
  assert(os::system::ActiveApplicationId() == "Cardenza/Synth" && groove->Ui().State().panicAt == panic);

  // Holding HOME stays with the app: this firmware has no launcher of its own.
  CardenzaSim::Input::SetHomeButton(true);
  Frames(80);
  CardenzaSim::Input::SetHomeButton(false);
  Frames();
  assert(os::system::ActiveApplicationId() == "Cardenza/Synth");
  groove = &Groove();

  // Another app: the groovebox is suspended (session saved) and destroyed.
  SwitchTo(kBlankId);
  const fs::path sessionFolder = synthFolder / "Session";
  const fs::path internal = root / "internal";
  // The working state is in internal flash, whatever the card.
  assert(Contains(ReadText(internal / "as_g.rec"), "SONG 01"));
  // A system app does not change the boot app.
  assert(Contains(ReadText(systemSettings), "boot.app=Cardenza/Synth\n"));

  // Back to the groovebox, as it was.
  SwitchTo("Cardenza/Synth");
  groove = &Groove();
  assert(groove->Sequencer().Tempo() == 133 && groove->Sequencer().BuildUiState(synth::kDefaultActiveTrack).steps[3].hasEvents);
  assert(groove->TrackLevel(1) == 42 && groove->Ui().State().projectName == "SONG 01");
  assert(groove->MasterVolume() == 60 && groove->LedBrightness() == 7 && groove->Backlight() == 150);
  assert(groove->Ui().State().keyboard.octave == 1 && groove->Sequencer().GetArpSettings(synth::kDefaultActiveTrack) == arp);
  assert(groove->Sequencer().GetArpSettings(1) == synth::ArpSettings{});
  assert(!fs::exists(internal / "as_boot.rec"));

  // The power can go at any moment. Nothing tells the app: what counts is what is in flash then.
  const fs::path cut = root / "power-cut";
  const auto powerCut = [&]() {
    CopyFolder(internal, cut / "internal");
    CopyFolder(sessionFolder, cut / "session");
  };
  const auto restart = [&]() {
    // The switch stands in for the restart; flash and card are put back to what the cut left.
    SwitchTo(kBlankId);
    CopyFolder(cut / "internal", internal);
    CopyFolder(cut / "session", sessionFolder);
    SwitchTo("Cardenza/Synth");
    groove = &Groove();
    Frames();
  };
  const auto keepWorking = [&](std::uint16_t tempo) {
    groove->Sequencer().SetTempo(tempo);
    Tap(1, 1);  // any key: it may have changed something
  };
  // A moment after the last key the work is kept, as the unsaved state it is.
  keepWorking(141);
  Frames(140);
  powerCut();
  // This one is too recent: the cut takes it.
  keepWorking(97);
  Frames(3);
  restart();
  assert(groove->Sequencer().Tempo() == 141 && groove->TrackLevel(1) == 42);
  assert(groove->Ui().State().projectName == "SONG 01");
  // Again right after the start, before anything else happened: nothing was used up.
  powerCut();
  restart();
  assert(groove->Sequencer().Tempo() == 141 && groove->Ui().State().projectName == "SONG 01");
  // FlushSession (the web page closing) writes a change at once, without waiting for quiet.
  keepWorking(133);
  Frames(3);
  groove->FlushSession();
  powerCut();
  restart();
  assert(groove->Sequencer().Tempo() == 133);
  keepWorking(141);
  groove->FlushSession();

  // It is small: a default project with a few changes is a few hundred bytes.
  const std::uintmax_t autosaveBytes = AutosaveBytes(internal);
  std::cout << "Autosave: " << autosaveBytes << " bytes" << std::endl;
  assert(autosaveBytes < 1500);

  // A key that changes nothing writes nothing, and a change only its own record.
  {
    Tap(1, 1);
    Frames(140);
    const auto before = WriteTimes(internal);
    Tap(1, 1);
    Frames(140);
    assert(WriteTimes(internal) == before);
    assert(groove->Sequencer().InsertNote(1, 0, 5, 0, 72, 100, 24));
    Tap(1, 1);
    Frames(140);
    const auto after = WriteTimes(internal);
    std::size_t changed = 0;
    for (const auto& record : after)
    {
      const auto earlier = before.find(record.first);
      if (earlier == before.end() || earlier->second != record.second) ++changed;
    }
    assert(changed == 1 && after.count("as_p1_0.rec") == 1);
    assert(groove->Sequencer().ClearPatternEvents(1, 0));
    Tap(1, 1);
    Frames(140);
  }

  // While something plays nothing goes to flash, because a write can stall the sound for a moment.
  {
    const std::string before = ReadText(internal / "as_g.rec");
    groove->Sequencer().Play();
    keepWorking(150);
    Frames(150);
    assert(ReadText(internal / "as_g.rec") == before);
    // Keys in a row count as playing too, also with the transport stopped.
    groove->Sequencer().Stop();
    for (int beat = 0; beat < 20; ++beat)
    {
      Tap(1, 1);
      Frames(20);
    }
    assert(ReadText(internal / "as_g.rec") == before);
    // Soon after it stops the changes are in.
    Frames(140);
    assert(ReadText(internal / "as_g.rec") != before);
    // Played on and on, changes do not wait forever.
    const std::string longPlay = ReadText(internal / "as_g.rec");
    groove->Sequencer().Play();
    keepWorking(151);
    Frames(20);
    assert(ReadText(internal / "as_g.rec") == longPlay);
    gNow += 61000;
    Frames(2);
    assert(ReadText(internal / "as_g.rec") != longPlay);
    groove->Sequencer().Stop();
  }

  // No card, no difference: the work stays, also over a restart. The sample it names waits for the card
  // and comes back with it.
  {
    const std::string tiny = (cardenza / "Shared" / "Samples" / "tiny.wav").string();
    const std::array<std::int16_t, 64> frames{{1000, -1000}};
    assert(synth::sampler::WriteMono16Wav(tiny, frames.data(), frames.size(), 44100));
    assert(groove->Sampler().SelectBrowserSample(3, "tiny.wav"));
    Tap(1, 1);
    Frames(140);
    CardenzaSim::Input::SetCardInserted(false);
    fs::rename(tiny, tiny + ".out");  // the simulated card's files stay readable
    keepWorking(144);
    Frames(140);
    powerCut();
    restart();
    assert(groove->Sequencer().Tempo() == 144 && groove->Sampler().BuildUiState(3).sampleFileName.empty());
    // A save without the card does not forget the sample.
    keepWorking(145);
    Frames(140);
    powerCut();
    restart();
    assert(groove->Sequencer().Tempo() == 145 && groove->Sampler().BuildUiState(3).sampleFileName.empty());
    fs::rename(tiny + ".out", tiny);
    CardenzaSim::Input::SetCardInserted(true);
    Frames(650);  // the system retries the card every ten seconds
    assert(groove->Sampler().BuildUiState(3).sampleFileName == "tiny.wav");
    Tap(1, 1);
    Frames(140);
    fs::remove(tiny);
    powerCut();
    restart();
  }

  // Too much for the room in flash: what plays stays, and the screen says so. SAVE keeps it all.
  {
    auto& sequencer = groove->Sequencer();
    for (std::size_t track = 0; track < 4; ++track)
      for (std::size_t pattern = 0; pattern < 16; ++pattern)
      {
        assert(sequencer.QueuePattern(track, pattern));
        for (std::size_t step = 0; step < 64; ++step) assert(sequencer.InsertNote(track, pattern, step, 0, 60 + step % 12, 100, 24));
      }
    for (std::size_t track = 0; track < 4; ++track) assert(sequencer.QueuePattern(track, 0));
    Tap(1, 1);
    Frames(140);
    assert(Contains(groove->Ui().State().toast.text, "TOO LONG"));
    assert(AutosaveBytes(internal) < 13 * 1024);
    powerCut();
    restart();
    for (std::size_t track = 0; track < 4; ++track) assert(groove->Sequencer().PatternStepMask(track, 0) != 0);
    // Back to a short one: the room is free again.
    groove::ProjectUiSettings fresh;
    std::string message;
    assert(groove->NewProject(fresh, message));
    groove->Sequencer().SetTempo(133);
    Tap(1, 1);
    Frames(140);
    assert(AutosaveBytes(internal) < 1500);
  }

  // A record that does not make sense: a clean start, no crash.
  powerCut();
  WriteText(cut / "internal" / "as_t1.rec", "x");
  restart();
  assert(groove->Sequencer().Tempo() == 120);
  // A state that keeps crashing the start is not tried forever.
  groove->Sequencer().SetTempo(146);
  Tap(1, 1);
  Frames(140);
  powerCut();
  WriteText(cut / "internal" / "as_boot.rec", "3");
  restart();
  assert(groove->Sequencer().Tempo() == 120);
  groove->Sequencer().SetTempo(133);

  // A sample recording survives too: at most the last second is gone, and its screen comes back.
  CardenzaSim::Input::SetMicrophonePresent(true);
  const auto startRecording = [&]() {
    // T4, the sampler: FN + GRID opens its record screen, ENTER starts.
    Key(1, 0, true);
    Tap(0, 12);
    Key(1, 0, false);
    Key(2, 0, true);
    Tap(2, 10);
    Key(2, 0, false);
    assert(groove->Ui().TopModal() != nullptr && groove->Ui().TopModal()->kind == groove::ModalKind::SampleRecord);
    Tap(2, 13);
    assert(groove->Sampler().RecordingTrack() == 3);
  };
  startRecording();
  CardenzaSim::Input::FeedMicrophone(60000);
  Frames(2);
  assert(groove->Sampler().RecordingTrack() == 3);
  Frames();
  powerCut();
  assert(fs::is_regular_file(cut / "session" / "take-4.wav"));
  restart();
  {
    const auto take = groove->Sampler().BuildUiState(3);
    assert(take.hasDraft && !take.recording && take.frameCount >= 44100 && take.frameCount <= 60000);
    assert(groove->Ui().TopModal() != nullptr && groove->Ui().TopModal()->kind == groove::ModalKind::SampleRecord && groove->Ui().State().track == 3);
    // Discarded for good.
    Tap(0, 0);
    Tap(2, 13);
    assert(!groove->Sampler().BuildUiState(3).hasDraft && groove->Ui().TopModal() == nullptr);
    assert(!fs::exists(sessionFolder / "take-4.wav"));
  }
  // An app switch finishes the recording into a take: nothing is cut off.
  startRecording();
  CardenzaSim::Input::FeedMicrophone(50000);
  Frames(2);
  SwitchTo(kBlankId);
  SwitchTo("Cardenza/Synth");
  groove = &Groove();
  Frames();
  assert(groove->Sampler().BuildUiState(3).hasDraft && groove->Sampler().BuildUiState(3).frameCount == 50000);
  Tap(0, 0);
  Tap(2, 13);
  CardenzaSim::Input::SetMicrophonePresent(false);
  Key(1, 0, true);  // back to T2
  Tap(0, 10);
  Key(1, 0, false);

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

  // While nothing happens nothing is written, however long the device stays on.
  {
    Frames(300);
    const auto snapshot = [&]() {
      std::map<std::string, fs::file_time_type> times;
      for (const fs::path& folder : {root / "internal", cardenza})
        for (const auto& entry : fs::recursive_directory_iterator(folder))
          if (entry.is_regular_file()) times[entry.path().string()] = entry.last_write_time();
      return times;
    };
    const auto before = snapshot();
    Frames(4000);
    assert(snapshot() == before);
  }

  // An orderly shutdown suspends the app: the session is waiting for the next start.
  os::system::Shutdown();
  assert(os::system::ActiveApplication() == nullptr);
  assert(fs::is_regular_file(internal / "as_g.rec"));

  fs::current_path(fs::temp_directory_path());
  fs::remove_all(root);
  std::cout << "App lifecycle passed\n";
  return 0;
}
