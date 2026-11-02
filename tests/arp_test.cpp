// Arpeggiator tests. The sequencer is built with CARDENZA_SEQUENCER_TEST_HOOKS: the clock worker
// never starts and every test steps the clock by hand, so emitted notes land on exact pulses.
#include "Applications/Synth/Runtime/Sequencer/SynthSequencer.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace {

using midi::MidiStatus;
using synth::ArpMode;
using synth::ArpSettings;
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
  std::uint8_t note = 0;  // controller number for a control change
  std::uint8_t velocity = 0;
  std::uint64_t tick = 0;
};

struct Capture {
  std::mutex mutex;
  std::vector<Event> events;
  // The clock pulse being run; between pulses, the one about to run.
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

  void RunTo(std::uint64_t tick)
  {
    CHECK(tick >= capture.tick);
    Run(tick - capture.tick);
  }

  std::uint64_t Now() const { return capture.tick; }

  std::vector<Event> Events()
  {
    std::lock_guard<std::mutex> lock(capture.mutex);
    return capture.events;
  }

  std::vector<Event> NoteEvents()
  {
    std::vector<Event> notes;
    for (const Event& event : Events())
      if (IsOn(event) || IsOff(event)) notes.push_back(event);
    return notes;
  }

  std::vector<Event> Ons()
  {
    std::vector<Event> ons;
    for (const Event& event : Events())
      if (IsOn(event)) ons.push_back(event);
    return ons;
  }

  std::vector<int> OnNotes()
  {
    std::vector<int> notes;
    for (const Event& event : Ons()) notes.push_back(event.note);
    return notes;
  }

  std::vector<std::uint64_t> OnTicks()
  {
    std::vector<std::uint64_t> ticks;
    for (const Event& event : Ons()) ticks.push_back(event.tick);
    return ticks;
  }

  std::size_t EventCount() { return Events().size(); }
};

ArpSettings Settings(ArpMode mode, std::uint8_t rate = 3, std::uint8_t octaves = 1, std::uint8_t gate = 50, bool latch = false)
{
  ArpSettings settings;
  settings.mode = mode;
  settings.rate = rate;
  settings.octaves = octaves;
  settings.gate = gate;
  settings.latch = latch;
  return settings;
}

// Every note-on is ended (note-off or all-notes-off on its track), none starts while the same
// note still sounds, and at most maxVoices notes sound at once.
bool Clean(const std::vector<Event>& events, int maxVoices = 1)
{
  std::array<std::array<int, 128>, synth::kSynthTrackCount> sounding{};
  int voices = 0;
  for (const Event& event : events)
  {
    if (event.track >= sounding.size()) return false;
    auto& track = sounding[event.track];
    if (IsOn(event))
    {
      if (track[event.note] != 0) return false;
      track[event.note] = 1;
      if (++voices > maxVoices) return false;
    }
    else if (IsOff(event))
    {
      if (track[event.note] != 0) --voices;
      track[event.note] = 0;
    }
    else if (event.status == MidiStatus::ControlChange && (event.note == 120 || event.note == 123))
    {
      for (int& note : track)
      {
        if (note != 0) --voices;
        note = 0;
      }
    }
  }
  return voices == 0;
}

std::vector<std::uint64_t> Spaced(std::uint64_t first, std::uint64_t period, std::size_t count)
{
  std::vector<std::uint64_t> ticks;
  for (std::size_t index = 0; index < count; ++index) ticks.push_back(first + index * period);
  return ticks;
}

void TestOffIsInert()
{
  Rig rig;
  CHECK(rig.seq().GetArpSettings(0) == ArpSettings{});
  rig.seq().ArpNoteOn(0, 60, 100);
  CHECK(!rig.seq().ArpActive());
  rig.Run(200);
  CHECK(rig.Events().empty());
  const auto ui = rig.seq().BuildUiState(0);
  CHECK(!ui.arpActive && ui.arpNoteCount == 0 && ui.arpSounding == -1 && ui.arpTick == 0 && ui.arpLastNote == -1);

  // With the arp off the transport plays the pattern exactly as before, and nothing else.
  auto state = rig.seq().CaptureState();
  auto& notes = state->tracks[0].patterns[0].steps;
  notes.Edit(0).notes[0] = {60, 100, 24, 0, true};
  notes.Edit(4).notes[0] = {62, 90, 12, 0, true};
  CHECK(rig.seq().RestoreState(*state));
  rig.seq().Play();
  const std::uint64_t start = rig.Now();
  rig.seq().ArpNoteOn(0, 72, 100);  // Off: ignored while playing too.
  rig.Run(384);
  const auto events = rig.NoteEvents();
  CHECK(events.size() == 4);
  CHECK(IsOn(events[0]) && events[0].note == 60 && events[0].tick == start);
  CHECK(IsOff(events[1]) && events[1].note == 60 && events[1].tick == start + 24);
  CHECK(IsOn(events[2]) && events[2].note == 62 && events[2].velocity == 90 && events[2].tick == start + 96);
  CHECK(IsOff(events[3]) && events[3].note == 62 && events[3].tick == start + 108);
  rig.seq().Stop();
  CHECK(Clean(rig.Events()));
}

