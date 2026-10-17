#include "Applications/Synth/Runtime/ProjectStorage.h"

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

namespace {
std::filesystem::path projectDirectory;
struct Output {
  std::mutex mutex;
  std::condition_variable changed;
  std::vector<midi::MidiPacket> packets;
  std::vector<std::size_t> tracks;
  bool blockNoteOn = false;
  bool noteEntered = false;
};
void Sink(void* context, std::size_t track, const midi::MidiPacket& packet) {
  auto& output = *static_cast<Output*>(context);
  std::unique_lock<std::mutex> lock(output.mutex);
  if (packet.status == midi::MidiStatus::NoteOn) {
    output.noteEntered = true;
    output.changed.notify_all();
    output.changed.wait(lock, [&] { return !output.blockNoteOn; });
  }
  output.packets.push_back(packet);
  output.tracks.push_back(track);
}

void TestSequencerEditing() {
  using namespace synth;
  Output output;
  auto sequencer = std::make_unique<SynthSequencer>();
  sequencer->SetMidiSink(Sink, &output);
  // Duplicate keyboard pitches form one chord tone, even in non-pitch input order.
  const std::uint8_t chord[] = {67, 60, 64, 60, 255};
  sequencer->PressStep(0, chord, std::size(chord));
  auto ui = sequencer->BuildUiState(0);
  assert(ui.noteCount == 3 && ui.noteNames == "C4 E4 G4");
  // Grid labels use the lowest active pitch, independent of entry order.
  assert(ui.steps[0].lowestNote == 60 && ui.steps[0].noteCount == 3 && ui.steps[0].hasEvents);
  assert(ui.steps[1].lowestNote == -1 && ui.steps[1].noteCount == 0 && !ui.steps[1].hasEvents);
  assert(sequencer->BuildUiState(1).steps[0].lowestNote == -1);
  assert(ui.heldStepCount == 1 && ui.primaryHeldStep == 0 && ui.scopedNote == -1);
  assert(ui.populatedStepCount == 1 && ui.patternHasEvents[0] && !ui.patternHasEvents[1]);
  assert(ui.tracks[0].populatedStepCount == 1 && ui.tracks[1].populatedStepCount == 0);

  assert(sequencer->PressEditLane(1));
  assert(sequencer->HandlePerformanceKeyPress(0, 8, 0)); // Sixteen-step length.
  sequencer->ReleaseEditLane(1);
  assert(sequencer->PressEditLane(3));
  assert(sequencer->HandlePerformanceKeyPress(0, 6, 0)); // Last microstep.
  sequencer->ClearHeldSteps();
  assert(sequencer->CopyStep(0, kSequencerStepCount - 1));
  const auto copiedGrid = sequencer->BuildUiState(0);
  assert(copiedGrid.steps.back().lowestNote == 60 && copiedGrid.steps.back().noteCount == 3);
  assert(!copiedGrid.steps[0].held && copiedGrid.steps[0].lowestNote == 60);
  auto state = sequencer->CaptureState();
  assert(state && SynthSequencer::ValidateState(*state));
  for (const auto& note : state->tracks[0].patterns[0].steps.back().notes) {
    if (note.active) assert(note.microstep == 5 && note.lengthPulses == 4);
  }
  assert(state->tracks[0].patterns[0].steps[0].notes[0].lengthPulses == 364);

  sequencer->Reset();
  const std::uint8_t otherChord[] = {76, 72};
  const std::uint8_t untouched = 84;
  sequencer->PressStep(10, &untouched, 1);
  assert(sequencer->PressEditLane(3));
  assert(sequencer->HandlePerformanceKeyPress(0, 6, 0));
  sequencer->ClearHeldSteps();
  sequencer->PressStep(2, chord, std::size(chord));
  sequencer->PressStep(6, otherChord, std::size(otherChord));
  ui = sequencer->BuildUiState(0);
  assert(ui.heldStepCount == 2 && ui.primaryHeldStep == 2 && ui.populatedStepCount == 3);
  assert(sequencer->PressEditLane(0)); // Scope is sorted C4, not first inserted G4.
  assert(sequencer->BuildUiState(0).scopedNote == 60);
  assert(output.packets.back().status == midi::MidiStatus::NoteOn && output.packets.back().data[1] == 60);
  sequencer->ReleaseEditLane(0);
  assert(output.packets.back().status == midi::MidiStatus::NoteOff && output.packets.back().data[1] == 60);
  assert(sequencer->PressEditLane(2));
  assert(sequencer->HandlePerformanceKeyPress(0, 8, 0));
  state = sequencer->CaptureState();
  for (std::size_t step : {std::size_t{2}, std::size_t{6}}) {
    for (const auto& note : state->tracks[0].patterns[0].steps[step].notes) {
      if (note.active) assert(note.velocity == (note.note == 60 || note.note == 72 ? 127 : 110));
    }
  }
  assert(sequencer->PressEditLane(1) && sequencer->HandlePerformanceKeyPress(0, 2, 0));
  assert(sequencer->PressEditLane(3) && sequencer->HandlePerformanceKeyPress(0, 2, 0));
  // Returning to ALL must show mixed values rather than claim one chord-wide value.
  sequencer->ClearHeldSteps();
  sequencer->PressStep(2, nullptr, 0);
  sequencer->PressStep(6, nullptr, 0);
  assert(sequencer->PressEditLane(1) && sequencer->BuildUiState(0).editValueMixed);
  assert(sequencer->PressEditLane(3) && sequencer->BuildUiState(0).editValueMixed);
  assert(sequencer->PressEditLane(2));
  ui = sequencer->BuildUiState(0);
  assert(ui.editValueMixed && ui.valueLabel == "VEL MIXED");
  assert(sequencer->HandlePerformanceKeyPress(0, 4, 0));
  assert(!sequencer->BuildUiState(0).editValueMixed);
  assert(sequencer->PressEditLane(1));
  assert(sequencer->HandlePerformanceKeyPress(0, 2, 0));
  assert(sequencer->PressEditLane(3));
  assert(sequencer->HandlePerformanceKeyPress(0, 4, 0));
  sequencer->QuantizeHeldSteps();
  state = sequencer->CaptureState();
  for (std::size_t step : {std::size_t{2}, std::size_t{6}}) {
    for (const auto& note : state->tracks[0].patterns[0].steps[step].notes) {
      if (note.active) assert(note.microstep == 0 && note.lengthPulses == 48 && note.velocity == 72);
    }
  }
  assert(state->tracks[0].patterns[0].steps[10].notes[0].microstep == 5);
  sequencer->ClearHeldStepEvents();
  assert(!sequencer->StepHasEvents(2) && !sequencer->StepHasEvents(6) && sequencer->StepHasEvents(10));
  sequencer->QuantizeActivePattern();
  state = sequencer->CaptureState();
  assert(state->tracks[0].patterns[0].steps[10].notes[0].microstep == 0);
  assert(SynthSequencer::ValidateState(*state));

  // A scope audition and edit selection belong to the track where they were started.
  sequencer->ClearHeldSteps();
  sequencer->PressStep(10, nullptr, 0);
  assert(sequencer->PressEditLane(0));
  sequencer->SetActiveTrack(1);
  assert(!sequencer->HasHeldSteps() && sequencer->BuildUiState(1).editLane == SequencerEditLane::Note);
  assert(output.packets.back().status == midi::MidiStatus::NoteOff && output.tracks.back() == 0);
  assert(!sequencer->HandlePerformanceKeyPress(0, 1, 61));
  assert(!sequencer->StepHasEvents(10));
  sequencer->SetActiveTrack(0);
  assert(sequencer->StepHasEvents(10));

  // Deleting before the selected pattern preserves the selected musical content.
  sequencer->Reset();
  for (std::size_t pattern = 0; pattern < 3; ++pattern) {
    assert(sequencer->SelectOrCreatePattern(pattern));
    const auto pitch = static_cast<std::uint8_t>(60 + pattern);
    sequencer->PressStep(0, &pitch, 1);
    sequencer->ReleaseStep(0);
  }
  assert(sequencer->SelectOrCreatePattern(1) && sequencer->DeletePattern(0));
  state = sequencer->CaptureState();
  assert(state->tracks[0].activePattern == 0 && state->tracks[0].patternCount == 2);
  assert(state->tracks[0].patterns[0].steps[0].notes[0].note == 61);
  assert(sequencer->CopyPattern(0, 2));
  assert(sequencer->BuildUiState(0).tracks[0].activePattern == 2);
  assert(sequencer->DeletePattern(1));
  assert(sequencer->BuildUiState(0).activePattern == 1);
  sequencer->ToggleTrackMuted();
  sequencer->ToggleTrackEnabled();
  ui = sequencer->BuildUiState(0);
  assert(ui.tracks[0].muted && !ui.tracks[0].enabled && ui.tracks[0].patternCount == 2);
  sequencer->PressStep(0, nullptr, 0);
  assert(sequencer->PressEditLane(0));
  sequencer->Stop();
  assert(output.packets.back().status == midi::MidiStatus::ControlChange);
  const auto stoppedCount = output.packets.size();
  sequencer->ReleaseEditLane(0);
  assert(output.packets.size() == stoppedCount); // Stop already released the audition.
  // Removing a filled trailing pattern and adding a new slot must not resurrect notes.
  assert(sequencer->PatternHasEvents(1));
  sequencer->AdjustPatternCount(-1);
  sequencer->AdjustPatternCount(1);
  assert(sequencer->SelectOrCreatePattern(1) && !sequencer->PatternHasEvents(1));
  assert(sequencer->PatternHasEvents(0));
}
}
namespace Device::Storage {
std::string ProjectDirectoryPath() { return projectDirectory.string(); }
bool EnsureDirectoryLayout() { return std::filesystem::is_directory(projectDirectory); }
}

