// ROLL tests. The sequencer is built with CARDENZA_SEQUENCER_TEST_HOOKS: the clock worker never
// starts and every test steps the clock by hand, so a rolled note's sub-hits land on exact pulses.
#include "Applications/Synth/Runtime/Sequencer/SynthSequencer.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace {

using midi::MidiStatus;
using synth::SynthSequencer;

#define CHECK(condition)                                                                      \
  do                                                                                          \
  {                                                                                           \
    if (!(condition))                                                                         \
    {                                                                                         \
      std::fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition);     \
      std::abort();                                                                           \
    }                                                                                         \
  } while (0)

struct Event {
  std::size_t track = 0;
  MidiStatus status = MidiStatus::None;
  std::uint8_t note = 0;
  std::uint8_t velocity = 0;
  std::uint64_t tick = 0;
};

struct Capture {
  std::mutex mutex;
  std::vector<Event> events;
  std::uint64_t tick = 0;
};

void Sink(void* context, std::size_t track, const midi::MidiPacket& packet)
{
  auto& capture = *static_cast<Capture*>(context);
  std::lock_guard<std::mutex> lock(capture.mutex);
  capture.events.push_back({track, packet.status, packet.data[1], packet.data[2], capture.tick});
}

bool IsOn(const Event& event) { return event.status == MidiStatus::NoteOn && event.velocity > 0; }
bool IsOff(const Event& event) { return event.status == MidiStatus::NoteOff || (event.status == MidiStatus::NoteOn && event.velocity == 0); }

// A sequencer on the manual clock. Members are destroyed in reverse order: the sequencer's final
// release and stop still reach the capture.
struct Rig {
  Capture capture;
  std::unique_ptr<SynthSequencer> sequencer;

  Rig() : sequencer(std::make_unique<SynthSequencer>())
  {
    sequencer->UseManualClockForTest();
    sequencer->SetMidiSink(Sink, &capture);
  }

  SynthSequencer& seq() { return *sequencer; }

  void Run(std::uint64_t pulses)
  {
    for (std::uint64_t index = 0; index < pulses; ++index)
    {
      sequencer->AdvancePulsesForTest(1);
      ++capture.tick;
    }
  }

  std::uint64_t Now() const { return capture.tick; }

  std::vector<Event> NoteEvents()
  {
    std::lock_guard<std::mutex> lock(capture.mutex);
    std::vector<Event> notes;
    for (const Event& event : capture.events)
      if (IsOn(event) || IsOff(event)) notes.push_back(event);
    return notes;
  }

  void Clear() { std::lock_guard<std::mutex> lock(capture.mutex); capture.events.clear(); }

  // Puts one note on a step of track 0's first pattern and hands it back to the sequencer.
  void Place(std::size_t step, std::uint8_t note, std::uint16_t lengthPulses, std::uint8_t roll)
  {
    auto state = sequencer->CaptureState();
    state->tracks[0].patterns[0].steps.Edit(step).notes[0] = {note, 100, lengthPulses, 0, true, roll};
    CHECK(sequencer->RestoreState(*state));
  }
};

// Every count divides the twenty-four pulses of a step, so a sub-hit never lands between pulses.
static_assert(synth::kSequencerPulsesPerStep % synth::kSequencerMaxRoll == 0, "Roll sub-hits must land on pulses.");

void TestSubHitsLandOnPulses()
{
  for (std::uint8_t roll = 1; roll <= synth::kSequencerMaxRoll; ++roll)
  {
    Rig rig;
    rig.Place(0, 60, 24, roll);
    rig.seq().Play();
    const std::uint64_t start = rig.Now();
    rig.Run(32);

    const std::uint16_t sub = synth::kSequencerPulsesPerStep / roll;
    const auto events = rig.NoteEvents();
    // One on and one off per hit: a rolled note is cut into the sub-steps it retriggers on.
    CHECK(events.size() == static_cast<std::size_t>(roll) * 2);
    for (std::size_t hit = 0; hit < roll; ++hit)
    {
      CHECK(IsOn(events[hit * 2]));
      CHECK(events[hit * 2].note == 60);
      CHECK(events[hit * 2].tick == start + hit * sub);
      CHECK(IsOff(events[hit * 2 + 1]));
      CHECK(events[hit * 2 + 1].tick == start + (hit + 1) * sub);
    }
    rig.seq().Stop();
  }
}

void TestRollKeepsTheStep()
{
  // A rolled step still starts on its own pulse, whichever sixteenth it sits on, and the next
  // step is untouched: roll never shifts the pattern.
  Rig rig;
  rig.Place(2, 62, 24, 4);
  rig.seq().Play();
  const std::uint64_t start = rig.Now();
  rig.Run(96);
  const auto events = rig.NoteEvents();
  CHECK(events.size() == 8);
  CHECK(events[0].tick == start + 2 * synth::kSequencerPulsesPerStep);
  CHECK(events[6].tick == start + 2 * synth::kSequencerPulsesPerStep + 18);
  rig.seq().Stop();
}

void TestPlainNoteKeepsItsLength()
{
  Rig rig;
  rig.Place(0, 60, 48, 1);  // Two steps long, no roll.
  rig.seq().Play();
  const std::uint64_t start = rig.Now();
  rig.Run(64);
  const auto events = rig.NoteEvents();
  CHECK(events.size() == 2);
  CHECK(IsOn(events[0]) && events[0].tick == start);
  CHECK(IsOff(events[1]) && events[1].tick == start + 48);
  rig.seq().Stop();
}