void TestUpOrderAndTiming()
{
  Rig rig;
  rig.seq().SetArpSettings(0, Settings(ArpMode::Up));
  // The first press sounds at once; the chord's other notes join before the next one.
  rig.seq().ArpNoteOn(0, 60, 100);
  CHECK(rig.OnNotes() == std::vector<int>{60});
  rig.seq().ArpNoteOn(0, 64, 90);
  rig.seq().ArpNoteOn(0, 67, 80);
  CHECK(rig.seq().ArpActive());
  auto ui = rig.seq().BuildUiState(0);
  CHECK(ui.arpActive && ui.arpTrack == 0 && ui.arpNoteCount == 3 && ui.arpSounding == 60 && ui.arpTick == 1 && ui.arpLastNote == 60);
  CHECK(ui.arpNotes[0] == 60 && ui.arpNotes[1] == 64 && ui.arpNotes[2] == 67 && ui.arpNotes[3] == 0);

  rig.Run(130);
  CHECK(rig.OnNotes() == (std::vector<int>{60, 64, 67, 60, 64, 67}));
  CHECK(rig.OnTicks() == Spaced(0, 24, 6));
  const auto ons = rig.Ons();
  CHECK(ons[0].velocity == 100 && ons[1].velocity == 90 && ons[2].velocity == 80 && ons[3].velocity == 100);
  // Gate 50% of a sixteenth: twelve pulses.
  for (const Event& event : rig.Events())
    if (IsOff(event)) CHECK(event.tick % 24 == 12);
  ui = rig.seq().BuildUiState(0);
  CHECK(ui.arpSounding == 67 && ui.arpTick == 6);
  rig.Run(5);  // Past the last gate: the latest pitch stays readable.
  ui = rig.seq().BuildUiState(0);
  CHECK(ui.arpSounding == -1 && ui.arpLastNote == 67 && ui.arpTick == 6);

  // Releasing every key stops the arp.
  rig.seq().ArpNoteOff(0, 64);
  rig.seq().ArpNoteOff(0, 60);
  CHECK(rig.seq().ArpActive() && rig.seq().BuildUiState(0).arpNoteCount == 1);
  rig.seq().ArpNoteOff(0, 67);
  CHECK(!rig.seq().ArpActive());
  const std::size_t count = rig.EventCount();
  rig.Run(200);
  CHECK(rig.EventCount() == count);
  CHECK(Clean(rig.Events()));
  // Velocity 0 is a note-off.
  rig.seq().ArpNoteOn(0, 50, 100);
  rig.seq().ArpNoteOn(0, 50, 0);
  CHECK(!rig.seq().ArpActive() && Clean(rig.Events()));
}

