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

void GoToScene(Harness& h, int number)
{
  h.Chord(kEsc, Grid(static_cast<std::size_t>(number - 1)));
  h.Wait(200);
  assert(static_cast<int>(h.State().scene) == number - 1);
}

void TestBoot(Harness& h)
{
  assert(h.State().scene == Scene::Sequence);
  assert(h.App().Ui().CurrentPage() == 0);
  assert(h.State().track == 0);
  for (std::size_t track = 0; track < synth::kSynthTrackCount; ++track)
    assert(h.App().EngineKind(track) == apps::GrooveApp::kDefaultKinds[track]);
  assert(h.App().EngineKind(0) == synth::SynthEngineKind::Drums);
}

void TestNavigation(Harness& h)
{
  // Holding ESC shows the scene map: GRID columns are the scenes (any row), the pads stay silent,
  // and the release does not also go back.
  h.Hold(kEsc, 300);
  assert(h.App().Ui().EscOverlay());
  h.Tap(Key{1, 3});
  assert(h.State().padNote[1 * 8 + 2] == 0xFF);
  h.Tap(Grid(3));  // the fourth column has no scene
  assert(h.State().scene == Scene::Sequence);
  h.Tap(Grid(8));  // row 2, column 0
  h.Up(kEsc);
  assert(h.State().scene == Scene::Instrument);
  assert(!h.App().Ui().EscOverlay());
  // Tapping ESC returns to the previous scene and back again.
  h.Tap(kEsc);
  assert(h.State().scene == Scene::Sequence);
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
  // to any page and the page stays after the release; CTRL + ADD / SUB step; Fn + CTRL goes back.
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
  h.Chord(kFn, kCtrl);
  assert(h.App().Ui().CurrentPage() == 1);
  assert(!h.App().Ui().CtrlOverlay());
  h.Chord(kFn, kCtrl);
  assert(h.App().Ui().CurrentPage() == 0);
  // The Fn panel appears only after the hold threshold.
  h.Down(kFn);
  assert(!h.App().Ui().FnOverlay());
  h.Wait(300);
  assert(h.App().Ui().FnOverlay());
  h.Up(kFn);

  // FN + GRID functions: tempo, sound, mute this track.
  const auto bpm = h.App().Sequencer().Tempo();
  h.Chord(kFn, Grid(1));
  assert(h.App().Sequencer().Tempo() == bpm + 1);
  h.Chord(kFn, Grid(0));
  assert(h.App().Sequencer().Tempo() == bpm);
  const std::string preset = h.App().Engine(0)->CurrentPresetName();
  h.Tap(kTab);
  const std::string junoPreset = h.App().Engine(1)->CurrentPresetName();
  h.Chord(kFn, Grid(5));
  assert(h.App().Engine(1)->CurrentPresetName() != junoPreset);
  h.Chord(kFn, Grid(4));
  assert(h.App().Engine(1)->CurrentPresetName() == junoPreset);
  h.Chord(kFn, Grid(12));
  assert(h.App().TrackMuted(1));
  h.Chord(kFn, Grid(12));
  assert(!h.App().TrackMuted(1));
  h.Chord(kTab, Grid(0));
  assert(h.State().track == 0 && h.App().Engine(0)->CurrentPresetName() == preset);
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

  // DCO page: GRID focuses PWM, ADD raises it, Fn + ENTER restores the default.
  h.Tap(kCtrl);
  assert(std::string(h.App().Ui().CurrentInstrumentPage()->tab) == "DCO");
  h.Tap(Grid(3));
  const int pwm = ParameterValue(app, 1, "dco.pwm");
  h.Tap(kAdd);
  assert(ParameterValue(app, 1, "dco.pwm") == std::min(127, pwm + 1));
  h.Chord(kFn, kAdd);
  assert(ParameterValue(app, 1, "dco.pwm") == std::min(127, pwm + 11));
  h.Chord(kFn, kEnter);
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
  h.Tap(Grid(5));
  h.Up(kCtrl);
  assert(std::string(h.App().Ui().CurrentInstrumentPage()->tab) == "FX");
  h.Tap(Grid(5));
  h.Tap(kAdd);
  h.Chord(kFn, kAdd);
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

  // KEYS page edits the pad layout.
  while (std::string(h.App().Ui().CurrentInstrumentPage()->tab) != "KEYS") h.Tap(kCtrl);
  h.Tap(Grid(3));
  h.Tap(kAdd);
  assert(h.State().keyboard.octave == 1);
  h.Chord(kFn, Key{0, 11});  // Fn + '-'
  assert(h.State().keyboard.octave == 0);

  // ENGINE picker: switch track 2 from Juno to DX7.
  while (h.App().Ui().CurrentPage() != 0) h.Tap(kCtrl);
  h.Tap(Grid(0));
  assert(h.Modal() != nullptr && h.Modal()->kind == ModalKind::EnginePicker);
  h.Render();
  h.Tap(Grid(0));
  assert(app.EngineKind(1) == synth::SynthEngineKind::Dx7);
  assert(h.Modal() == nullptr);
  // The preset browser filters as you type and loads with ENTER.
  h.Tap(Grid(1));
  assert(h.Modal() != nullptr && h.Modal()->kind == ModalKind::PresetBrowser);
  const std::size_t all = h.Modal()->items.size();
  h.Tap(Key{2, 2});  // A
  assert(h.Modal()->items.size() < all);
  h.Render();
  h.Tap(kEsc);
  assert(h.Modal() != nullptr && h.Modal()->search.empty());
  h.Tap(kAdd);
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
  // Fn + ADD lengthens the row.
  h.Chord(kFn, kAdd);
  assert(sequencer.SongRowAt(1).repeats == 2);
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
  h.Chord(kFn, kSub);
  assert(app.TrackLevel(1) == 89);
  h.Tap(kEnter);    // master focus
  const std::uint8_t master = app.MasterVolume();
  h.Tap(kSub);
  assert(app.MasterVolume() == master - 1);
  h.Tap(kEnter);
  h.Tap(Grid(9));
  assert(!app.TrackSoloed(1) && app.TrackAudible(2));
  // Fn + number mutes from any scene.
  h.Chord(kFn, Digit(1));
  assert(!app.TrackMuted(0));
  h.Chord(kFn, Digit(1));
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
  h.Chord(kFn, kAdd);
  h.Tap(kAdd);
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
  h.Chord(kFn, kAdd);
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
  GoToScene(h, 3);
  while (h.App().Ui().CurrentPage() != groove::kTapeProject) h.Tap(kCtrl);
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
  assert(h.App().Ui().CurrentPage() == groove::kTapeFiles);
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

  // SYSTEM page.
  h.Tap(kCtrl);
  h.Tap(Grid(0));
  h.Tap(kSub);
  assert(app.LedBrightness() == 14);
  h.Tap(kAdd);
  h.Tap(Grid(1));
  h.Tap(kSub);
  assert(app.Backlight() == 175);
}