int main() {
  using namespace synth;
  using namespace std::chrono_literals;
  projectDirectory = std::filesystem::temp_directory_path() /
      ("cardenza-project-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directory(projectDirectory);
  TestSequencerEditing();
  Output output; // Outlives the sequencer's final stop callback.
  auto sequencer = std::make_unique<SynthSequencer>();
  sequencer->SetMidiSink(Sink, &output);
  const std::uint8_t notes[] = {60, 64, 67};
  sequencer->PressStep(0, notes, 3);
  sequencer->ReleaseStep(0);
  sequencer->AdjustTempo(200);
  assert(sequencer->BuildUiState(0).bpm == 300);
  sequencer->AdjustTempo(-1000);
  assert(sequencer->BuildUiState(0).bpm == 30);
  sequencer->AdjustTempo(90);
  assert(sequencer->CopyPattern(0, 1));
  auto snapshot = sequencer->CaptureState();
  assert(snapshot && SynthSequencer::ValidateState(*snapshot));
  assert(snapshot->tracks[0].patterns[1].steps[0].notes[0].active);

  ProjectData project;
  project.sequence = std::move(snapshot);
  project.activeTrack = 2;
  project.octave = 3;
  for (std::size_t i = 0; i < kSynthTrackCount; ++i) {
    project.kinds[i] = SynthEngineKind::Dx7;
    project.engines[i] = {"factory.1", "user.1", "Test Voice", {{"global.feedback", 5}}};
  }
  project.kinds[2] = SynthEngineKind::Sampler;
  project.samples[2] = "Exact.WAV";
  assert(SaveProjectFile("round trip", project, false) == ProjectResult::Ok);
  assert(SaveProjectFile("round trip", project, false) == ProjectResult::Exists);
  ProjectData loaded;
  assert(LoadProjectFile("round trip", loaded) == ProjectResult::Ok);
  assert(loaded.activeTrack == 2 && loaded.octave == 3 && loaded.samples[2] == "Exact.WAV");
  assert(loaded.engines[0].values.at("global.feedback") == 5);
  assert(loaded.sequence->tracks[0].patternCount == project.sequence->tracks[0].patternCount);
  assert(loaded.sequence->tracks[0].patterns[1].steps[0].notes[2].note == 67);
  const auto path = projectDirectory / "round trip.ccproj";
  std::ifstream input(path);
  const std::string valid{std::istreambuf_iterator<char>(input), {}};
  input.close();
  auto reject = [&](const std::string& text) {
    std::ofstream file(path, std::ios::trunc); file << text; file.close();
    auto* old = loaded.sequence.get();
    assert(LoadProjectFile("round trip", loaded) == ProjectResult::Invalid);
    assert(loaded.sequence.get() == old && loaded.activeTrack == 2);
  };
  reject(valid.substr(0, valid.size() / 2));
  reject(valid + "TRAILING");
  auto bad = valid; bad.replace(bad.find("SEQ 120"), 7, "SEQ 999"); reject(bad);
  bad = valid; bad.replace(bad.find("factory.1"), 9, std::string(4000, 'x')); reject(bad);
  assert(SaveProjectFile("../escape", project, true) == ProjectResult::Invalid);
  assert(!IsProjectNameValid("CON") && !IsProjectNameValid("com1"));
  assert(SaveProjectFile("round trip", project, true) == ProjectResult::Ok);
  std::filesystem::rename(path, path.string() + ".bak");
  assert(ListProjects() == std::vector<std::string>{"round trip"});
  assert(LoadProjectFile("round trip", loaded) == ProjectResult::Ok);
  assert(SaveProjectFile("round trip", project, false) == ProjectResult::Exists);

  // Force a MIDI batch to overlap Stop. Stop must wait for that batch, then release it.
  assert(sequencer->RestoreState(*project.sequence));
  {
    std::lock_guard<std::mutex> lock(output.mutex);
    output.packets.clear(); output.noteEntered = false; output.blockNoteOn = true;
  }
  sequencer->Play();
  {
    std::unique_lock<std::mutex> lock(output.mutex);
    assert(output.changed.wait_for(lock, 2s, [&] { return output.noteEntered; }));
  }
  std::atomic<bool> stopped{false};
  std::thread stop([&] { sequencer->Stop(); stopped = true; });
  std::this_thread::sleep_for(20ms);
  assert(!stopped.load());
  {
    std::lock_guard<std::mutex> lock(output.mutex);
    output.blockNoteOn = false;
  }
  output.changed.notify_all();
  stop.join();
  assert(stopped.load() && !sequencer->Playing());
  std::size_t count;
  {
    std::lock_guard<std::mutex> lock(output.mutex);
    assert(!output.packets.empty());
    assert(output.packets.back().status == midi::MidiStatus::ControlChange);
    count = output.packets.size();
  }
  std::this_thread::sleep_for(40ms);
  {
    std::lock_guard<std::mutex> lock(output.mutex);
    assert(output.packets.size() == count);
  }
  sequencer->Resume();
  assert(sequencer->Playing());
  sequencer->ToggleTrackMuted();
  assert(sequencer->BuildUiState(0).trackMuted);
  sequencer->ToggleTrackEnabled();
  assert(!sequencer->BuildUiState(0).trackEnabled);
  sequencer->Stop();
  project.sequence->tracks[0].patterns[0].steps[0].notes[0].microstep = 255;
  assert(!sequencer->RestoreState(*project.sequence));
  sequencer->Reset();
  assert(!sequencer->StepHasEvents(0));
  sequencer.reset();
  // Only files created by this test are removed.
  std::filesystem::remove(path.string() + ".bak");
  std::filesystem::remove(projectDirectory);
  std::cout << "Project round-trip/recovery/malformed input and sequencer transport ordering passed\n";
}