void TestOrders()
{
  {
    Rig rig;  // DOWN: after the first press, from the top.
    rig.seq().SetArpSettings(0, Settings(ArpMode::Down));
    for (std::uint8_t note : {60, 64, 67}) rig.seq().ArpNoteOn(0, note, 100);
    rig.Run(24 * 6 - 1);
    CHECK(rig.OnNotes() == (std::vector<int>{60, 67, 64, 60, 67, 64}));
  }
  {
    Rig rig;  // UP-DN over two octaves: no repeated note at either turn.
    rig.seq().SetArpSettings(0, Settings(ArpMode::UpDown, 3, 2));
    for (std::uint8_t note : {60, 64}) rig.seq().ArpNoteOn(0, note, 100);
    rig.Run(24 * 10 - 1);
    CHECK(rig.OnNotes() == (std::vector<int>{60, 64, 72, 76, 72, 64, 60, 64, 72, 76}));
  }
  {
    Rig rig;  // UP-DN with one note repeats it.
    rig.seq().SetArpSettings(0, Settings(ArpMode::UpDown));
    rig.seq().ArpNoteOn(0, 62, 100);
    rig.Run(24 * 3 - 1);
    CHECK(rig.OnNotes() == (std::vector<int>{62, 62, 62}));
  }
  {
    Rig rig;  // UP over three octaves.
    rig.seq().SetArpSettings(0, Settings(ArpMode::Up, 3, 3));
    rig.seq().ArpNoteOn(0, 60, 100);
    rig.Run(24 * 4 - 1);
    CHECK(rig.OnNotes() == (std::vector<int>{60, 72, 84, 60}));
  }
  {
    Rig rig;  // Octaves past 127 are skipped.
    rig.seq().SetArpSettings(0, Settings(ArpMode::Up, 3, 4));
    rig.seq().ArpNoteOn(0, 100, 100);
    rig.seq().ArpNoteOn(0, 110, 100);
    rig.Run(24 * 7 - 1);
    CHECK(rig.OnNotes() == (std::vector<int>{100, 110, 112, 122, 124, 100, 110}));
  }
  {
    Rig rig;  // ORDER: press order, then the same an octave up.
    rig.seq().SetArpSettings(0, Settings(ArpMode::Played, 3, 2));
    for (std::uint8_t note : {67, 60, 64}) rig.seq().ArpNoteOn(0, note, 100);
    rig.Run(24 * 8 - 1);
    CHECK(rig.OnNotes() == (std::vector<int>{67, 60, 64, 79, 72, 76, 67, 60}));
    // Releasing a key keeps the place: the key after it comes next.
    rig.seq().ArpNoteOff(0, 64);
    rig.Run(24 * 3);
    const auto notes = rig.OnNotes();
    CHECK((std::vector<int>(notes.end() - 3, notes.end()) == std::vector<int>{79, 72, 67}));
  }
  {
    Rig rig;  // RANDOM: only held pitches, never the same twice in a row.
    rig.seq().SetArpSettings(0, Settings(ArpMode::Random, 3, 2));
    for (std::uint8_t note : {60, 62, 72}) rig.seq().ArpNoteOn(0, note, 100);  // 72 twice over octaves.
    rig.Run(24 * 80);
    const auto notes = rig.OnNotes();
    CHECK(notes.size() == 80);
    std::array<int, 128> seen{};
    for (std::size_t index = 0; index < notes.size(); ++index)
    {
      CHECK(notes[index] == 60 || notes[index] == 62 || notes[index] == 72 || notes[index] == 74 || notes[index] == 84);
      if (index > 0) CHECK(notes[index] != notes[index - 1]);
      ++seen[static_cast<std::size_t>(notes[index])];
    }
    for (int note : {60, 62, 72, 74, 84}) CHECK(seen[static_cast<std::size_t>(note)] > 0);
  }
  {
    Rig rig;  // RANDOM with one pitch repeats it.
    rig.seq().SetArpSettings(0, Settings(ArpMode::Random));
    rig.seq().ArpNoteOn(0, 70, 100);
    rig.Run(24 * 3 - 1);
    CHECK(rig.OnNotes() == (std::vector<int>{70, 70, 70}));
  }
  {
    Rig rig;  // Sixteen notes at most; a held pitch pressed again is not added twice.
    rig.seq().SetArpSettings(0, Settings(ArpMode::Up));
    for (std::uint8_t note = 40; note < 60; ++note) rig.seq().ArpNoteOn(0, note, 100);
    rig.seq().ArpNoteOn(0, 40, 50);
    const auto ui = rig.seq().BuildUiState(0);
    CHECK(ui.arpNoteCount == synth::kArpMaxNotes && ui.arpNotes[15] == 55);
  }
}

void TestRateAndGate()
{
  for (std::size_t rate = 0; rate < synth::kArpRateCount; ++rate)
  {
    for (std::uint8_t gate : {10, 50, 100})
    {
      Rig rig;
      rig.seq().SetArpSettings(0, Settings(ArpMode::Up, static_cast<std::uint8_t>(rate), 1, gate));
      rig.seq().ArpNoteOn(0, 60, 100);
      const std::uint64_t period = synth::kArpRatePulses[rate];
      rig.Run(period * 4 + 1);
      rig.seq().ArpNoteOff(0, 60);
      CHECK(rig.OnTicks() == Spaced(0, period, 5));
      const std::uint64_t length = std::max<std::uint64_t>(1, period * gate / 100);
      // Notes alternate on/off; a full gate ends each note right before the next starts.
      const auto events = rig.Events();
      CHECK(events.size() == 10);
      for (std::size_t index = 0; index + 1 < events.size(); index += 2)
      {
        CHECK(IsOn(events[index]) && IsOff(events[index + 1]));
        if (index + 2 < events.size()) CHECK(events[index + 1].tick - events[index].tick == length);
      }
      CHECK(Clean(events));
    }
  }
}

