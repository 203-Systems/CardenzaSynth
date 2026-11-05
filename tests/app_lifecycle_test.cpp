// App lifecycle on the real system: boot on a card from before the storage protocol, change device
// settings, play settings and the session in the groovebox, switch to another app and back, and
// check that nothing was lost and where everything was stored: on the card, never in internal flash
// (an older firmware's autosave there is moved to the card).
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
#include "Applications/Synth/Runtime/ProjectStorage.h"
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
  if (!fs::exists(from)) return;
  fs::create_directories(to.parent_path());
  fs::copy(from, to, fs::copy_options::recursive);
}

// Records of an autosave in internal flash (older firmware wrote them, this one only erases them).
std::size_t FlashRecords(const fs::path& folder)
{
  std::size_t records = 0;
  if (!fs::exists(folder)) return 0;
  for (const auto& entry : fs::directory_iterator(folder))
    if (entry.path().filename().string().rfind("as_", 0) == 0) ++records;
  return records;
}

// What older firmware wrote to internal flash for the autosave (its SessionStore, until 2026-11-05):
// a record per part of the project, in the simulator a file per record.
namespace older {

class Writer {
public:
  void Reserve(std::size_t bytes) { bytes_.reserve(bytes); }
  void Byte(int value) { bytes_.push_back(static_cast<char>(value)); }
  void Number(std::uint32_t value)
  {
    while (value >= 0x80)
    {
      Byte(static_cast<int>(value & 0x7F) | 0x80);
      value >>= 7;
    }
    Byte(static_cast<int>(value));
  }
  void Signed(int value) { Number((static_cast<std::uint32_t>(value) << 1) ^ static_cast<std::uint32_t>(value >> 31)); }
  void Text(const std::string& text)
  {
    Number(static_cast<std::uint32_t>(text.size()));
    bytes_ += text;
  }
  std::string Take() { return std::move(bytes_); }

private:
  std::string bytes_;
};

std::string EncodeGeneral(const synth::SessionInfo& info, const synth::ProjectData& project)
{
  const auto& sequence = *project.sequence;
  Writer out;
  out.Byte(1);
  out.Byte(info.dirty ? 1 : 0);
  out.Text(info.name);
  out.Byte(static_cast<int>(project.activeTrack));
  out.Signed(project.octave);
  out.Byte(project.root);
  out.Byte(static_cast<int>(project.scale));
  out.Byte(project.layout);
  out.Byte(project.lock);
  out.Byte(project.velocity);
  out.Number(sequence.bpm);
  out.Byte(sequence.swing);
  int muted = 0;
  int soloed = 0;
  for (std::size_t track = 0; track < synth::kSynthTrackCount; ++track)
  {
    out.Byte(project.mixer.level[track]);
    muted |= project.mixer.muted[track] ? 1 << track : 0;
    soloed |= project.mixer.soloed[track] ? 1 << track : 0;
  }
  out.Byte(muted);
  out.Byte(soloed);
  out.Number(static_cast<std::uint32_t>(project.masterFx.size()));
  for (const std::uint8_t value : project.masterFx) out.Byte(value);
  for (const auto& play : project.play)
  {
    out.Byte(play.chord);
    out.Byte(play.voicing);
    out.Byte(static_cast<int>(play.arp.mode));
    out.Byte(play.arp.rate);
    out.Byte(play.arp.octaves);
    out.Byte(play.arp.gate);
    out.Byte(play.arp.latch ? 1 : 0);
  }
  // Last, so a record from before it reads as generation 0.
  out.Number(info.generation);
  return out.Take();
}

std::string EncodeSong(const synth::SynthSequencer::ProjectState& sequence)
{
  // A song is rare: no record at all until there is one.
  if (sequence.songLength == 0 && sequence.songLoop) return {};
  Writer out;
  out.Number(static_cast<std::uint32_t>(sequence.songLength));
  out.Byte(sequence.songLoop ? 1 : 0);
  for (std::size_t row = 0; row < sequence.songLength; ++row)
  {
    for (const std::int8_t pattern : sequence.song[row].patterns) out.Byte(pattern + 1);
    out.Byte(sequence.song[row].repeats);
  }
  return out.Take();
}

std::string EncodeEngine(synth::SynthEngineKind kind, const synth::SynthEngineState& state, const std::string& sample)
{
  Writer out;
  out.Byte(static_cast<int>(kind));
  out.Text(state.basePresetId);
  out.Text(state.presetId == state.basePresetId ? std::string() : state.presetId);
  out.Text(state.presetName);
  out.Text(sample);
  out.Number(static_cast<std::uint32_t>(state.values.size()));
  for (const auto& entry : state.values)
  {
    out.Text(entry.first);
    out.Signed(entry.second);
  }
  return out.Take();
}

std::string EncodeTrack(const synth::SynthSequencer::TrackData& track)
{
  Writer out;
  out.Number(static_cast<std::uint32_t>(track.patternCount));
  out.Number(static_cast<std::uint32_t>(track.activePattern));
  out.Byte(track.defaultVelocity);
  out.Number(track.defaultLengthPulses);
  out.Byte((track.enabled ? 1 : 0) | (track.muted ? 2 : 0));
  for (std::size_t pattern = 0; pattern < track.patternCount; ++pattern) out.Number(track.patternSteps[pattern]);
  return out.Take();
}

std::string EncodePattern(const synth::SynthSequencer::PatternData& pattern, std::size_t notes)
{
  Writer out;
  // One block of about the right size: growing doubles, and needs both blocks for a moment.
  out.Reserve(notes * 7);
  std::size_t previous = 0;
  for (std::size_t step = pattern.steps.NextInUse(0); step < pattern.steps.size(); step = pattern.steps.NextInUse(step + 1))
  {
    const auto& notes = pattern.steps[step].notes;
    const auto count = static_cast<std::uint32_t>(std::count_if(notes.begin(), notes.end(), [](const auto& note) { return note.active; }));
    if (count == 0) continue;
    out.Number(static_cast<std::uint32_t>(step - previous));
    out.Byte(static_cast<int>(count));
    previous = step;
    for (const auto& note : notes)
    {
      if (!note.active) continue;
      out.Byte(note.note);
      out.Byte(note.velocity);
      out.Number(note.lengthPulses);
      out.Byte(note.microstep);
    }
  }
  return out.Take();
}

void WriteFlashAutosave(const fs::path& folder, const synth::SessionInfo& info, const synth::ProjectData& project)
{
  const auto& sequence = *project.sequence;
  const auto put = [&](const std::string& key, const std::string& bytes) {
    if (!bytes.empty()) WriteText(folder / (key + ".rec"), bytes);
  };
  put("as_g", EncodeGeneral(info, project));
  put("as_s", EncodeSong(sequence));
  for (std::size_t track = 0; track < synth::kSynthTrackCount; ++track)
  {
    put("as_e" + std::to_string(track), EncodeEngine(project.kinds[track], project.engines[track], project.samples[track]));
    put("as_t" + std::to_string(track), EncodeTrack(sequence.tracks[track]));
    for (std::size_t pattern = 0; pattern < sequence.tracks[track].patternCount; ++pattern)
      if (sequence.tracks[track].patterns.Allocated(pattern))
        put("as_p" + std::to_string(track) + "_" + std::to_string(pattern), EncodePattern(sequence.tracks[track].patterns[pattern], 64));
  }
}

}  // namespace older

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
  // An older firmware's autosave records go at the first quiet moment; another firmware's stay.
  Frames(140);
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
  // The working state is on the card; internal flash is never written.
  assert(Contains(ReadText(sessionFolder / "autosave.txt"), "name=SONG 01\n") && FlashRecords(internal) == 0);
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
  assert(ReadText(sessionFolder / "restore.txt") == "0\n");  // the start that restored it finished

  // The power can go at any moment. Nothing tells the app: what counts is what is on the card then.
  const fs::path cut = root / "power-cut";
  const auto powerCut = [&]() {
    CopyFolder(internal, cut / "internal");
    CopyFolder(sessionFolder, cut / "session");
  };
  const auto restart = [&]() {
    // The switch stands in for the restart; the card (and the flash) are put back to what the cut left.
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

  assert(fs::is_regular_file(sessionFolder / "autosave.ccproj") && FlashRecords(internal) == 0);

  // While something plays the changes go to the card once they have waited a little (on the device's
  // background task); soon after it stops, at once.
  {
    const std::string before = ReadText(sessionFolder / "autosave.txt");
    groove->Sequencer().Play();
    keepWorking(150);
    Frames(150);
    assert(ReadText(sessionFolder / "autosave.txt") == before);
    // Keys in a row count as playing too, also with the transport stopped.
    groove->Sequencer().Stop();
    for (int beat = 0; beat < 20; ++beat)
    {
      Tap(1, 1);
      Frames(20);
    }
    assert(ReadText(sessionFolder / "autosave.txt") == before);
    // Soon after it stops the changes are in.
    Frames(140);
    const std::string stopped = ReadText(sessionFolder / "autosave.txt");
    assert(stopped != before);
    // Played on and on: on the card after ten seconds.
    groove->Sequencer().Play();
    keepWorking(151);
    Frames(20);
    assert(ReadText(sessionFolder / "autosave.txt") == stopped);
    gNow += 11000;
    Frames(2);
    assert(ReadText(sessionFolder / "autosave.txt") != stopped && FlashRecords(internal) == 0);
    powerCut();
    restart();
    assert(groove->Sequencer().Tempo() == 151);
    keepWorking(152);
    Frames(140);
    powerCut();
    restart();
    assert(groove->Sequencer().Tempo() == 152);
  }

  // Without a card nothing outlives the power (internal flash is never written), and the screen says so.
  // A start without it begins empty; the card coming later brings its working state back, as long as
  // nothing was changed meanwhile.
  {
    CardenzaSim::Input::SetCardInserted(false);
    Frames(2);
    keepWorking(144);
    Frames(140);
    assert(Contains(groove->Ui().State().toast.text, "NO SD CARD") && FlashRecords(internal) == 0);
    powerCut();
    restart();
    assert(groove->Sequencer().Tempo() == 120);
    CardenzaSim::Input::SetCardInserted(true);
    Frames(700);  // the device looks for the card now and then
    assert(groove->Sequencer().Tempo() == 152 && groove->Ui().State().projectName == "SONG 01");
    // With the card back the changes go to it again.
    keepWorking(153);
    Frames(140);
    powerCut();
    restart();
    assert(groove->Sequencer().Tempo() == 153);
  }

  // A long project goes whole: every pattern of every track comes back.
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
    powerCut();
    restart();
    for (std::size_t track = 0; track < 4; ++track)
      for (std::size_t pattern = 0; pattern < 16; ++pattern) assert(groove->Sequencer().PatternStepMask(track, pattern) != 0);
    groove::ProjectUiSettings fresh;
    std::string message;
    assert(groove->NewProject(fresh, message));
    groove->Sequencer().SetTempo(133);
    Tap(1, 1);
    Frames(140);
  }

  // An autosave that does not make sense: a clean start, no crash.
  powerCut();
  WriteText(cut / "session" / "autosave.ccproj", "CARDENZA_PROJECT 7\nnonsense\n");
  restart();
  assert(groove->Sequencer().Tempo() == 120);
  // A state that keeps crashing the start is not tried forever.
  groove->Sequencer().SetTempo(146);
  Tap(1, 1);
  Frames(140);
  powerCut();
  WriteText(cut / "session" / "restore.txt", "3\n");
  restart();
  assert(groove->Sequencer().Tempo() == 120);

  // An older firmware's autosave in internal flash, newer than the card's: it comes back, goes to the
  // card at the next quiet moment and is erased from the flash; the next start takes the card's.
  {
    groove->Sequencer().SetTempo(147);
    Tap(1, 1);
    Frames(140);
    powerCut();
    synth::ProjectData project;
    assert(synth::LoadProjectFileAt((cut / "session" / "autosave.ccproj").string(), project) == synth::ProjectResult::Ok);
    project.sequence->bpm = 149;
    synth::SessionInfo info;
    info.name = "SONG 01";
    info.dirty = true;
    info.generation = 1000000;
    older::WriteFlashAutosave(cut / "internal", info, project);
    WriteText(cut / "internal" / "as_boot.rec", "1");
    restart();
    assert(groove->Sequencer().Tempo() == 149 && groove->Ui().State().projectName == "SONG 01" && groove->Ui().State().dirty);
    // Nothing plays at a start: on the card in the first frame, out of the flash in the next.
    assert(FlashRecords(internal) == 0 && ReadText(root / "internal" / "launcher.rec") == "theirs");
    assert(Contains(ReadText(sessionFolder / "autosave.txt"), "generation=1000001\n"));
    powerCut();
    restart();
    assert(groove->Sequencer().Tempo() == 149);
    // An older one than the card's stays unread, and goes too.
    project.sequence->bpm = 99;
    info.generation = 5;
    older::WriteFlashAutosave(internal, info, project);
    powerCut();
    restart();
    assert(groove->Sequencer().Tempo() == 149);
    Frames(140);
    assert(FlashRecords(internal) == 0);
    groove->Sequencer().SetTempo(133);
  }

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

  // Card out: device settings wait in RAM (not in internal flash) and go to the card when it returns.
  CardenzaSim::Input::SetCardInserted(false);
  groove->SetMasterVolume(30);
  Frames(70);
  assert(!fs::exists(root / "internal" / "sys.rec") && Contains(ReadText(systemSettings), "sys.volume=60\n"));
  CardenzaSim::Input::SetCardInserted(true);
  Frames(700);  // the system tries the card again every ten seconds
  assert(!fs::exists(root / "internal" / "sys.rec") && Contains(ReadText(systemSettings), "sys.volume=30\n"));

  // While nothing happens nothing is written, however long the device stays on.
  {
    Frames(300);
    const auto snapshot = [&]() {
      std::map<std::string, fs::file_time_type> times;
      for (const fs::path& folder : {root / "internal", cardenza})
        for (const auto& entry : fs::recursive_directory_iterator(folder, fs::directory_options::none))
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
  assert(fs::is_regular_file(sessionFolder / "autosave.ccproj") && FlashRecords(internal) == 0);

  fs::current_path(fs::temp_directory_path());
  fs::remove_all(root);
  std::cout << "App lifecycle passed\n";
  return 0;
}