void TestEditing()
{
  Rig rig;
  rig.Place(3, 60, 24, 0);  // A zero left by an older file reads as off.
  CHECK(rig.seq().StepRoll(3) == 1);
  CHECK(rig.seq().DefaultRoll() == 1);

  CHECK(rig.seq().SetStepRoll(3, 3));
  CHECK(rig.seq().StepRoll(3) == 3);
  CHECK(rig.seq().DefaultRoll() == 3);  // The last count becomes the brush.

  CHECK(rig.seq().CycleStepRoll(3));
  CHECK(rig.seq().StepRoll(3) == 4);
  CHECK(rig.seq().CycleStepRoll(3));
  CHECK(rig.seq().StepRoll(3) == 1);
  CHECK(rig.seq().CycleStepRoll(3));
  CHECK(rig.seq().StepRoll(3) == 2);

  // Out of range is clamped, not rejected.
  CHECK(rig.seq().SetStepRoll(3, 99));
  CHECK(rig.seq().StepRoll(3) == synth::kSequencerMaxRoll);

  // An empty step edits the brush alone and allocates nothing.
  CHECK(!rig.seq().SetStepRoll(7, 3));
  CHECK(rig.seq().DefaultRoll() == 3);
  CHECK(rig.seq().StepRoll(7) == 1);

  // A note written after that starts rolled, the way it starts at the track's new-note velocity.
  CHECK(rig.seq().InsertNote(0, 0, 5, 0, 64, 100, 24));
  CHECK(rig.seq().StepRoll(5) == 3);

  // The UI sees it.
  const auto ui = rig.seq().BuildUiState(0);
  CHECK(ui.steps[3].roll == synth::kSequencerMaxRoll);
  CHECK(ui.steps[5].roll == 3);
  std::array<SynthSequencer::StepNoteView, 8> views{};
  CHECK(rig.seq().StepNoteEvents(0, 5, views) == 1);
  CHECK(views[0].roll == 3);
}

void TestChordScope()
{
  // A chord rolls its notes together by default, and one pitch at a time when asked.
  Rig rig;
  auto state = rig.seq().CaptureState();
  auto& notes = state->tracks[0].patterns[0].steps;
  notes.Edit(6).notes[0] = {60, 100, 24, 0, true};
  notes.Edit(6).notes[1] = {64, 100, 24, 0, true};
  notes.Edit(6).notes[2] = {67, 100, 24, 0, true};
  CHECK(rig.seq().RestoreState(*state));

  CHECK(rig.seq().SetStepRoll(6, 2));
  CHECK(rig.seq().StepRoll(6) == 2);
  CHECK(rig.seq().SetStepRoll(6, 4, 64));
  CHECK(rig.seq().StepRoll(6) == 4);  // The highest count on the step.

  std::array<SynthSequencer::StepNoteView, 8> views{};
  CHECK(rig.seq().StepNoteEvents(0, 6, views) == 3);
  int loud = 0;
  for (std::size_t index = 0; index < 3; ++index)
  {
    if (views[index].note == 64) CHECK(views[index].roll == 4);
    else CHECK(views[index].roll == 2);
    loud += views[index].roll;
  }
  CHECK(loud == 8);
}

void TestSurvivesSnapshot()
{
  // CaptureState / RestoreState is what a project save and a pattern copy go through.
  Rig rig;
  rig.Place(9, 60, 24, 3);
  const auto state = rig.seq().CaptureState();
  CHECK(state);
  CHECK(synth::SynthSequencer::ValidateState(*state));
  Rig other;
  CHECK(other.seq().RestoreState(*state));
  CHECK(other.seq().StepRoll(9) == 3);
}

void TestEditLaneCycles()
{
  // ENTER walks NOTE -> LEN -> VEL -> TIME -> ROLL -> NOTE.
  Rig rig;
  CHECK(rig.seq().BuildUiState(0).editLane == synth::SequencerEditLane::Note);
  rig.seq().CycleEditLane();
  CHECK(rig.seq().BuildUiState(0).editLane == synth::SequencerEditLane::Length);
  rig.seq().CycleEditLane();
  CHECK(rig.seq().BuildUiState(0).editLane == synth::SequencerEditLane::Velocity);
  rig.seq().CycleEditLane();
  CHECK(rig.seq().BuildUiState(0).editLane == synth::SequencerEditLane::Micro);
  rig.seq().CycleEditLane();
  CHECK(rig.seq().BuildUiState(0).editLane == synth::SequencerEditLane::Roll);
  rig.seq().CycleEditLane();
  CHECK(rig.seq().BuildUiState(0).editLane == synth::SequencerEditLane::Note);
  CHECK(std::string(synth::SequencerEditLaneLabel(synth::SequencerEditLane::Roll)) == "ROLL");
}

}  // namespace

int main()
{
  TestSubHitsLandOnPulses();
  TestRollKeepsTheStep();
  TestPlainNoteKeepsItsLength();
  TestEditing();
  TestChordScope();
  TestSurvivesSnapshot();
  TestEditLaneCycles();
  std::printf("roll_test: all checks passed\n");
  return 0;
}