void TestLatch()
{
  Rig rig;
  rig.seq().SetArpSettings(0, Settings(ArpMode::Up, 3, 1, 50, true));
  rig.seq().ArpNoteOn(0, 60, 100);
  rig.seq().ArpNoteOn(0, 64, 100);
  rig.Run(20);
  rig.seq().ArpNoteOff(0, 60);
  rig.seq().ArpNoteOff(0, 64);
  // Released keys stay latched and keep playing.
  CHECK(rig.seq().ArpActive() && rig.seq().BuildUiState(0).arpNoteCount == 2);
  rig.RunTo(60);
  CHECK(rig.OnNotes() == (std::vector<int>{60, 64, 60}));

  // The next press replaces the latched set, without breaking the arp's timing.
  const std::size_t before = rig.Ons().size();
  rig.seq().ArpNoteOn(0, 67, 100);
  CHECK(rig.Ons().size() == before);
  auto ui = rig.seq().BuildUiState(0);
  CHECK(ui.arpNoteCount == 1 && ui.arpNotes[0] == 67);
  rig.RunTo(97);
  CHECK(rig.OnNotes() == (std::vector<int>{60, 64, 60, 67, 67}));
  CHECK(rig.OnTicks().back() == 96);

  // While a key is down, presses add to the set; a released key stays latched.
  rig.seq().ArpNoteOn(0, 72, 100);
  rig.seq().ArpNoteOff(0, 67);
  ui = rig.seq().BuildUiState(0);
  CHECK(ui.arpNoteCount == 2 && ui.arpNotes[0] == 67 && ui.arpNotes[1] == 72);

  // Latch off keeps the keys still held.
  auto settings = rig.seq().GetArpSettings(0);
  settings.latch = false;
  rig.seq().SetArpSettings(0, settings);
  ui = rig.seq().BuildUiState(0);
  CHECK(ui.arpActive && ui.arpNoteCount == 1 && ui.arpNotes[0] == 72);
  const std::size_t kept = rig.Ons().size();
  rig.Run(24 * 3);
  const auto notes = rig.OnNotes();
  CHECK(notes.size() == kept + 3);
  CHECK(std::all_of(notes.begin() + static_cast<std::ptrdiff_t>(kept), notes.end(), [](int note) { return note == 72; }));
  rig.seq().ArpNoteOff(0, 72);
  CHECK(!rig.seq().ArpActive());

  // Latch off with nothing held releases the arp and its sounding note.
  rig.seq().SetArpSettings(0, Settings(ArpMode::Up, 3, 1, 100, true));
  rig.seq().ArpNoteOn(0, 62, 100);
  rig.seq().ArpNoteOff(0, 62);
  rig.Run(30);
  CHECK(rig.seq().ArpActive() && rig.seq().BuildUiState(0).arpSounding == 62);
  settings = rig.seq().GetArpSettings(0);
  settings.latch = false;
  rig.seq().SetArpSettings(0, settings);
  const auto events = rig.Events();
  CHECK(IsOff(events.back()) && events.back().note == 62 && events.back().tick == rig.Now());
  CHECK(!rig.seq().ArpActive() && rig.seq().BuildUiState(0).arpSounding == -1);
  const std::size_t count = rig.EventCount();
  rig.Run(100);
  CHECK(rig.EventCount() == count);
  CHECK(Clean(rig.Events()));
}

// Every track has its own arp with its own settings; they run side by side and a track change
// leaves them alone.
void TestTracksRunTheirOwnArps()
{
  Rig rig;
  rig.seq().SetArpSettings(0, Settings(ArpMode::Up, 3, 1, 100));
  rig.seq().ArpNoteOn(1, 62, 100);  // Track 1's arp is off: ignored.
  CHECK(!rig.seq().ArpActive(1) && rig.EventCount() == 0);
  rig.seq().ArpNoteOn(0, 60, 100);
  rig.seq().ArpNoteOn(0, 64, 100);
  rig.Run(10);
  rig.seq().SetArpSettings(1, Settings(ArpMode::Down, 1, 1, 50));
  CHECK(rig.seq().GetArpSettings(0).mode == ArpMode::Up && rig.seq().GetArpSettings(1).mode == ArpMode::Down);
  CHECK(rig.seq().GetArpSettings(2) == ArpSettings{});
  // Track 1 starts at once; track 0 runs on untouched.
  rig.seq().ArpNoteOn(1, 62, 100);
  rig.seq().ArpNoteOn(1, 69, 100);
  CHECK(rig.seq().ArpActive(0) && rig.seq().ArpActive(1) && !rig.seq().ArpActive(2));
  auto events = rig.Events();
  CHECK(IsOn(events.back()) && events.back().track == 1 && events.back().note == 62 && events.back().tick == 10);
  for (const auto& event : events) CHECK(event.track != 0 || !IsOff(event) || event.tick != 10);
  const auto ui0 = rig.seq().BuildUiState(0);
  const auto ui1 = rig.seq().BuildUiState(1);
  CHECK(ui0.arpActive && ui0.arpTrack == 0 && ui0.arpNoteCount == 2);
  CHECK(ui1.arpActive && ui1.arpTrack == 1 && ui1.arpNoteCount == 2 && ui1.arpSounding == 62);
  CHECK(!rig.seq().BuildUiState(2).arpActive);
  rig.Run(60);
  // Each on its own grid: track 0 every sixteenth from tick 0, track 1 every eighth from tick 10.
  bool track0 = false, track1 = false;
  for (const auto& on : rig.Ons())
  {
    if (on.track == 0 && on.tick == 48) track0 = true;
    if (on.track == 1 && on.tick == 58 && on.note == 69) track1 = true;
  }
  CHECK(track0 && track1);

  // Selecting another track leaves both running; releasing one track stops only its arp.
  rig.seq().SetActiveTrack(2);
  CHECK(rig.seq().ArpActive(0) && rig.seq().ArpActive(1));
  rig.seq().ArpRelease(0);
  CHECK(!rig.seq().ArpActive(0) && rig.seq().ArpActive(1));
  rig.seq().ArpNoteOff(1, 62);
  rig.seq().ArpNoteOff(1, 69);
  CHECK(!rig.seq().ArpActive());
  const std::size_t count = rig.EventCount();
  rig.Run(100);
  CHECK(rig.EventCount() == count && Clean(rig.Events(), 2));  // two arps sound together
}

