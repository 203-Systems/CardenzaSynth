// Groovebox UI workflow tests: the real GrooveApp and runtime on the simulator device, driven key by
// key with a fake clock so hold, repeat and transition timing is deterministic.
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>

#include "Applications/Synth/GrooveApp.h"
#include "Applications/Synth/UI/Groove/Render/Theme.h"
#include "Devices/Device.h"
#include "OS/System/System.h"

namespace {

using groove::KeyRole;
using groove::ModalKind;
using groove::Scene;

std::uint32_t gNow = 10000;
std::uint32_t FakeClock() { return gNow; }

struct Key {
  std::size_t row;
  std::size_t column;
};

constexpr Key kEsc{0, 0}, kTab{1, 0}, kFn{2, 0}, kCtrl{3, 0};
constexpr Key kAdd{0, 13}, kSub{1, 13}, kEnter{2, 13}, kPlay{3, 13};
// In text entry the top-right key ("del" on the cap) erases.
constexpr Key kDel{0, 13};

constexpr Key Digit(int digit) { return Key{0, static_cast<std::size_t>(digit == 0 ? 10 : digit)}; }
constexpr Key Grid(std::size_t index) { return Key{index / 4, 9 + index % 4}; }

class Harness {
public:
  explicit Harness(apps::GrooveApp& app) : app_(app) {}

  void Frames(int count = 1)
  {
    for (int index = 0; index < count; ++index)
    {
      gNow += 16;
      app_.Tick();
    }
  }
  void Wait(std::uint32_t ms) { Frames(static_cast<int>(ms / 16) + 1); }
  void Down(Key key) { app_.HandleMatrixKeyEvent(key.row, key.column, true); Frames(); }
  void Up(Key key) { app_.HandleMatrixKeyEvent(key.row, key.column, false); Frames(); }
  void Tap(Key key) { Down(key); Up(key); }
  void Hold(Key key, std::uint32_t ms) { Down(key); Wait(ms); }
  void Chord(Key modifier, Key key) { Down(modifier); Tap(key); Up(modifier); }
  void Render() { app_.RenderFrame(); }