void TestQuickSaveReleasesFn(Harness& h)
{
  // Fn + S on an unnamed project opens the name field while FN is still held; letting go of FN
  // must not leave it stuck down.
  GoToScene(h, 3);
  while (h.App().Ui().CurrentPage() != groove::kTapeProject) h.Tap(kCtrl);
  h.Tap(Grid(3));  // NEW
  if (h.Modal() != nullptr) h.Tap(kEnter);
  assert(h.State().projectName.empty());
  h.Down(kFn);
  h.Tap(Key{2, 3});  // S
  assert(h.Modal() != nullptr && h.Modal()->kind == ModalKind::TextEntry);
  h.Up(kFn);
  assert(!h.State().left[2].down && !h.App().Ui().FnOverlay());
  h.Tap(kEsc);
  assert(h.Modal() == nullptr);
}

void TestLeds(Harness& h)
{
  std::array<std::uint8_t, groove::kKeyCount> levels{};
  h.App().Ui().ComputeLeds(levels);
  // Navigation keys: steady 3; CTRL only when the screen has pages.
  for (std::size_t row = 0; row < 3; ++row) assert(levels[groove::KeyIndex(row, 0)] == 3);
  h.Down(kTab);
  h.App().Ui().ComputeLeds(levels);
  assert(levels[groove::KeyIndex(1, 0)] == 15);
  h.Up(kTab);
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

void TestLedMotion(Harness& h)
{
  GoToScene(h, 2);
  h.Wait(1200);
  // A key press throws a ring of light: a key two columns away lights up shortly after.
  const int before = Led(h, Key{0, 11});
  h.Tap(Grid(0));
  h.Wait(70);
  assert(Led(h, Key{0, 11}) > before);
  h.Tap(Grid(0));  // deselect again
  h.Wait(1200);
  // ESC map: the GRID keys come on corner to corner; each column is a scene.
  h.Down(kEsc);
  h.Wait(250);
  assert(h.App().Ui().EscOverlay());
  const int early = Led(h, Grid(14));
  h.Wait(250);
  // Column 2 (TAPE, another scene) breathes once it has lit; the current scene (SEQ, column 1)
  // shimmers at focus level; the fourth column has no scene and stays dark; the pads go dark.
  assert(Led(h, Grid(14)) > early && Led(h, Grid(14)) >= 3);
  assert(Led(h, Grid(1)) >= 12 && Led(h, Grid(13)) >= 12);
  assert(Led(h, Grid(3)) == 0 && Led(h, Key{1, 4}) == 0);
  h.Up(kEsc);
  h.Wait(600);
  // CTRL map: one key per page of the current scene.
  h.Down(kCtrl);
  h.Wait(600);
  assert(h.App().Ui().CtrlOverlay());
  assert(Led(h, Grid(0)) >= 12 && Led(h, Grid(1)) >= 3 && Led(h, Grid(2)) >= 3 && Led(h, Grid(3)) == 0);
  h.Up(kCtrl);
  assert(h.App().Ui().CurrentPage() == 0);
  h.Wait(600);
  // Panic: every key flares, then the lights settle back.
  h.App().HandleHomeButtonEvent(true);
  h.App().HandleHomeButtonEvent(false);
  h.Frames();
  assert(Led(h, Key{1, 5}) >= 14 && Led(h, Key{3, 0}) >= 14);
  h.Wait(900);
  assert(Led(h, Key{1, 5}) < 14);
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
  TestRenderSmoke(h);

  std::filesystem::current_path(std::filesystem::temp_directory_path());
  std::filesystem::remove_all(root);
  std::cout << "Groovebox UI workflows passed\n";
  return 0;
}