void TestOffAndReleaseSilence()
{
  Rig rig;
  rig.seq().SetArpSettings(0, Settings(ArpMode::Up, 3, 1, 100));
  rig.seq().ArpNoteOn(0, 60, 100);
  rig.Run(5);
  CHECK(rig.seq().BuildUiState(0).arpSounding == 60);
  auto settings = rig.seq().GetArpSettings(0);
  settings.mode = ArpMode::Off;
  rig.seq().SetArpSettings(0, settings);
  auto events = rig.Events();
  CHECK(events.size() == 2 && IsOff(events[1]) && events[1].note == 60 && events[1].tick == 5);
  CHECK(!rig.seq().ArpActive() && rig.seq().BuildUiState(0).arpSounding == -1);
  rig.Run(100);
  CHECK(rig.EventCount() == 2);
  // The held keys went with it: switching back on plays nothing until the next press.
  settings.mode = ArpMode::Up;
  rig.seq().SetArpSettings(0, settings);
  CHECK(!rig.seq().ArpActive());
  rig.Run(100);
  CHECK(rig.EventCount() == 2);

  // ArpReleaseAll, as the panic and project paths use it.
  rig.seq().ArpNoteOn(0, 64, 100);
  rig.Run(5);
  rig.seq().ArpReleaseAll();
  events = rig.Events();
  CHECK(IsOff(events.back()) && events.back().note == 64);
  CHECK(!rig.seq().ArpActive());
  rig.Run(100);
  CHECK(rig.EventCount() == events.size() && Clean(rig.Events()));
}

void TestSettings()
{
  Rig rig;
  ArpSettings wild = Settings(static_cast<ArpMode>(200), 99, 0, 0);
  rig.seq().SetArpSettings(0, wild);
  ArpSettings got = rig.seq().GetArpSettings(0);
  CHECK(got.mode == ArpMode::Off && got.rate == synth::kArpRateCount - 1 && got.octaves == 1 && got.gate == synth::kArpMinGate);
  rig.seq().SetArpSettings(0, Settings(ArpMode::UpDown, 2, 9, 250, true));
  got = rig.seq().GetArpSettings(0);
  CHECK(got == Settings(ArpMode::UpDown, 2, synth::kArpMaxOctaves, synth::kArpMaxGate, true));
  CHECK(got != Settings(ArpMode::UpDown, 2, synth::kArpMaxOctaves, synth::kArpMaxGate, false));

  // A new rate applies from the next note, counted from the last one.
  rig.seq().SetArpSettings(0, Settings(ArpMode::Up, 0));
  rig.seq().ArpNoteOn(0, 60, 100);
  rig.Run(10);
  rig.seq().SetArpSettings(0, Settings(ArpMode::Up, 3));
  rig.RunTo(49);
  CHECK(rig.OnTicks() == (std::vector<std::uint64_t>{0, 24, 48}));
  // Gate changes apply to the next note.
  rig.seq().SetArpSettings(0, Settings(ArpMode::Up, 3, 1, 10));
  rig.RunTo(80);
  const auto events = rig.Events();
  CHECK(IsOff(events.back()) && events.back().tick == 72 + 2);
}

void TestGridWhilePlaying()
{
  {
    Rig rig;
    rig.seq().SetArpSettings(0, Settings(ArpMode::Up));
    rig.seq().Play();
    rig.Run(5);
    // Pressed between grid lines: sounds at once, then joins the sixteenth grid.
    rig.seq().ArpNoteOn(0, 60, 100);
    rig.RunTo(80);
    CHECK(rig.OnTicks() == (std::vector<std::uint64_t>{5, 24, 48, 72}));
    rig.RunTo(90);
    rig.seq().ArpNoteOff(0, 60);
    // Pressed just before a grid line: that line is skipped rather than played a few pulses later.
    rig.RunTo(92);
    rig.seq().ArpNoteOn(0, 62, 100);
    rig.RunTo(150);
    const auto ticks = rig.OnTicks();
    CHECK((std::vector<std::uint64_t>(ticks.end() - 3, ticks.end()) == std::vector<std::uint64_t>{92, 120, 144}));
    rig.seq().ArpNoteOff(0, 62);
    rig.seq().Stop();
    CHECK(Clean(rig.Events()));
  }
  {
    Rig rig;  // Swing delays odd sixteenths, as it does sequenced steps.
    rig.seq().SetSwing(75);
    rig.seq().SetArpSettings(0, Settings(ArpMode::Up));
    rig.seq().Play();
    rig.seq().ArpNoteOn(0, 60, 100);
    rig.RunTo(150);
    CHECK(rig.OnTicks() == (std::vector<std::uint64_t>{0, 36, 48, 84, 96, 132, 144}));
    rig.seq().Stop();
    rig.seq().ArpReleaseAll();
    CHECK(Clean(rig.Events()));
  }
  {
    Rig rig;  // Eighths sit on even sixteenths: swing leaves them alone.
    rig.seq().SetSwing(75);
    rig.seq().SetArpSettings(0, Settings(ArpMode::Up, 1));
    rig.seq().Play();
    rig.seq().ArpNoteOn(0, 60, 100);
    rig.RunTo(150);
    CHECK(rig.OnTicks() == (std::vector<std::uint64_t>{0, 48, 96, 144}));
    rig.seq().ArpReleaseAll();
    rig.seq().Stop();
    CHECK(Clean(rig.Events()));
  }
}