  const groove::UiState& State() const { return app_.Ui().State(); }
  const groove::Modal* Modal() const { return app_.Ui().TopModal(); }
  synth::SequencerUiState Seq() const { return app_.Sequencer().BuildUiState(State().track); }
  apps::GrooveApp& App() { return app_; }

private:
  apps::GrooveApp& app_;
};

int ParameterValue(apps::GrooveApp& app, std::size_t track, const char* id)
{
  synth::SynthParameterInfo info;
  assert(app.Engine(track) != nullptr && app.Engine(track)->GetParameterInfo(id, info));
  return info.value;
}

// Scenes by their number on screen (0 SETUP, 1 INSTR, 2 SEQ, 3 TAPE) = their ESC map column.
void GoToScene(Harness& h, int number)
{
  h.Chord(kEsc, Grid(static_cast<std::size_t>(number)));
  h.Wait(200);
  assert(static_cast<int>(h.State().scene) == number);
}

// SETUP (scene 0) is reached only with ESC held + GRID column 1; CTRL then walks its pages.
void GoToSetupPage(Harness& h, std::uint8_t page)
{
  GoToScene(h, 0);
  while (h.App().Ui().CurrentPage() != page) h.Tap(kCtrl);
}

// An INSTR page of the current track by its tab.
void GoToInstrumentPage(Harness& h, const char* tab)
{
  GoToScene(h, 1);
  for (std::size_t tries = 0; tries < 16 && std::string(h.App().Ui().CurrentInstrumentPage()->tab) != tab; ++tries) h.Tap(kCtrl);
  assert(std::string(h.App().Ui().CurrentInstrumentPage()->tab) == tab);
}

void TestBoot(Harness& h)
{
  // It boots on the instrument, first page (VOICE).
  assert(h.State().scene == Scene::Instrument);
  assert(h.App().Ui().CurrentPage() == 0 && h.App().Ui().CurrentInstrumentPage()->graphic == groove::PageGraphic::Voice);
  // It starts on T2, the first melodic track: the pads show the piano layout, white keys on the
  // bottom row, black keys above with the gaps (Shift, D, J) dark; unpressed keys are half lit.
  assert(h.State().track == synth::kDefaultActiveTrack && h.App().EngineKind(h.State().track) != synth::SynthEngineKind::Drums);
  std::array<std::uint8_t, groove::kKeyCount> leds{};
  h.App().Ui().ComputeLeds(leds);
  for (std::size_t column = 1; column <= 8; ++column)
  {
    assert(leds[groove::KeyIndex(3, column)] == groove::kLedHalf);  // OPT .. N
    const bool gap = column == 1 || column == 4 || column == 8;    // Shift, D, J
    assert(leds[groove::KeyIndex(2, column)] == (gap ? groove::kLedOff : groove::kLedHalf));
  }
  // The walkthrough below was written for T1 (drums): start it there.
  h.Chord(kTab, Grid(0));
  assert(h.State().track == 0);
  // The note page starts as a piano roll.
  assert(h.App().Ui().View() == groove::SeqView::Roll && h.App().Ui().PageTab(Scene::Sequence, 0) == "ROLL");
  for (std::size_t track = 0; track < synth::kSynthTrackCount; ++track)
    assert(h.App().EngineKind(track) == apps::GrooveApp::kDefaultKinds[track]);
  assert(h.App().EngineKind(0) == synth::SynthEngineKind::Drums);
  // The walkthrough continues in SEQ.
  GoToScene(h, 2);
}

void TestNavigation(Harness& h)
{
  // Holding ESC shows the scene map: GRID columns are the scenes (any row), the pads stay silent,
  // and the release does not also go back.
  h.Hold(kEsc, 300);
  assert(h.App().Ui().EscOverlay());
  h.Tap(Key{1, 3});
  assert(h.State().padNote[1 * 8 + 2] == 0xFF);
  h.Tap(Grid(0));  // the first column is SETUP (the only way in)
  assert(h.State().scene == Scene::Setup);
  h.Tap(Grid(9));  // row 2, column 1: INSTR
  h.Up(kEsc);
  assert(h.State().scene == Scene::Instrument);
  assert(!h.App().Ui().EscOverlay());
  // Tapping ESC walks the music scenes in a ring: INSTR, SEQ, TAPE, INSTR (never SETUP).
  h.Tap(kEsc);
  assert(h.State().scene == Scene::Sequence);
  h.Tap(kEsc);
  assert(h.State().scene == Scene::Tape);
  h.Tap(kEsc);
  assert(h.State().scene == Scene::Instrument);
  GoToScene(h, 2);

  // TAB: tap = next track; held, the GRID is laid out like MIX (columns are tracks: select, mute,
  // solo, sequencer); TAB + SUB / ADD = previous / next.
  h.Tap(kTab);
  assert(h.State().track == 1);
  h.Hold(kTab, 300);
  assert(h.App().Ui().TabOverlay());
  h.Tap(Grid(3));
  assert(h.State().track == 3);
  h.Tap(Grid(4 + 2));
  assert(h.App().TrackMuted(2));
  h.Tap(Grid(4 + 2));
  assert(!h.App().TrackMuted(2));
  h.Up(kTab);
  assert(h.State().track == 3);
  h.Chord(kTab, kSub);
  assert(h.State().track == 2);
  h.Chord(kTab, kAdd);
  h.Chord(kTab, kAdd);
  assert(h.State().track == 0);

  // CTRL: a tap steps to the next page; a hold opens the page map without moving, the GRID jumps
  // to any page and the page stays after the release; CTRL + ADD / SUB step.
  h.Tap(kCtrl);
  assert(h.App().Ui().CurrentPage() == 1);
  h.Hold(kCtrl, 400);
  assert(h.App().Ui().CtrlOverlay());
  assert(h.App().Ui().CurrentPage() == 1);
  h.Tap(Grid(2));
  assert(h.App().Ui().CurrentPage() == 2);
  h.Tap(Grid(9));  // SEQ has three pages: nothing there
  assert(h.App().Ui().CurrentPage() == 2);
  h.Tap(kAdd);
  assert(h.App().Ui().CurrentPage() == 0);
  h.Tap(kSub);
  assert(h.App().Ui().CurrentPage() == 2);
  h.Up(kCtrl);
  assert(h.App().Ui().CurrentPage() == 2 && !h.App().Ui().CtrlOverlay());
  h.Tap(kCtrl);
  assert(h.App().Ui().CurrentPage() == 0);

  // Held FN shows the menu of this screen after the hold threshold: the SEQ items on rows 0-2, the
  // shared row (SAVE, TAP, REC, SETUP) at the bottom. Nothing touches the screen underneath until an
  // item runs.
  const auto steps = h.Seq().populatedStepCount;
  h.Down(kFn);
  assert(!h.App().Ui().FnOverlay());
  h.Wait(300);
  assert(h.App().Ui().FnOverlay() && h.App().Ui().Menu()[0].action == groove::MenuAction::Copy);
  assert(h.App().Ui().Menu()[15].action == groove::MenuAction::Setup);
  h.Render();
  // FN + GRID 16 (column 4) runs SETUP; a tap on ESC goes back to where it came from, and taps
  // never cycle into SETUP.
  h.Tap(Grid(15));
  h.Up(kFn);
  assert(h.State().scene == Scene::Setup && !h.App().Ui().FnOverlay());
  h.Render();
  h.Tap(kEsc);
  assert(h.State().scene == Scene::Sequence);
  for (int tap = 0; tap < 3; ++tap)
  {
    h.Tap(kEsc);
    assert(h.State().scene != Scene::Setup);
  }
  assert(h.State().scene == Scene::Sequence);
  // A tap on FN keeps the menu open; the GRID runs an item and closes it. FN + GRID runs it too.
  h.Tap(kFn);
  assert(h.App().Ui().MenuOpen());
  h.Render();
  h.Tap(Grid(5));  // VIEW: ROLL -> STEP
  assert(!h.App().Ui().MenuOpen() && h.App().Ui().View() == groove::SeqView::Step);
  h.Chord(kFn, Grid(5));
  assert(h.App().Ui().View() == groove::SeqView::Roll);
  // FN + CTRL steps back a page.
  h.Chord(kFn, kCtrl);
  assert(h.App().Ui().CurrentPage() == 2);
  h.Tap(kCtrl);
  assert(h.Seq().populatedStepCount == steps && h.App().Ui().CurrentPage() == 0);
  assert(h.State().track == 0);
}

void TestStepEditing(Harness& h)
{
  auto& sequencer = h.App().Sequencer();
  assert(h.State().scene == Scene::Sequence && h.App().Ui().CurrentPage() == 0 && h.State().track == 0);
  // Hold a pad, tap steps: the snare goes onto steps 1 and 3 without selecting them.
  h.Down(Key{2, 2});
  h.Tap(Grid(0));
  h.Tap(Grid(2));
  h.Up(Key{2, 2});
  auto seq = h.Seq();
  assert(seq.steps[0].hasEvents && seq.steps[0].lowestNote == 38);
  assert(seq.steps[2].hasEvents && !seq.hasHeldSteps);
  assert(h.State().dirty);

  // Tap selects; pads then toggle notes on the selection.
  h.Tap(Grid(0));
  seq = h.Seq();
  assert(seq.hasHeldSteps && seq.steps[0].held);
  h.Tap(Key{2, 3});
  assert(h.Seq().steps[0].noteCount == 2);
  h.Tap(Key{2, 3});
  assert(h.Seq().steps[0].noteCount == 1);

  // ENTER cycles the lane, ADD edits it.
  assert(h.Seq().editLane == synth::SequencerEditLane::Note);
  h.Tap(kEnter);
  assert(h.Seq().editLane == synth::SequencerEditLane::Length);
  h.Tap(kEnter);
  assert(h.Seq().editLane == synth::SequencerEditLane::Velocity);
  const std::string before = h.Seq().valueLabel;
  h.Tap(kSub);
  assert(h.Seq().valueLabel != before);

  // ESC clears the selection before it leaves the scene.
  h.Tap(kEsc);
  assert(!h.Seq().hasHeldSteps && h.State().scene == Scene::Sequence);

  // Fn + C / V copies step 1 onto a selected step.
  h.Tap(Grid(0));
  h.Chord(kFn, Key{3, 5});  // C
  h.Tap(kEsc);
  h.Tap(Grid(5));
  h.Chord(kFn, Key{3, 6});  // V
  assert(h.Seq().steps[5].hasEvents && h.Seq().steps[5].lowestNote == 38);
  // Fn + X clears the selected steps.
  h.Chord(kFn, Key{3, 4});
  assert(!h.Seq().steps[5].hasEvents);
  h.Tap(kEsc);
  assert(sequencer.ActivePattern(0) == 0);
}

void TestPatterns(Harness& h)
{
  auto& sequencer = h.App().Sequencer();
  h.Tap(kCtrl);
  assert(h.App().Ui().CurrentPage() == 1);
  // The slot after the last pattern creates one; while stopped the switch is immediate.
  h.Tap(Grid(1));
  assert(sequencer.PatternCount(0) == 2 && sequencer.ActivePattern(0) == 1);
  h.Tap(Grid(0));
  assert(sequencer.ActivePattern(0) == 0);
  // Fn + D duplicates the active pattern into the next free slot.
  h.Chord(kFn, Key{2, 4});
  assert(sequencer.PatternCount(0) == 3 && sequencer.ActivePattern(0) == 2);
  assert(h.Seq().patternHasEvents[2]);
  h.Tap(Grid(0));
  // TRACK page: step count and the sequence switch.
  h.Tap(kCtrl);
  assert(h.App().Ui().CurrentPage() == 2);
  h.Tap(Grid(1));
  h.Tap(kSub);
  assert(sequencer.TrackStepCount(0) == 15);
  h.Tap(kAdd);
  assert(sequencer.TrackStepCount(0) == 16);
  h.Tap(Grid(0));
  assert(!sequencer.TrackEnabled(0));
  h.Tap(Grid(0));
  assert(sequencer.TrackEnabled(0));
  h.Tap(kCtrl);
  assert(h.App().Ui().CurrentPage() == 0);
}

void TestTransport(Harness& h)
{
  auto& sequencer = h.App().Sequencer();
  h.Tap(kPlay);
  assert(sequencer.Playing());
  h.Chord(kFn, kPlay);
  assert(sequencer.RecordArmed());
  h.Tap(kPlay);
  assert(!sequencer.Playing());
  h.Chord(kFn, kPlay);
  assert(!sequencer.RecordArmed());
  // Tap tempo: the interval between two Fn + T taps sets the BPM.
  h.Down(kFn);
  const std::uint32_t first = gNow;
  h.Tap(Key{1, 5});
  h.Wait(480);
  const std::uint32_t second = gNow;
  h.Tap(Key{1, 5});
  h.Up(kFn);
  const std::uint32_t interval = second - first;
  assert(h.Seq().bpm == (60000 + interval / 2) / interval);
  // HOME is a panic stop.
  h.Tap(kPlay);
  assert(sequencer.Playing());
  h.App().HandleHomeButtonEvent(true);
  h.App().HandleHomeButtonEvent(false);
  assert(!sequencer.Playing());
}

void TestInstrument(Harness& h)
{
  auto& app = h.App();
  GoToScene(h, 1);
  h.Chord(kTab, Grid(1));  // Juno
  assert(app.EngineKind(1) == synth::SynthEngineKind::Juno);
  assert(h.App().Ui().CurrentPage() == 0);

  // VOICE: SUB / ADD step through presets immediately.
  const std::string first = app.Engine(1)->CurrentPresetId();
  h.Tap(kAdd);
  assert(app.Engine(1)->CurrentPresetId() != first);
  h.Tap(kSub);
  assert(app.Engine(1)->CurrentPresetId() == first);

  // VOICE, then the track's MIDI effects CHORD and ARP, then the engine: DCO.
  h.Tap(kCtrl);
  assert(std::string(h.App().Ui().CurrentInstrumentPage()->tab) == "CHORD");
  h.Tap(kCtrl);
  assert(std::string(h.App().Ui().CurrentInstrumentPage()->tab) == "ARP");
  h.Tap(kCtrl);
  // DCO page: GRID focuses PWM, ADD raises it, Fn + ENTER restores the default.
  assert(std::string(h.App().Ui().CurrentInstrumentPage()->tab) == "DCO");
  h.Tap(Grid(3));
  const int pwm = ParameterValue(app, 1, "dco.pwm");
  h.Tap(kAdd);
  assert(ParameterValue(app, 1, "dco.pwm") == std::min(127, pwm + 1));
  // FN + ADD / SUB take big steps (PWM by ten); FN + ENTER, or RESET in the menu, restores the
  // default.
  h.Chord(kFn, kAdd);
  assert(ParameterValue(app, 1, "dco.pwm") == std::min(127, pwm + 11));
  h.Chord(kFn, kSub);
  assert(ParameterValue(app, 1, "dco.pwm") == std::min(127, pwm + 1));
  h.Chord(kFn, kEnter);
  assert(ParameterValue(app, 1, "dco.pwm") == 48);
  h.Tap(kAdd);
  assert(h.App().Ui().Menu()[4].action == groove::MenuAction::Default && h.App().Ui().Menu()[4].detail == "PWM");
  h.Chord(kFn, Grid(4));
  assert(ParameterValue(app, 1, "dco.pwm") == 48);
  // A second press on a focused toggle flips it.
  const int saw = ParameterValue(app, 1, "dco.saw");
  h.Tap(Grid(1));
  h.Tap(Grid(1));
  assert(ParameterValue(app, 1, "dco.saw") != saw);

  // Auto-repeat on a held ADD.
  h.Tap(Grid(5));
  const int sub = ParameterValue(app, 1, "dco.sub");
  h.Hold(kAdd, 800);
  h.Up(kAdd);
  assert(ParameterValue(app, 1, "dco.sub") >= std::min(127, sub + 4));

  // Each track keeps its own INSTR page.
  h.Chord(kTab, Grid(2));
  assert(h.App().Ui().CurrentPage() == 0);
  h.Chord(kTab, Grid(1));
  assert(std::string(h.App().Ui().CurrentInstrumentPage()->tab) == "DCO");

  // FX page through the CTRL page map: drive, crush, filter, chorus and the two sends are sound
  // parameters of the track's engine.
  h.Down(kCtrl);
  h.Tap(Grid(7));
  h.Up(kCtrl);
  assert(std::string(h.App().Ui().CurrentInstrumentPage()->tab) == "FX");
  h.Tap(Grid(5));
  for (int press = 0; press < 11; ++press) h.Tap(kAdd);
  assert(ParameterValue(app, 1, "fx.delay") == 11);
  h.Tap(Grid(4));
  const int chorus = ParameterValue(app, 1, "fx.chorus");
  h.Tap(kEnter);
  assert(ParameterValue(app, 1, "fx.chorus") == (chorus + 1) % 4);
  h.Tap(Grid(2));
  h.Tap(kSub);
  assert(ParameterValue(app, 1, "fx.filter") == 63);
  assert(app.Engine(1)->DescribeParameterValue("fx.filter").rfind("LP", 0) == 0);
  h.Chord(kFn, kEnter);
  assert(ParameterValue(app, 1, "fx.filter") == 64 && app.Engine(1)->DescribeParameterValue("fx.filter") == "OFF");
  h.Render();

  // SETUP KEYS: the keyboard settings for every track. ESC goes back to INSTR.
  GoToSetupPage(h, groove::kSetupKeys);
  h.Render();
  h.Tap(Grid(3));  // OCTAVE
  h.Tap(kAdd);
  assert(h.State().keyboard.octave == 1);
  h.Chord(kFn, kEnter);  // FN + ENTER: the default
  assert(h.State().keyboard.octave == 0);
  h.Tap(kEsc);
  assert(h.State().scene == Scene::Instrument);

  // ENGINE picker: switch track 2 from Juno to DX7.
  while (h.App().Ui().CurrentPage() != 0) h.Tap(kCtrl);
  h.Tap(Grid(0));
  assert(h.Modal() != nullptr && h.Modal()->kind == ModalKind::EnginePicker);
  h.Render();
  h.Tap(Grid(0));
  assert(app.EngineKind(1) == synth::SynthEngineKind::Dx7);
  assert(h.Modal() == nullptr);
  // The preset browser: the pads do not type; FN + F opens the search field, ENTER filters. ADD
  // (the upper key) moves up, SUB down; ENTER loads.
  h.Tap(Grid(1));
  assert(h.Modal() != nullptr && h.Modal()->kind == ModalKind::PresetBrowser);
  const std::size_t all = h.Modal()->items.size();
  h.Tap(Key{2, 2});  // A: nothing
  assert(h.Modal()->kind == ModalKind::PresetBrowser && h.Modal()->search.empty() && h.Modal()->items.size() == all);
  const std::size_t start = h.Modal()->cursor;
  h.Tap(kSub);
  assert(h.Modal()->cursor == start + 1);
  h.Tap(kAdd);
  assert(h.Modal()->cursor == start);
  h.Chord(kFn, Key{2, 5});  // F
  assert(h.Modal()->kind == ModalKind::TextEntry);
  h.Tap(Key{2, 2});  // A
  h.Tap(kEnter);
  assert(h.Modal()->kind == ModalKind::PresetBrowser && h.Modal()->search == "A" && h.Modal()->items.size() < all);
  h.Render();
  h.Tap(kEsc);
  assert(h.Modal() != nullptr && h.Modal()->search.empty());
  h.Tap(kSub);
  h.Tap(kEnter);
  assert(h.Modal() == nullptr);
}

void TestTape(Harness& h)
{
  auto& sequencer = h.App().Sequencer();
  GoToScene(h, 3);
  while (h.App().Ui().CurrentPage() != groove::kTapeSong) h.Tap(kCtrl);
  assert(sequencer.SongLength() == 0);
  // The first empty column appends rows; each new row copies the one before.
  h.Tap(Grid(0));
  h.Tap(Grid(1));
  h.Tap(Grid(2));
  assert(sequencer.SongLength() == 3);
  assert(h.State().songRow == 2 && h.State().songTrack == 0);
  // GRID row = track, column = song row. ADD steps the cell through silent and the track's patterns.
  h.Tap(Grid(5));
  assert(h.State().songRow == 1 && h.State().songTrack == 1);
  const int before = sequencer.SongRowAt(1).patterns[1];
  h.Tap(kAdd);
  const int after = sequencer.SongRowAt(1).patterns[1];
  assert(after != before && after >= -1);
  h.Tap(kSub);
  assert(sequencer.SongRowAt(1).patterns[1] == before);
  // A second tap on the cell switches to the row length (BARS); ADD lengthens the row.
  h.Tap(Grid(5));
  assert(h.State().songLane == 1);
  h.Tap(kAdd);
  assert(sequencer.SongRowAt(1).repeats == 2);
  h.Tap(Grid(5));
  assert(h.State().songLane == 0);
  // ENTER cues the row and switches to song mode; Fn + ENTER toggles back to loop mode.
  h.Tap(kEnter);
  assert(sequencer.SongMode());
  h.Chord(kFn, kEnter);
  assert(!sequencer.SongMode());
  // Row clipboard: copy, paste after the cursor, delete, duplicate.
  h.Chord(kFn, Key{3, 5});  // C
  h.Chord(kFn, Key{3, 6});  // V
  assert(sequencer.SongLength() == 4 && sequencer.SongRowAt(2).repeats == 2);
  h.Chord(kFn, Key{3, 4});  // X
  assert(sequencer.SongLength() == 3);
  h.Chord(kFn, Key{2, 4});  // D
  assert(sequencer.SongLength() == 4);
  // Lights: the cursor cell shimmers at focus level, empty columns stay dark.
  std::array<std::uint8_t, groove::kKeyCount> levels{};
  h.App().Ui().ComputeLeds(levels);
  const std::size_t cursor = h.State().songTrack * 4 + (h.State().songRow - h.State().songScroll);
  assert(levels[groove::KeyIndex(cursor / 4, 9 + cursor % 4)] >= 12);
  h.Render();
}

void TestMix(Harness& h)
{
  auto& app = h.App();
  GoToScene(h, 3);
  while (h.App().Ui().CurrentPage() != groove::kTapeMix) h.Tap(kCtrl);
  h.Tap(Grid(4));   // mute T1
  h.Tap(Grid(9));   // solo T2
  assert(app.TrackMuted(0) && app.TrackSoloed(1));
  assert(!app.TrackAudible(0) && app.TrackAudible(1) && !app.TrackAudible(2));
  h.Tap(Grid(1));   // focus T2
  assert(h.State().track == 1);
  h.Tap(kSub);
  assert(app.TrackLevel(1) == 99);
  for (int press = 0; press < 10; ++press) h.Tap(kSub);
  assert(app.TrackLevel(1) == 89);
  h.Tap(kEnter);    // master focus
  const std::uint8_t master = app.MasterVolume();
  h.Tap(kSub);
  assert(app.MasterVolume() == master - 1);
  h.Tap(kEnter);
  h.Tap(Grid(9));
  assert(!app.TrackSoloed(1) && app.TrackAudible(2));
  // FN + a number does nothing (the menu is on the GRID); it never mutes.
  h.Chord(kFn, Digit(1));
  assert(app.TrackMuted(0) && h.Modal() == nullptr);
  // The MIX menu: UNMUTE ALL, then T1 muted again on the GRID.
  h.Chord(kFn, Grid(0));
  assert(!app.TrackMuted(0));
  h.Tap(Grid(4));
  assert(app.TrackMuted(0));
}

void TestSendsAndFx(Harness& h)
{
  auto& app = h.App();
  GoToScene(h, 3);
  // SENDS: columns are tracks, rows filter / chorus / delay / reverb of each track's engine.
  h.Down(kCtrl);
  h.Tap(Grid(groove::kTapeSends));
  h.Up(kCtrl);
  assert(h.App().Ui().CurrentPage() == groove::kTapeSends);
  h.Tap(Grid(8 + 2));  // T3 delay send, also selects T3
  assert(h.State().track == 2);
  for (int press = 0; press < 11; ++press) h.Tap(kAdd);
  assert(ParameterValue(app, 2, "fx.delay") == 11);
  h.Tap(kEnter);       // switches the send off ...
  assert(ParameterValue(app, 2, "fx.delay") == 0);
  h.Tap(kEnter);       // ... and back to where it was
  assert(ParameterValue(app, 2, "fx.delay") == 11);
  h.Tap(Grid(0));      // T1 filter: bipolar, low-pass below the centre
  h.Tap(kSub);
  assert(ParameterValue(app, 0, "fx.filter") == 63);
  h.Chord(kFn, kEnter);
  assert(ParameterValue(app, 0, "fx.filter") == 64);
  h.Tap(Grid(4 + 1));  // T2 chorus: a second press steps the mode
  const int chorus = ParameterValue(app, 1, "fx.chorus");
  h.Tap(Grid(4 + 1));
  assert(ParameterValue(app, 1, "fx.chorus") == (chorus + 1) % 4);
  h.Render();

  // FX: the master bus belongs to the project.
  h.Tap(kCtrl);
  assert(h.App().Ui().CurrentPage() == groove::kTapeFx);
  using synth::fx::MasterParam;
  const auto param = [](MasterParam value) { return static_cast<std::size_t>(value); };
  const int time = app.MasterFx(param(MasterParam::DelayTime));
  h.Tap(Grid(param(MasterParam::DelayTime)));
  h.Tap(kEnter);
  assert(app.MasterFx(param(MasterParam::DelayTime)) == (time + 1) % 12);
  h.Tap(Grid(param(MasterParam::EqLow)));
  for (int press = 0; press < 10; ++press) h.Tap(kAdd);
  assert(app.MasterFx(param(MasterParam::EqLow)) == 74);
  h.Chord(kFn, kEnter);
  assert(app.MasterFx(param(MasterParam::EqLow)) == 64);
  h.Tap(Grid(param(MasterParam::ReverbSize)));
  h.Tap(kSub);
  assert(app.MasterFx(param(MasterParam::ReverbSize)) == synth::fx::kMasterParams[param(MasterParam::ReverbSize)].defaultValue - 1);
  h.Tap(Grid(13));     // the last row is the bus display, not a parameter
  assert(h.State().fxFocus == param(MasterParam::ReverbSize));
  h.Render();
}

void TypeText(Harness& h, const std::string& text)
{
  for (const char c : text)
  {
    for (std::size_t row = 0; row < 4; ++row)
      for (std::size_t column = 1; column < 14; ++column)
        if (groove::LegendChar(row, column) == c) { h.Tap(Key{row, column}); goto typed; }
    assert(false && "character has no key");
  typed:;
  }
}

void TestProject(Harness& h)
{
  auto& app = h.App();
  GoToSetupPage(h, groove::kSetupProject);
  // SAVE AS: the name field starts with a suggestion; DEL erases, letters type.
  h.Tap(Grid(1));
  assert(h.Modal() != nullptr && h.Modal()->kind == ModalKind::TextEntry);
  h.Tap(kSub);  // the second key does not erase while typing
  assert(h.Modal()->text == "SONG");
  for (int index = 0; index < 8; ++index) h.Tap(kDel);
  assert(h.Modal()->text.empty());
  TypeText(h, "GROOVE 1");
  assert(h.Modal()->text == "GROOVE 1");
  h.Render();
  h.Tap(kEnter);
  assert(h.Modal() == nullptr);
  assert(h.State().projectName == "GROOVE 1" && !h.State().dirty);
  const auto projects = app.ListProjects();
  assert(std::find(projects.begin(), projects.end(), "GROOVE 1") != projects.end());

  // Keys pressed before the name field opened are released cleanly.
  assert(!h.State().gridDown[1]);

  // Saving again under the same name overwrites silently.
  h.Tap(Grid(0));
  assert(h.Modal() == nullptr && !h.State().dirty);

  // NEW with unsaved changes asks first; ESC keeps the session.
  h.Tap(Grid(4));
  h.Tap(kAdd);  // BPM + 1 marks the project dirty
  assert(h.State().dirty);
  h.Tap(Grid(3));
  assert(h.Modal() != nullptr && h.Modal()->kind == ModalKind::Confirm);
  h.Tap(kEsc);
  assert(h.Modal() == nullptr && h.State().projectName == "GROOVE 1");
  h.Tap(Grid(3));
  h.Tap(kEnter);
  assert(h.State().projectName.empty() && !h.State().dirty);
  assert(!app.TrackMuted(0));
  assert(app.EngineKind(1) == synth::SynthEngineKind::Juno);
  assert(!app.Sequencer().BuildUiState(0).steps[0].hasEvents);
  const auto reverbSize = static_cast<std::size_t>(synth::fx::MasterParam::ReverbSize);
  assert(app.MasterFx(reverbSize) == synth::fx::kMasterParams[reverbSize].defaultValue);
  assert(ParameterValue(app, 2, "fx.delay") == 0);

  // FILES: load the saved project back, mixer and engines included.
  h.Tap(Grid(2));
  assert(h.App().Ui().CurrentPage() == groove::kSetupFiles);
  assert(!h.State().projects.empty());
  h.Tap(kEnter);
  assert(h.State().projectName == "GROOVE 1");
  assert(app.TrackMuted(0) && app.TrackLevel(1) == 89);
  assert(app.EngineKind(1) == synth::SynthEngineKind::Dx7);
  assert(app.Sequencer().BuildUiState(0).steps[0].hasEvents);
  assert(app.Sequencer().PatternCount(0) == 3);
  // Master effects and the track sends come back with the project.
  assert(app.MasterFx(reverbSize) == synth::fx::kMasterParams[reverbSize].defaultValue - 1);
  assert(ParameterValue(app, 2, "fx.delay") == 11);

  // SETUP SYSTEM page (CTRL steps the pages): key lights and screen.
  while (h.App().Ui().CurrentPage() != groove::kSetupSystem) h.Tap(kCtrl);
  assert(std::string(h.App().Ui().SettingsPageShown()->tab) == "SYSTEM");
  h.Render();
  h.Tap(Grid(1));
  // KEY LED focused: the pads show the brightness ramp, dark to full.
  std::array<std::uint8_t, groove::kKeyCount> ramp{};
  app.Ui().ComputeLeds(ramp);
  assert(ramp[groove::KeyIndex(2, 1)] == 0 && ramp[groove::KeyIndex(2, 3)] == 7 && ramp[groove::KeyIndex(2, 8)] == 15);
  h.Tap(kSub);
  assert(app.LedBrightness() == 14);
  h.Tap(kAdd);
  h.Tap(Grid(2));
  h.Tap(kSub);
  assert(app.Backlight() == 175);
  h.Tap(kEsc);
  assert(h.State().scene != Scene::Setup);
}

void TestQuickSaveReleasesFn(Harness& h)
{
  // Fn + S on an unnamed project saves it at once as SONG nn: no name field. Letting go of FN
  // must not leave it stuck down. The name field is one step further: SAVE AS.
  GoToSetupPage(h, groove::kSetupProject);
  // FN + ADD / SUB take big steps: BPM by ten.
  h.Tap(Grid(4));
  const auto tempo = h.App().Sequencer().Tempo();
  h.Chord(kFn, kAdd);
  assert(h.App().Sequencer().Tempo() == tempo + 10);
  h.Chord(kFn, kSub);
  h.Tap(Grid(3));  // NEW
  if (h.Modal() != nullptr) h.Tap(kEnter);
  assert(h.State().projectName.empty());
  h.Down(kFn);
  h.Tap(Key{2, 3});  // S
  assert(h.Modal() == nullptr && h.State().projectName.rfind("SONG ", 0) == 0);
  h.Up(kFn);
  assert(!h.State().left[2].down && !h.App().Ui().FnOverlay());
  h.Tap(Grid(1));  // SAVE AS
  assert(h.Modal() != nullptr && h.Modal()->kind == ModalKind::TextEntry);
  // FN in text entry is the yellow keycap layer: , / move the cursor, ; . jump to the start / end,
  // FN + del deletes forward. No FN panel shows meanwhile.
  while (!h.Modal()->text.empty()) h.Tap(kDel);  // the field opens with a suggested name
  h.Tap(Key{2, 2});  // A
  h.Tap(Key{3, 7});  // B
  h.Down(kFn);
  h.Wait(300);
  assert(!h.App().Ui().FnOverlay());
  h.Tap(Key{3, 10});  // ,
  h.Up(kFn);
  h.Tap(Key{3, 5});  // C
  assert(h.Modal()->text == "ACB" && h.Modal()->textCursor == 2);
  h.Render();
  h.Chord(kFn, Key{2, 11});  // ;
  h.Chord(kFn, kDel);
  assert(h.Modal()->text == "CB" && h.Modal()->textCursor == 0);
  h.Chord(kFn, Key{3, 11});  // .
  h.Tap(kDel);
  assert(h.Modal()->text == "C");
  h.Tap(kEsc);
  assert(h.Modal() == nullptr);
}

// Every key is off (nothing there), half (there, not chosen) or full (chosen, held, sounding).
constexpr int kOff = groove::kLedOff, kHalf = groove::kLedHalf, kFull = groove::kLedFull;

void TestLeds(Harness& h)
{
  // A fresh project on T2 (Juno) in SEQ: no notes anywhere.
  GoToSetupPage(h, groove::kSetupProject);
  h.Tap(Grid(3));  // NEW
  if (h.Modal() != nullptr) h.Tap(kEnter);
  GoToScene(h, 2);
  while (h.App().Ui().CurrentPage() != 0) h.Tap(kCtrl);
  h.Chord(kTab, Grid(1));
  std::array<std::uint8_t, groove::kKeyCount> levels{};
  auto level = [&](Key key) {
    h.App().Ui().ComputeLeds(levels);
    return static_cast<int>(levels[groove::KeyIndex(key.row, key.column)]);
  };

  // Steps: empty off; with notes half; selected full.
  for (std::size_t step = 0; step < 16; ++step) assert(level(Grid(step)) == kOff);
  const std::uint8_t e3 = 52;
  h.App().Sequencer().PressStep(5, &e3, 1);
  h.App().Sequencer().ReleaseStep(5);
  assert(level(Grid(5)) == kHalf && level(Grid(4)) == kOff);
  h.Tap(Grid(5));
  assert(h.Seq().steps[5].held && level(Grid(5)) == kFull);
  // The selected step's note is full on the pads (E3 on the white row), the other keys half.
  assert(level(Key{3, 3}) == kFull && level(Key{3, 1}) == kHalf);
  h.Tap(Grid(5));
  assert(level(Grid(5)) == kHalf);

  // PIANO keyboard: the bottom row is white keys, all half; the row above is the black keys, half
  // where there is one and off in the gaps (before C#, between E and F, after A#).
  for (std::size_t column = 1; column <= 8; ++column) assert(level(Key{3, column}) == kHalf);
  for (std::size_t column = 1; column <= 8; ++column)
  {
    const bool gap = column == 1 || column == 4 || column == 8;
    assert(level(Key{2, column}) == (gap ? kOff : kHalf));
  }
  // A held key is full; letting go brings it back to half.
  h.Down(Key{3, 1});
  assert(level(Key{3, 1}) == kFull && level(Key{3, 2}) == kHalf);
  h.Up(Key{3, 1});
  assert(level(Key{3, 1}) == kHalf);

  // Edge keys: navigation half (CTRL too, SEQ has pages), ADD / SUB half, OK half (the record key:
  // full in STEP REC), PLAY half when stopped and full while playing.
  for (std::size_t row = 0; row < 4; ++row) assert(level(Key{row, 0}) == kHalf);
  assert(level(kAdd) == kHalf && level(kSub) == kHalf && level(kEnter) == kHalf && level(kPlay) == kHalf);
  h.Down(kTab);
  assert(level(kTab) == kFull);
  h.Up(kTab);
  h.Tap(kPlay);
  h.Wait(200);
  assert(level(kPlay) == kFull);
  // The playhead runs full over the steps.
  const auto seq = h.Seq();
  assert(seq.playing && level(Grid(seq.currentStep)) == kFull);
  h.Tap(kPlay);
  h.Wait(200);
  assert(level(kPlay) == kHalf);
  h.Chord(kTab, Grid(0));
}

// Publishes one LED frame the way os::system::RunFrame does and reads a key back.
int Led(Harness& h, Key key)
{
  Device::LED::BeginFrame();
  Device::LED::Clear();
  h.App().UpdateLeds();
  Device::LED::Present();
  return Device::LED::Brightness(key.row, key.column);
}

// The lights fade toward new levels frame by frame (falls take about half a second): publish
// frames until they arrive.
void Settle(Harness& h)
{
  for (int frame = 0; frame < 40; ++frame)
  {
    h.Frames();
    Led(h, kEsc);
  }
}

void TestLedMotion(Harness& h)
{
  GoToScene(h, 2);
  h.Wait(1200);
  // A press lights only its own key: no ring spills onto the neighbours.
  const int before = Led(h, Key{0, 11});
  h.Down(Grid(0));
  h.Wait(70);
  assert(Led(h, Grid(0)) == kFull && Led(h, Key{0, 11}) == before);
  h.Up(Grid(0));
  h.Tap(Grid(0));  // the release selected it: deselect again
  h.Wait(1200);
  // ESC map: the GRID keys come on corner to corner; each column is a scene.
  h.Down(kEsc);
  h.Wait(250);
  assert(h.App().Ui().EscOverlay());
  const int early = Led(h, Grid(15));
  h.Wait(250);
  // Column 4 (TAPE, another scene) is half once it has lit; the current scene (SEQ, column 3) is
  // full; the first column (SETUP) is half like any other scene; the pads go dark.
  assert(Led(h, Grid(15)) > early && Led(h, Grid(15)) == kHalf);
  assert(Led(h, Grid(2)) == kFull && Led(h, Grid(14)) == kFull);
  assert(Led(h, Grid(0)) == kHalf && Led(h, Key{1, 4}) == kOff);
  h.Up(kEsc);
  h.Wait(600);
  // CTRL map: one key per page of the current scene.
  h.Down(kCtrl);
  h.Wait(600);
  assert(h.App().Ui().CtrlOverlay());
  Settle(h);
  assert(Led(h, Grid(0)) == kFull && Led(h, Grid(1)) == kHalf && Led(h, Grid(2)) == kHalf && Led(h, Grid(3)) == kOff);
  h.Up(kCtrl);
  assert(h.App().Ui().CurrentPage() == 0);
  h.Wait(600);
  // Panic: every key flares, then the lights settle back.
  h.App().HandleHomeButtonEvent(true);
  h.App().HandleHomeButtonEvent(false);
  h.Frames();
  // The edge keys never go above the dimmest glow, even here.
  assert(Led(h, Key{1, 5}) >= 14 && Led(h, Key{3, 0}) <= groove::kLedEdgeMax && Led(h, Key{3, 0}) > 0);
  h.Wait(900);
  assert(Led(h, Key{1, 5}) < 14);
}

std::size_t PadSlot(Key key) { return key.row * groove::kPadColumns + (key.column - groove::kPadFirstColumn); }

void TestPlayFeatures(Harness& h)
{
  auto& app = h.App();
  const auto& keys = h.State().keyboard;
  GoToScene(h, 1);
  h.Chord(kTab, Grid(1));  // T2, a melodic track
  assert(h.State().track == 1 && app.EngineKind(1) != synth::SynthEngineKind::Drums);
  h.Wait(900);
  // The default keyboard is the piano: white keys on the lower row of each pair, C major.
  assert(keys.layout == groove::KeyboardLayout::Piano && keys.scale == 0 && keys.root == 0);
  const Key c3{3, 1}, e3{3, 3}, g3{3, 5}, cSharp{2, 2};
  int note = 0;
  assert(app.Ui().PadNoteFor(c3.row, c3.column, note) && note == 48);
  assert(app.Ui().PadNoteFor(cSharp.row, cSharp.column, note) && note == 49);
  // Key lights: every key that sounds is half, white or black.
  std::array<std::uint8_t, groove::kKeyCount> levels{};
  app.Ui().ComputeLeds(levels);
  assert(levels[groove::KeyIndex(c3.row, c3.column)] == kHalf && levels[groove::KeyIndex(e3.row, e3.column)] == kHalf);
  assert(levels[groove::KeyIndex(cSharp.row, cSharp.column)] == kHalf);

  // INSTR CHORD (the track's MIDI effect): TRIAD turns one key into C E G, and the chord tones light
  // on the pads. Each page starts focused on its first setting.
  GoToInstrumentPage(h, "CHORD");
  assert(app.Ui().InstrumentFocus() == 0);
  h.Tap(kAdd);
  const auto& shape = h.State().chords[1];
  assert(shape.chord == groove::ChordType::Triad);
  h.Down(c3);
  const auto& chord = h.State().padChord[PadSlot(c3)];
  assert(h.State().padChordCount[PadSlot(c3)] == 3 && chord[0] == 48 && chord[1] == 52 && chord[2] == 55);
  app.Ui().ComputeLeds(levels);
  assert(levels[groove::KeyIndex(c3.row, c3.column)] == kFull && levels[groove::KeyIndex(e3.row, e3.column)] == kFull &&
         levels[groove::KeyIndex(g3.row, g3.column)] == kFull);
  h.Render();
  h.Up(c3);
  // VOICING INV1 lifts the root an octave; Fn + ENTER restores the default.
  h.Tap(Grid(1));
  h.Tap(kAdd);
  h.Down(c3);
  assert(chord[0] == 52 && chord[1] == 55 && chord[2] == 60);
  h.Up(c3);
  h.Chord(kFn, kEnter);
  assert(shape.voicing == 0);
  // The chord belongs to T2: T3 still plays single notes, and comes back to its own page.
  h.Chord(kTab, Grid(2));
  h.Down(c3);
  assert(h.State().padChordCount[PadSlot(c3)] == 1 && h.State().chords[2].chord == groove::ChordType::Off);
  h.Up(c3);
  h.Chord(kTab, Grid(1));
  assert(std::string(app.Ui().CurrentInstrumentPage()->tab) == "CHORD");
  // SETUP KEYS LOCK (the keyboard of every track): MUTE silences keys outside the scale (and darkens
  // them); SNAP moves them down to it.
  GoToSetupPage(h, groove::kSetupKeys);
  h.Tap(Grid(5));
  h.Tap(kAdd);
  assert(keys.lock == groove::ScaleLock::Mute);
  h.Down(cSharp);
  assert(h.State().padNote[PadSlot(cSharp)] == 0xFF);
  h.Up(cSharp);
  app.Ui().ComputeLeds(levels);
  assert(levels[groove::KeyIndex(cSharp.row, cSharp.column)] == 0);
  h.Tap(kAdd);
  h.Down(cSharp);
  assert(h.State().padChordCount[PadSlot(cSharp)] == 3 && h.State().padChord[PadSlot(cSharp)][0] == 48);
  h.Up(cSharp);
  h.Chord(kFn, kEnter);
  assert(keys.lock == groove::ScaleLock::Off);
  // VELO: what every pad plays.
  h.Tap(Grid(4));
  h.Tap(kSub);
  assert(keys.velocity == 109);
  h.Chord(kFn, kEnter);
  assert(keys.velocity == 110);
  // Every SETUP page draws; ESC goes back to INSTR, still on CHORD.
  for (std::uint8_t page = 0; page < groove::kSetupPageCount; ++page)
  {
    while (app.Ui().CurrentPage() != page) h.Tap(kCtrl);
    h.Render();
  }
  h.Tap(kEsc);
  assert(h.State().scene == Scene::Instrument && std::string(app.Ui().CurrentInstrumentPage()->tab) == "CHORD");
  // CHORD back to OFF (a second press on the focused CHORD steps it); then ARP, T2's own: UP.
  assert(app.Ui().InstrumentFocus() == 1);  // the page kept its focus: VOICING
  h.Tap(Grid(0));
  assert(app.Ui().InstrumentFocus() == 0 && shape.chord == groove::ChordType::Triad);
  h.Tap(Grid(0));
  assert(shape.chord != groove::ChordType::Triad);
  while (shape.chord != groove::ChordType::Off) h.Tap(kSub);
  h.Tap(kCtrl);
  assert(std::string(app.Ui().CurrentInstrumentPage()->tab) == "ARP");
  h.Render();
  h.Tap(kAdd);  // ARP UP
  assert(app.Sequencer().GetArpSettings(1).mode == synth::ArpMode::Up && app.Sequencer().GetArpSettings(2).mode == synth::ArpMode::Off);
  // ARP: a held key feeds T2's arpeggiator; letting go stops it (latch is off). The menu draws over it.
  h.Down(c3);
  assert(app.Sequencer().ArpActive(1));
  h.Wait(300);
  h.Down(kFn);
  h.Wait(300);
  h.Render();
  h.Up(kFn);
  h.Up(c3);
  h.Wait(50);
  assert(!app.Sequencer().ArpActive());
  h.Tap(kSub);
  assert(app.Sequencer().GetArpSettings(1).mode == synth::ArpMode::Off);

  // FN + TAB = previous track, FN + ESC = all stop; neither does its plain job.
  h.Chord(kFn, kTab);
  assert(h.State().track == 0 && h.State().scene == Scene::Instrument);
  const std::uint32_t panic = h.State().panicAt;
  h.Chord(kFn, kEsc);
  assert(h.State().panicAt != panic && h.State().scene == Scene::Instrument);
  h.Wait(900);
  // After a track change the left column says which track: T2 = the second key.
  h.Tap(kTab);
  h.Wait(60);
  assert(Led(h, kTab) == groove::kLedEdgeMax && Led(h, kEsc) == 0 && Led(h, kCtrl) == 0);
  h.Wait(900);
  // Holding FN lights only the keys that have an FN function: the menu items on the GRID, their
  // letters and the quick keys on the pads, ADD / SUB (big steps).
  h.Down(kFn);
  h.Wait(300);
  app.Ui().ComputeLeds(levels);
  assert(levels[groove::KeyIndex(0, 13)] == kHalf && levels[groove::KeyIndex(1, 13)] == kHalf);  // big steps
  assert(levels[groove::KeyIndex(0, 1)] == kOff && levels[groove::KeyIndex(0, 3)] == kOff);      // 1, 3: nothing
  assert(levels[groove::KeyIndex(0, 6)] == kHalf && levels[groove::KeyIndex(1, 6)] == kHalf);  // quick BPM + / -
  assert(levels[groove::KeyIndex(Grid(0).row, Grid(0).column)] == kHalf);                         // PRESETS
  assert(levels[groove::KeyIndex(Grid(15).row, Grid(15).column)] == kHalf);                       // SETUP
  assert(levels[groove::KeyIndex(Grid(8).row, Grid(8).column)] == kOff);                          // nothing there
  assert(levels[groove::KeyIndex(3, 5)] == kOff);                                                 // C: no COPY in INSTR
  assert(levels[groove::KeyIndex(3, 13)] == kHalf && levels[groove::KeyIndex(0, 0)] == kHalf);  // REC, STOP
  assert(levels[groove::KeyIndex(1, 2)] == kOff);                                                 // W: nothing
  assert(levels[groove::KeyIndex(2, 3)] == kHalf);                                                // S: SAVE
  h.Up(kFn);
}

void TestFnQuick(Harness& h)
{
  auto& app = h.App();
  // FN + 6 / Y: BPM up / down, FN + 7 / U: volume, FN + 8 / I: octave, from any FN page, with a
  // pop-up showing the value. Holding repeats.
  const auto bpm = app.Sequencer().Tempo();
  const auto volume = app.MasterVolume();
  const int octave = h.State().keyboard.octave;
  h.Down(kFn);
  h.Tap(Key{0, 6});
  assert(app.Sequencer().Tempo() == bpm + 1 && h.State().hud.label == "BPM");
  h.Tap(Key{1, 6});
  assert(app.Sequencer().Tempo() == bpm);
  h.Tap(Key{1, 7});
  assert(app.MasterVolume() == std::max(0, volume - 2) && h.State().hud.label == "VOLUME");
  h.Tap(Key{0, 7});
  assert(app.MasterVolume() == volume);
  h.Tap(Key{0, 8});
  assert(h.State().keyboard.octave == std::min(3, octave + 1) && h.State().hud.label == "OCTAVE");
  h.Tap(Key{1, 8});
  assert(h.State().keyboard.octave == octave);
  // Held: repeats until the key is let go, and the quick key is lit full meanwhile.
  h.Down(Key{0, 6});
  h.Wait(800);
  std::array<std::uint8_t, groove::kKeyCount> levels{};
  app.Ui().ComputeLeds(levels);
  assert(levels[groove::KeyIndex(0, 6)] == kFull && levels[groove::KeyIndex(0, 13)] == kHalf);
  h.Up(Key{0, 6});
  const auto raised = app.Sequencer().Tempo();
  assert(raised >= bpm + 4);
  h.Wait(300);
  assert(app.Sequencer().Tempo() == raised);
  while (app.Sequencer().Tempo() > bpm) h.Tap(Key{1, 6});
  h.Up(kFn);
  // The pads play again after FN.
  assert(!h.App().Ui().FnOverlay());
}

void TestRecording(Harness& h)
{
  auto& app = h.App();
  auto& sequencer = app.Sequencer();
  GoToScene(h, 2);
  while (app.Ui().CurrentPage() != 0) h.Tap(kCtrl);
  h.Chord(kTab, Grid(1));  // T2 (Juno)
  if (sequencer.Playing()) h.Tap(kPlay);
  // A fresh pattern to write into.
  const std::size_t patterns = sequencer.PatternCount(1);
  h.Down(kCtrl);
  h.Wait(300);
  h.Up(kCtrl);
  h.Chord(kCtrl, Grid(1));  // PATTERN page
  h.Tap(Grid(patterns));    // new pattern
  h.Chord(kCtrl, Grid(0));
  assert(sequencer.ActivePattern(1) == patterns && !h.Seq().populatedStepCount);

  // A tap on OK switches the note page between PIANO ROLL and STEP REC.
  if (app.Ui().View() != groove::SeqView::Roll) h.Tap(kEnter);
  assert(app.Ui().View() == groove::SeqView::Roll && app.Ui().PageTab(Scene::Sequence, 0) == "ROLL");
  h.Render();
  h.Tap(kEnter);
  assert(app.Ui().View() == groove::SeqView::Step && h.State().hud.value == "STEP REC" && app.Ui().PageTab(Scene::Sequence, 0) == "STEP");
  groove::LedFrame frame{};
  app.Ui().ComputeLeds(frame);
  assert(frame[groove::KeyIndex(kEnter.row, kEnter.column)].level == kFull);
  assert(frame[groove::KeyIndex(Grid(0).row, Grid(0).column)].effect == groove::LedEffect::Blink);  // the cursor

  // STEP REC: held pads become the cursor step's notes; letting go moves the cursor on.
  const Key c3{3, 1}, e3{3, 3}, g3{3, 5};
  h.Tap(c3);
  std::array<std::uint8_t, 8> notes{};
  assert(sequencer.StepNotes(0, notes) == 1 && notes[0] == 48 && h.State().record.cursor == 1);
  h.Down(c3);
  h.Down(e3);
  h.Up(c3);
  assert(h.State().record.cursor == 1);  // still a pad down: same step
  h.Up(e3);
  assert(sequencer.StepNotes(1, notes) == 2 && notes[0] == 48 && notes[1] == 52 && h.State().record.cursor == 2);
  h.Tap(kAdd);  // a rest
  assert(h.State().record.cursor == 3);
  h.Tap(kSub);
  h.Tap(Grid(8));  // the GRID moves the cursor
  assert(h.State().record.cursor == 8);
  h.Tap(g3);
  assert(sequencer.StepNotes(8, notes) == 1 && notes[0] == 55 && h.State().record.cursor == 9);
  h.Render();
  h.Tap(kEnter);
  assert(app.Ui().View() == groove::SeqView::Roll);
  h.Render();

  // Capture: notes played without recording are kept as takes, split by pauses (the STEP REC
  // writes above are the take before).
  h.Wait(3000);
  const std::size_t takes = app.Ui().TakeCount();
  for (const Key pad : {c3, e3, g3, e3})
  {
    h.Down(pad);
    h.Wait(100);
    h.Up(pad);
    h.Wait(150);
  }
  assert(app.Ui().TakeCount() == takes + 1);
  const groove::CaptureTake* take = app.Ui().TakeAt(0);
  assert(take != nullptr && take->count == 4 && take->track == 1 && !take->synced);
  h.Wait(3000);
  h.Tap(c3);
  assert(app.Ui().TakeCount() == takes + 2 && app.Ui().TakeAt(0)->count == 1 && app.Ui().TakeAt(1)->count == 4);

  // Hold OK: the RECORD panel, on the newest take. The previous take goes into a new pattern.
  h.Down(kEnter);
  h.Wait(600);
  assert(app.Ui().RecordOpen() && h.State().record.focus == groove::kRecordFirstTake);
  h.Up(kEnter);
  assert(app.Ui().RecordOpen());
  h.Render();
  h.Tap(Grid(groove::kRecordFirstTake + 1));
  std::array<groove::Controller::PlacedNote, groove::kCaptureNotes> placed{};
  assert(app.Ui().TakeBarNotes(*app.Ui().TakeAt(1), 0, placed) == 4 && placed[0].step == 0 && placed[1].step == 2);
  h.Render();
  const std::size_t before = sequencer.PatternCount(1);
  h.Tap(kEnter);
  assert(!app.Ui().RecordOpen() && sequencer.PatternCount(1) == before + 1 && sequencer.ActivePattern(1) == before);
  assert(h.State().hud.label.rfind("TAKE", 0) == 0 && h.Seq().populatedStepCount == 4);
  assert(sequencer.StepNotes(0, notes) == 1 && notes[0] == 48);
  // FN + OK merges the newest take into the pattern that is playing.
  h.Down(kEnter);
  h.Wait(600);
  h.Up(kEnter);
  h.Chord(kFn, kEnter);
  assert(sequencer.PatternCount(1) == before + 1 && h.State().hud.value.rfind("MERGED", 0) == 0);
  // REC from the panel arms recording like FN + PLAY.
  h.Down(kEnter);
  h.Wait(600);
  h.Up(kEnter);
  h.Tap(Grid(groove::kRecordRec));
  assert(sequencer.RecordArmed());
  h.Tap(Grid(groove::kRecordRec));
  assert(!sequencer.RecordArmed());
  h.Tap(kEsc);
  assert(!app.Ui().RecordOpen());
}

void TestSession(Harness& h)
{
  auto& app = h.App();
  auto& sequencer = app.Sequencer();
  GoToScene(h, 3);
  while (h.App().Ui().CurrentPage() != groove::kTapeSession) h.Tap(kCtrl);
  if (sequencer.Playing()) h.Tap(kPlay);
  const std::size_t patterns = sequencer.PatternCount(1);
  assert(patterns < 4);
  // Columns are tracks, rows pattern slots. The slot after the last pattern creates one, and while
  // stopped the track switches to it at once.
  h.Tap(Grid(patterns * 4 + 1));
  assert(sequencer.PatternCount(1) == patterns + 1 && sequencer.ActivePattern(1) == patterns);
  assert(h.State().track == 1 && h.State().sessionTrack == 1 && h.State().sessionPattern == patterns);
  h.Wait(400);
  std::array<std::uint8_t, groove::kKeyCount> levels{};
  app.Ui().ComputeLeds(levels);
  const auto grid = [](std::size_t index) { return groove::KeyIndex(index / 4, 9 + index % 4); };
  assert(levels[grid(patterns * 4 + 1)] == kFull);                 // the track's current pattern
  assert(levels[grid(0 * 4 + 1)] == kHalf);                         // another pattern of T2
  assert(levels[grid((patterns + 1) * 4 + 1)] == kOff);             // "+": no pattern there yet
  assert(levels[grid(3 * 4 + 0)] == kOff || sequencer.PatternCount(0) > 3);  // nothing there
  // While playing a launch waits for the loop end: the cell is queued and blinks.
  h.Render();
  h.Tap(kPlay);
  h.Tap(Grid(1));
  auto ui = sequencer.BuildUiState(1);
  assert(ui.tracks[1].queuedPattern == 0 && h.State().hud.label == "T2 NEXT");
  groove::LedFrame frame{};
  app.Ui().ComputeLeds(frame);
  assert(frame[grid(1)].level == kFull && frame[grid(1)].effect == groove::LedEffect::Blink);
  h.Render();
  // Fn + ENTER captures what plays (or is queued) into the SONG as a new row.
  const std::size_t rows = sequencer.SongLength();
  h.Chord(kFn, kEnter);
  assert(sequencer.SongLength() == rows + 1 && sequencer.SongRowAt(rows).patterns[1] == 0);
  assert(h.State().hud.label == "CAPTURED");
  h.Tap(kPlay);
  // Launching from the session leaves SONG mode.
  sequencer.SetSongMode(true);
  h.Tap(Grid(patterns * 4 + 1));
  assert(!sequencer.SongMode());
  // ENTER launches the cursor's row on every track that has that pattern.
  h.Tap(kEnter);
  assert(sequencer.ActivePattern(1) == patterns && h.State().hud.label == "ROW");
  // ADD / SUB walk the patterns and scroll the grid.
  for (int press = 0; press < 8; ++press) h.Tap(kAdd);
  assert(h.State().sessionPattern == std::min<std::size_t>(patterns + 8, 15) && h.State().sessionScroll + 3 == h.State().sessionPattern);
  for (int press = 0; press < 16; ++press) h.Tap(kSub);
  assert(h.State().sessionPattern == 0 && h.State().sessionScroll == 0);
  h.Render();
}

void TestSelectMode(Harness& h)
{
  // ROLL with SELECT ONE (the default): a GRID tap walks to that one step, its notes are full on the
  // pads, and the same step again lets go so the pads play.
  GoToScene(h, 2);
  while (h.App().Ui().CurrentPage() != 0) h.Tap(kCtrl);
  h.Chord(kTab, Grid(1));
  assert(!h.State().keyboard.multiSelect);
  // Steps 1 and 5 hold just the C under the bottom-left pad.
  const Key c{3, 1};
  int noteC = 0;
  assert(groove::PadNote(h.State().keyboard, h.App().EngineKind(h.State().track), c.row, c.column, noteC));
  const std::uint8_t cNote = static_cast<std::uint8_t>(noteC);
  for (const std::size_t step : {std::size_t{0}, std::size_t{4}})
    assert(h.App().Sequencer().SetStepNotes(h.State().track, step, &cNote, 1, 110));
  h.Tap(Grid(0));
  assert(h.Seq().steps[0].held && h.Seq().heldStepCount == 1);
  h.Tap(Grid(4));
  assert(!h.Seq().steps[0].held && h.Seq().steps[4].held && h.Seq().heldStepCount == 1);
  std::array<std::uint8_t, groove::kKeyCount> leds{};
  h.App().Ui().ComputeLeds(leds);
  assert(leds[groove::KeyIndex(c.row, c.column)] == groove::kLedFull && leds[groove::KeyIndex(3, 2)] == groove::kLedHalf);
  assert(leds[groove::KeyIndex(Grid(4).row, Grid(4).column)] == groove::kLedFull);
  h.Tap(Grid(4));
  assert(!h.Seq().hasHeldSteps);

  // Playing with nothing selected: the notes of the step under the playhead light up (playback
  // starts on step 1, which holds C).
  h.Tap(kPlay);
  h.Wait(20);
  assert(h.Seq().playing && h.Seq().currentStep == 0);
  h.App().Ui().ComputeLeds(leds);
  assert(leds[groove::KeyIndex(c.row, c.column)] == groove::kLedFull && leds[groove::KeyIndex(3, 2)] == groove::kLedHalf);
  h.Tap(kPlay);
  h.Wait(100);
  h.App().Ui().ComputeLeds(leds);
  assert(leds[groove::KeyIndex(c.row, c.column)] == groove::kLedHalf);

  // SELECT MULTI (FN panel, KEYS page): taps add steps to the selection.
  auto settings = h.App().Ui().Settings();
  settings.keyboard.multiSelect = true;
  h.App().Ui().Restore(settings, h.State().projectName, h.State().dirty);
  h.Tap(Grid(0));
  h.Tap(Grid(4));
  assert(h.Seq().steps[0].held && h.Seq().steps[4].held && h.Seq().heldStepCount == 2);
  h.App().Sequencer().ClearSelectedSteps();
  settings.keyboard.multiSelect = false;
  h.App().Ui().Restore(settings, h.State().projectName, h.State().dirty);
}

void TestRenderSmoke(Harness& h)
{
  // Every scene and page renders; the left strips and header are always drawn.
  for (int scene = 1; scene <= 3; ++scene)
  {
    GoToScene(h, scene);
    const std::size_t pages = h.App().Ui().PageCount(h.State().scene);
    for (std::size_t page = 0; page < pages; ++page)
    {
      h.Wait(200);
      h.Render();
      const auto frame = Device::Display::CurrentFrameBuffer();
      bool content = false;
      for (int y = groove::theme::kContentY; y < groove::theme::kContentBottom && !content; ++y)
        for (int x = groove::theme::kContentX; x < groove::theme::kContentRight; ++x)
          if (frame.pixels[static_cast<std::size_t>(y) * frame.width + x] != 0) { content = true; break; }
      assert(content);
      h.Tap(kCtrl);
    }
  }
  h.Down(kEsc);
  h.Wait(300);
  h.Render();
  h.Up(kEsc);

  // Informational: render cost of a steady frame versus a scene-slide frame (both scenes drawn).
  using Clock = std::chrono::steady_clock;
  auto timeFrames = [&](int frames) {
    const auto start = Clock::now();
    for (int index = 0; index < frames; ++index) h.Render();
    return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start).count() / frames;
  };
  GoToScene(h, 2);
  h.Wait(400);
  const auto steady = timeFrames(200);
  h.Chord(kEsc, Digit(3));
  gNow -= 32;  // stay inside the 260 ms slide while timing it
  const auto sliding = timeFrames(200);
  std::cout << "render us: steady " << steady << ", scene slide " << sliding << "\n";
}

}  // namespace

int main()
{
  // Projects and samples go to a scratch card under the test's working directory.
  const auto root = std::filesystem::temp_directory_path() / ("cardenza-groove-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  std::filesystem::current_path(root);

  assert(os::system::Begin());
  auto* app = dynamic_cast<apps::GrooveApp*>(os::system::ActiveApplication());
  assert(app != nullptr);
  app->UseClock(&FakeClock);
  Harness h(*app);
  h.Frames(4);

  TestBoot(h);
  TestNavigation(h);
  TestStepEditing(h);
  TestPatterns(h);
  TestTransport(h);
  TestInstrument(h);
  TestTape(h);
  TestMix(h);
  TestSendsAndFx(h);
  TestProject(h);
  TestQuickSaveReleasesFn(h);
  TestLeds(h);
  TestLedMotion(h);
  TestPlayFeatures(h);
  TestFnQuick(h);
  TestSession(h);
  TestRecording(h);
  TestSelectMode(h);
  TestRenderSmoke(h);

  std::filesystem::current_path(std::filesystem::temp_directory_path());
  std::filesystem::remove_all(root);
  std::cout << "Groovebox UI workflows passed\n";
  return 0;
}