void TestTransportStartStop()
{
  Rig rig;
  rig.seq().SetArpSettings(0, Settings(ArpMode::Up));
  rig.seq().ArpNoteOn(0, 60, 100);
  rig.seq().ArpNoteOn(0, 64, 100);
  rig.RunTo(50);
  CHECK(!rig.seq().Playing() && rig.OnTicks() == (std::vector<std::uint64_t>{0, 24, 48}));

  // Play locks the running arp onto the transport grid; a note just played counts as the downbeat.
  rig.seq().Play();
  rig.RunTo(130);
  CHECK(rig.OnTicks() == (std::vector<std::uint64_t>{0, 24, 48, 74, 98, 122}));

  // Stop cuts the sounding note but the held arp runs on, on its own clock, without a gap.
  CHECK(rig.seq().BuildUiState(0).arpSounding >= 0);
  rig.seq().Stop();
  CHECK(!rig.seq().Playing() && rig.seq().ArpActive() && rig.seq().BuildUiState(0).arpSounding == -1);
  rig.RunTo(200);
  CHECK(rig.OnTicks() == (std::vector<std::uint64_t>{0, 24, 48, 74, 98, 122, 146, 170, 194}));

  // Long enough after the last note, Play starts the arp on the transport's first pulse.
  rig.RunTo(210);
  rig.seq().Play();
  rig.RunTo(260);
  CHECK(rig.OnTicks() == (std::vector<std::uint64_t>{0, 24, 48, 74, 98, 122, 146, 170, 194, 210, 234, 258}));

  // Resume continues the transport; the arp moves onto the grid without a double note.
  rig.seq().Stop();
  rig.RunTo(300);
  rig.seq().Resume();
  rig.RunTo(400);
  const auto ticks = rig.OnTicks();
  for (std::size_t index = 1; index < ticks.size(); ++index) CHECK(ticks[index] - ticks[index - 1] >= 12);
  rig.seq().Stop();
  rig.seq().ArpNoteOff(0, 60);
  rig.seq().ArpNoteOff(0, 64);
  CHECK(!rig.seq().ArpActive());
  const std::size_t count = rig.EventCount();
  rig.Run(100);
  CHECK(rig.EventCount() == count && Clean(rig.Events()));
}

void TestRecording()
{
  using NoteEvent = SynthSequencer::NoteEvent;
  auto noteAt = [](const SynthSequencer::ProjectState& state, std::size_t step) -> const NoteEvent* {
    for (const NoteEvent& note : state.tracks[0].patterns[0].steps[step].notes)
      if (note.active) return &note;
    return nullptr;
  };
  auto activeNotes = [](const SynthSequencer::ProjectState& state) {
    std::size_t count = 0;
    const auto& steps = state.tracks[0].patterns[0].steps;
    for (std::size_t step = steps.NextInUse(0); step < steps.size(); step = steps.NextInUse(step + 1))
      for (const NoteEvent& note : steps[step].notes) count += note.active ? 1 : 0;
    return count;
  };

  {
    Rig rig;  // Playing and armed: every arp note is overdubbed with its own gate length.
    rig.seq().SetArpSettings(0, Settings(ArpMode::Up));
    rig.seq().SetRecordArmed(true);
    rig.seq().Play();
    rig.seq().ArpNoteOn(0, 60, 100);
    rig.seq().ArpNoteOn(0, 64, 90);
    rig.RunTo(90);
    rig.seq().ArpNoteOff(0, 60);
    rig.seq().ArpNoteOff(0, 64);
    rig.seq().Stop();
    // The first note was recorded ahead of the playhead: the pattern did not sound it again.
    CHECK(rig.OnTicks() == (std::vector<std::uint64_t>{0, 24, 48, 72}));
    const auto state = rig.seq().CaptureState();
    CHECK(activeNotes(*state) == 4);
    const std::array<std::uint8_t, 4> pitches{{60, 64, 60, 64}};
    for (std::size_t step = 0; step < pitches.size(); ++step)
    {
      const NoteEvent* note = noteAt(*state, step);
      CHECK(note != nullptr && note->note == pitches[step] && note->microstep == 0 && note->lengthPulses == 12);
      CHECK(note->velocity == (pitches[step] == 60 ? 100 : 90));
    }
    CHECK(SynthSequencer::ValidateState(*state));
    CHECK(Clean(rig.Events()));
  }
  {
    Rig rig;  // Armed but stopped: the free-running arp records nothing.
    rig.seq().SetArpSettings(0, Settings(ArpMode::Up));
    rig.seq().SetRecordArmed(true);
    rig.seq().ArpNoteOn(0, 60, 100);
    rig.Run(100);
    rig.seq().ArpReleaseAll();
    CHECK(activeNotes(*rig.seq().CaptureState()) == 0);
  }
  {
    Rig rig;  // Playing but not armed: nothing either.
    rig.seq().SetArpSettings(0, Settings(ArpMode::Up));
    rig.seq().Play();
    rig.seq().ArpNoteOn(0, 60, 100);
    rig.Run(100);
    rig.seq().ArpReleaseAll();
    rig.seq().Stop();
    CHECK(activeNotes(*rig.seq().CaptureState()) == 0);
  }
}

void TestProjectPathsRelease()
{
  Rig rig;
  rig.seq().SetArpSettings(0, Settings(ArpMode::Up, 3, 1, 100, true));
  rig.seq().ArpNoteOn(0, 60, 100);
  rig.Run(10);
  const auto state = rig.seq().CaptureState();
  CHECK(rig.seq().RestoreState(*state));
  CHECK(!rig.seq().ArpActive() && rig.seq().BuildUiState(0).arpSounding == -1);
  std::size_t count = rig.EventCount();
  rig.Run(100);
  CHECK(rig.EventCount() == count);
  // The settings are a performance control: a project load keeps them.
  CHECK(rig.seq().GetArpSettings(0) == Settings(ArpMode::Up, 3, 1, 100, true));

  rig.seq().ArpNoteOn(0, 62, 100);
  rig.Run(10);
  rig.seq().Reset();
  CHECK(!rig.seq().ArpActive());
  count = rig.EventCount();
  rig.Run(100);
  CHECK(rig.EventCount() == count && Clean(rig.Events()));
}

// The real clock worker: a held arp runs while the transport is stopped, and stops with the keys.
void TestClockWorker()
{
  using namespace std::chrono_literals;
  Capture capture;
  auto sequencer = std::make_unique<SynthSequencer>();
  sequencer->SetMidiSink(Sink, &capture);
  sequencer->SetTempo(300);
  sequencer->SetArpSettings(0, Settings(ArpMode::Up, 5));  // 1/32 at 300 BPM: one note per 25 ms.
  auto ons = [&capture] {
    std::lock_guard<std::mutex> lock(capture.mutex);
    return static_cast<std::size_t>(std::count_if(capture.events.begin(), capture.events.end(), IsOn));
  };
  sequencer->ArpNoteOn(0, 60, 100);
  sequencer->ArpNoteOn(0, 67, 100);
  std::this_thread::sleep_for(300ms);
  CHECK(!sequencer->Playing());
  CHECK(ons() >= 4);
  sequencer->ArpNoteOff(0, 60);
  sequencer->ArpNoteOff(0, 67);
  std::size_t settled = 0;
  {
    std::lock_guard<std::mutex> lock(capture.mutex);
    settled = capture.events.size();
  }
  std::this_thread::sleep_for(100ms);
  {
    std::lock_guard<std::mutex> lock(capture.mutex);
    CHECK(capture.events.size() == settled);
    CHECK(Clean(capture.events));
  }
  // The transport still starts and stops normally afterwards, and the arp follows it.
  sequencer->ArpNoteOn(0, 64, 100);
  sequencer->Play();
  std::this_thread::sleep_for(100ms);
  CHECK(sequencer->Playing());
  sequencer->Stop();
  sequencer->ArpReleaseAll();
  sequencer.reset();
  std::lock_guard<std::mutex> lock(capture.mutex);
  CHECK(Clean(capture.events));
}

}  // namespace

// Patterns longer than the GRID: pages of steps, a length per pattern, memory on demand.
void TestLongPatterns()
{
  const std::uint8_t pitch = 72;
  {
    Rig rig;
    auto& seq = rig.seq();
    seq.SetActiveTrack(0);
    seq.SetTrackStepCount(0, 40);
    seq.PressStep(33, &pitch, 1);
    seq.ReleaseStep(33);
    auto ui = seq.BuildUiState(0);
    CHECK(ui.viewPageCount == 3 && ui.viewPage == 0 && ui.firstViewStep == 0 && !ui.steps[1].hasEvents);
    seq.SetViewPage(0, 2);
    ui = seq.BuildUiState(0);
    CHECK(ui.viewPage == 2 && ui.firstViewStep == 32 && ui.steps[1].hasEvents && ui.tracks[0].populatedStepCount == 1);
    // The note plays at its step of the long loop, and again after the wrap at 40 steps.
    seq.Play();
    const std::uint64_t start = rig.Now();
    ui = seq.BuildUiState(0);
    CHECK(ui.viewFollow && ui.viewPage == 0);  // Play goes back to following the playhead
    rig.Run(33 * 24 + 1);
    ui = seq.BuildUiState(0);
    CHECK(ui.currentStep == 33 && ui.viewPage == 2 && ui.steps[1].playing);
    rig.Run(40 * 24);
    auto events = rig.NoteEvents();
    CHECK(events.size() >= 3 && IsOn(events[0]) && events[0].tick == start + 33 * 24 && IsOn(events[2]) && events[2].tick == start + (33 + 40) * 24);
    // A page flipped by hand stays until the next Play.
    seq.SetViewPage(0, 0);
    rig.Run(24 * 8);
    ui = seq.BuildUiState(0);
    CHECK(ui.viewPage == 0 && !ui.viewFollow);
    seq.Stop();
    seq.Play();
    CHECK(seq.BuildUiState(0).viewFollow);
    seq.Stop();
    // Every pattern has its own length; a new one starts as long as the current one.
    CHECK(seq.SelectOrCreatePattern(1) && seq.TrackStepCount(0) == 40);
    seq.SetTrackStepCount(0, 256);
    CHECK(seq.SelectOrCreatePattern(0) && seq.TrackStepCount(0) == 40);
    CHECK(seq.PatternStepMask(0, 0) == (1u << (33 * 16 / 40)));
    CHECK(seq.CopyPattern(1, 2) && seq.TrackStepCount(0) == 256);
    CHECK(seq.DeletePattern(0) && seq.PatternCount(0) == 2 && seq.ActivePattern(0) == 1 && seq.TrackStepCount(0) == 256);
    // Length per pattern and all 256 steps survive a snapshot.
    seq.PressStep(255, &pitch, 1);
    seq.ReleaseStep(255);
    auto state = seq.CaptureState();
    CHECK(SynthSequencer::ValidateState(*state) && state->tracks[0].patternSteps[1] == 256);
    CHECK(state->tracks[0].patterns[1].steps[255].notes[0].active);
    // A snapshot keeps what it saw: later edits copy the page.
    seq.ClearStep(255);
    CHECK(!seq.StepHasEvents(255) && state->tracks[0].patterns[1].steps[255].notes[0].active);
    CHECK(seq.RestoreState(*state) && seq.StepHasEvents(255));
  }
  {
    // A snapshot is read on another thread (the session write) while the sequencer keeps editing the
    // same pages: the reader sees the snapshot, whole, for as long as it holds it.
    Rig rig;
    auto& seq = rig.seq();
    seq.SetActiveTrack(0);
    seq.SetTrackStepCount(0, 64);
    for (std::size_t step = 0; step < 64; step += 4)
    {
      seq.PressStep(step, &pitch, 1);
      seq.ReleaseStep(step);
    }
    for (int round = 0; round < 40; ++round)
    {
      std::shared_ptr<SynthSequencer::ProjectState> snapshot(seq.CaptureState().release());
      std::thread reader([snapshot] {
        const auto& steps = snapshot->tracks[0].patterns[0].steps;
        for (int pass = 0; pass < 20; ++pass)
        {
          std::size_t notes = 0;
          for (std::size_t step = steps.NextInUse(0); step < steps.size(); step = steps.NextInUse(step + 1))
            for (const auto& note : steps[step].notes) notes += note.active ? 1 : 0;
          CHECK(notes == 16);
        }
      });
      for (std::size_t step = 1; step < 64; step += 4)
      {
        seq.PressStep(step, &pitch, 1);
        seq.ReleaseStep(step);
        seq.ClearStep(step);
      }
      reader.join();
    }
    CHECK(seq.BuildUiState(0).tracks[0].populatedStepCount == 16);
  }
  {
    // Out of memory: the write is dropped and reported, nothing else changes.
    Rig rig;
    auto& seq = rig.seq();
    seq.SetActiveTrack(0);
    seq.SetTrackStepCount(0, 64);
    SynthSequencer::SetPageBudgetForTest(1);
    seq.PressStep(0, &pitch, 1);
    seq.ReleaseStep(0);
    CHECK(seq.StepHasEvents(0) && !SynthSequencer::ConsumePatternMemoryFull());
    seq.PressStep(1, &pitch, 1);  // the same page costs nothing
    seq.ReleaseStep(1);
    CHECK(seq.StepHasEvents(1) && !SynthSequencer::ConsumePatternMemoryFull());
    seq.PressStep(16, &pitch, 1);
    seq.ReleaseStep(16);
    CHECK(!seq.StepHasEvents(16) && SynthSequencer::ConsumePatternMemoryFull() && !SynthSequencer::ConsumePatternMemoryFull());
    // Reading, clearing and quantizing empty steps never asks for a page.
    seq.ClearStep(32);
    seq.QuantizeStep(48);
    seq.QuantizeActivePattern();
    CHECK(!SynthSequencer::ConsumePatternMemoryFull());
    // Emptying a page gives it back.
    seq.ClearStep(0);
    seq.ClearStep(1);
    SynthSequencer::SetPageBudgetForTest(0);
    seq.PressStep(16, &pitch, 1);
    seq.ReleaseStep(16);
    CHECK(!seq.StepHasEvents(16) && SynthSequencer::ConsumePatternMemoryFull());
    SynthSequencer::SetPageBudgetForTest(-1);
  }
}

int main()
{
  TestLongPatterns();
  TestOffIsInert();
  TestUpOrderAndTiming();
  TestOrders();
  TestRateAndGate();
  TestLatch();
  TestTracksRunTheirOwnArps();
  TestOffAndReleaseSilence();
  TestSettings();
  TestGridWhilePlaying();
  TestTransportStartStop();
  TestRecording();
  TestProjectPathsRelease();
  TestClockWorker();
  std::puts("arp tests passed");
  return 0;
}
