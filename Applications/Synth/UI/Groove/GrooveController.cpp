#include "Applications/Synth/UI/Groove/GrooveController.h"

#include <algorithm>
#include <cstdlib>

namespace groove {

namespace {

constexpr std::uint8_t kNoNote = 0xFF;

constexpr std::size_t LeftIndex(KeyRole role)
{
  switch (role)
  {
    case KeyRole::Esc: return 0;
    case KeyRole::Tab: return 1;
    case KeyRole::Fn: return 2;
    case KeyRole::Ctrl:
    default: return 3;
  }
}

constexpr std::size_t PadIndex(std::size_t row, std::size_t column)
{
  return row * kPadColumns + (column - kPadFirstColumn);
}

}  // namespace

Controller::Controller(Host& host) : host_(host)
{
  state_.padNote.fill(kNoNote);
  state_.padTrack.fill(0);
  state_.instrumentFocus.fill(0);
  state_.page.fill(0);
}

void Controller::Restore(const ProjectUiSettings& settings, const std::string& projectName)
{
  ReleaseAllPads();
  state_.modals.clear();
  state_.track = std::min(settings.activeTrack, kTrackCount - 1);
  state_.keyboard = settings.keyboard;
  state_.projectName = projectName;
  state_.dirty = false;
  state_.sessionAt = state_.now == 0 ? 1 : state_.now;
  state_.clipboardValid = false;
  state_.songRow = 0;
  state_.songTrack = 0;
  state_.songScroll = 0;
  state_.songClipboardValid = false;
  state_.instrumentFocus.fill(0);
  state_.trackInstrumentPage.fill(0);
  for (auto& focus : state_.trackInstrumentFocus) focus.fill(0);
  state_.page[static_cast<std::size_t>(Scene::Instrument)] = 0;
  host_.Sequencer().SetActiveTrack(state_.track);
  host_.Sequencer().ClearSelectedSteps();
  ClampPages();
}

ProjectUiSettings Controller::Settings() const
{
  ProjectUiSettings settings;
  settings.activeTrack = state_.track;
  settings.keyboard = state_.keyboard;
  return settings;
}

bool Controller::EscOverlay() const
{
  const auto& key = state_.left[0];
  return key.down && (key.used || state_.now - key.pressedAt >= kHoldMs);
}

bool Controller::TabOverlay() const
{
  const auto& key = state_.left[1];
  return key.down && (key.used || state_.now - key.pressedAt >= kHoldMs);
}

bool Controller::FnOverlay() const
{
  const auto& key = state_.left[2];
  return key.down && state_.now - key.pressedAt >= kHoldMs;
}

bool Controller::CtrlOverlay() const
{
  const auto& key = state_.left[3];
  return key.down && !key.suppressed && TopModal() == nullptr && !state_.left[2].down &&
         (key.used || state_.now - key.pressedAt >= kHoldMs);
}

synth::ISynthEngine* Controller::ActiveEngine()
{
  return host_.Engine(state_.track);
}

const synth::ISynthEngine* Controller::ActiveEngine() const
{
  return host_.Engine(state_.track);
}

synth::SynthEngineKind Controller::ActiveKind() const
{
  return host_.EngineKind(state_.track);
}

InstrumentPageList Controller::InstrumentPages() const
{
  return InstrumentPagesFor(ActiveKind());
}

const InstrumentPage* Controller::CurrentInstrumentPage() const
{
  const InstrumentPageList list = InstrumentPages();
  const std::size_t page = std::min<std::size_t>(state_.page[static_cast<std::size_t>(Scene::Instrument)], list.count - 1);
  return list.pages + page;
}

std::uint8_t Controller::InstrumentFocus() const
{
  const std::size_t page = std::min<std::size_t>(state_.page[static_cast<std::size_t>(Scene::Instrument)], state_.instrumentFocus.size() - 1);
  return state_.instrumentFocus[page];
}

std::size_t Controller::PageCount(Scene scene) const
{
  switch (scene)
  {
    case Scene::Instrument: return InstrumentPages().count;
    case Scene::Sequence: return 3;
    case Scene::Tape:
    default: return kTapePageCount;
  }
}

std::string Controller::PageName(Scene scene, std::size_t page) const
{
  static constexpr std::array<const char*, kTapePageCount> kTape{{"SONG", "LEVELS", "PER TRK", "MASTER", "SAVE", "LOAD", "DEVICE"}};
  static constexpr std::array<const char*, 3> kSequence{{"EDIT", "SLOTS", "SETUP"}};
  switch (scene)
  {
    case Scene::Instrument:
    {
      const InstrumentPageList list = InstrumentPages();
      return page < list.count ? list.pages[page].name : "";
    }
    case Scene::Sequence: return page < kSequence.size() ? kSequence[page] : "";
    case Scene::Tape:
    default: return page < kTape.size() ? kTape[page] : "";
  }
}

std::string Controller::PageTab(Scene scene, std::size_t page) const
{
  static constexpr std::array<const char*, kTapePageCount> kTape{{"TAPE", "MIX", "SENDS", "FX", "PROJECT", "FILES", "SYSTEM"}};
  static constexpr std::array<const char*, 3> kSequence{{"STEPS", "PATTERN", "TRACK"}};
  switch (scene)
  {
    case Scene::Instrument:
    {
      const InstrumentPageList list = InstrumentPages();
      return page < list.count ? list.pages[page].tab : "";
    }
    case Scene::Sequence: return page < kSequence.size() ? kSequence[page] : "";
    case Scene::Tape:
    default: return page < kTape.size() ? kTape[page] : "";
  }
}

void Controller::ClampPages()
{
  for (std::size_t scene = 0; scene < kSceneCount; ++scene)
  {
    const std::size_t count = PageCount(static_cast<Scene>(scene));
    if (state_.page[scene] >= count) state_.page[scene] = static_cast<std::uint8_t>(count - 1);
  }
}

bool Controller::PadNoteFor(std::size_t row, std::size_t column, int& note) const
{
  return PadNote(state_.keyboard, ActiveKind(), row, column, note);
}

bool Controller::IsPadDown(std::size_t row, std::size_t column) const
{
  if (row >= kRows || column < kPadFirstColumn || column >= kPadFirstColumn + kPadColumns) return false;
  return state_.padNote[PadIndex(row, column)] != kNoNote;
}

bool Controller::IsNoteHeld(int note) const
{
  for (std::size_t index = 0; index < state_.padNote.size(); ++index)
    if (state_.padNote[index] == note && state_.padTrack[index] == state_.track) return true;
  return false;
}

std::size_t Controller::CollectHeldNotes(std::array<std::uint8_t, kRows * kPadColumns>& notes) const
{
  std::size_t count = 0;
  for (std::size_t index = 0; index < state_.padNote.size(); ++index)
    if (state_.padNote[index] != kNoNote && state_.padTrack[index] == state_.track) notes[count++] = state_.padNote[index];
  return count;
}

void Controller::ReleaseAllPads()
{
  for (std::size_t index = 0; index < state_.padNote.size(); ++index)
  {
    if (state_.padNote[index] == kNoNote) continue;
    host_.NoteOff(state_.padTrack[index], state_.padNote[index]);
    host_.Sequencer().RecordNoteOff(state_.padTrack[index], state_.padNote[index]);
    state_.padNote[index] = kNoNote;
  }
}

void Controller::ShowToast(const std::string& text, ToastKind kind)
{
  state_.toast.text = text;
  state_.toast.kind = kind;
  state_.toast.until = state_.now + kToastMs;
}

// ---------------------------------------------------------------------------------------------
// Input routing

void Controller::KeyEvent(std::size_t row, std::size_t column, bool pressed, std::uint32_t nowMs)
{
  state_.now = nowMs;
  const KeyRole role = RoleFor(row, column);
  if (role == KeyRole::None) return;
  if (pressed) state_.keyPressedAt[KeyIndex(row, column)] = nowMs == 0 ? 1 : nowMs;

  // Text entry owns the whole keyboard except the left modifiers. Releases still settle key state
  // that was pressed before the field opened (a held FN, the SAVE AS key, a sounding pad).
  const Modal* modal = TopModal();
  if (modal != nullptr && modal->kind == ModalKind::TextEntry)
  {
    if (pressed)
    {
      if (role != KeyRole::Esc) HandleTextKey(row, column);
      return;
    }
    switch (role)
    {
      case KeyRole::Esc:
        if (state_.left[0].down) state_.left[0] = LeftKeyState{};
        else CloseModal();
        return;
      case KeyRole::Tab:
      case KeyRole::Fn:
      case KeyRole::Ctrl:
        state_.left[row] = LeftKeyState{};
        return;
      case KeyRole::Sub:
      case KeyRole::Add:
        state_.repeat = RepeatState{};
        return;
      case KeyRole::Pad:
        ReleasePad(row, column);
        return;
      case KeyRole::Grid:
        state_.gridDown[GridIndex(row, column)] = false;
        return;
      default:
        return;
    }
  }

  switch (role)
  {
    case KeyRole::Esc:
    case KeyRole::Tab:
    case KeyRole::Fn:
    case KeyRole::Ctrl:
      pressed ? PressLeft(row) : ReleaseLeft(row);
      return;
    case KeyRole::Sub:
    case KeyRole::Add:
    case KeyRole::Enter:
    case KeyRole::Play:
      pressed ? PressRight(role) : ReleaseRight(role);
      return;
    case KeyRole::Pad:
      pressed ? PressPad(row, column) : ReleasePad(row, column);
      return;
    case KeyRole::Grid:
      pressed ? PressGrid(GridIndex(row, column)) : ReleaseGrid(GridIndex(row, column));
      return;
    case KeyRole::None:
    default:
      return;
  }
}

void Controller::HomeButton(bool pressed, std::uint32_t nowMs)
{
  state_.now = nowMs;
  if (!pressed) return;
  ReleaseAllPads();
  host_.Panic();
  state_.panicAt = nowMs == 0 ? 1 : nowMs;
  ShowToast("ALL STOP");
}

void Controller::PressLeft(std::size_t row)
{
  auto& key = state_.left[row];
  key.down = true;
  key.used = false;
  key.suppressed = false;
  key.pressedAt = state_.now;
  const KeyRole role = RoleFor(row, 0);
  if (role == KeyRole::Ctrl)
  {
    // CTRL steps pages on release (a tap); held, it opens the page map. Fn + CTRL steps back now.
    if (TopModal() != nullptr)
    {
      key.suppressed = true;
      return;
    }
    if (state_.left[LeftIndex(KeyRole::Fn)].down)
    {
      state_.left[LeftIndex(KeyRole::Fn)].used = true;
      key.used = true;
      key.suppressed = true;
      StepPage(-1);
    }
  }
}

void Controller::ReleaseLeft(std::size_t row)
{
  auto& key = state_.left[row];
  if (!key.down) return;
  const bool tap = !key.used && state_.now - key.pressedAt < kHoldMs;
  key.down = false;
  switch (RoleFor(row, 0))
  {
    case KeyRole::Esc:
      if (tap) Back();
      break;
    case KeyRole::Tab:
      if (tap && TopModal() == nullptr) SelectTrack((state_.track + 1) % kTrackCount);
      break;
    case KeyRole::Ctrl:
      if (tap && !key.suppressed && TopModal() == nullptr) StepPage(1);
      break;
    case KeyRole::Fn:
    default:
      break;
  }
  key.used = false;
  key.suppressed = false;
}

void Controller::PressRight(KeyRole role)
{
  const bool fn = state_.left[LeftIndex(KeyRole::Fn)].down;
  if (fn) state_.left[LeftIndex(KeyRole::Fn)].used = true;
  const bool tab = state_.left[LeftIndex(KeyRole::Tab)].down;
  auto& ctrl = state_.left[LeftIndex(KeyRole::Ctrl)];
  const bool pageMap = ctrl.down && !ctrl.suppressed && TopModal() == nullptr;
  if (pageMap) ctrl.used = true;
  switch (role)
  {
    case KeyRole::Sub:
    case KeyRole::Add:
    {
      const int direction = role == KeyRole::Sub ? -1 : 1;
      if (pageMap)
      {
        StepPage(direction);
        return;
      }
      if (tab && TopModal() == nullptr)
      {
        state_.left[LeftIndex(KeyRole::Tab)].used = true;
        SelectTrack((state_.track + kTrackCount + static_cast<std::size_t>(direction + static_cast<int>(kTrackCount))) % kTrackCount);
        return;
      }
      state_.repeat.down = true;
      state_.repeat.direction = direction;
      state_.repeat.coarse = fn;
      state_.repeat.count = 0;
      state_.repeat.nextAt = state_.now + kRepeatDelayMs;
      Adjust(direction, fn);
      return;
    }
    case KeyRole::Enter:
      fn ? FnEnter() : Enter();
      return;
    case KeyRole::Play:
      fn ? ToggleRecord() : TogglePlay();
      return;
    default:
      return;
  }
}

void Controller::ReleaseRight(KeyRole role)
{
  if (role == KeyRole::Sub || role == KeyRole::Add)
  {
    const int direction = role == KeyRole::Sub ? -1 : 1;
    if (state_.repeat.direction == direction) state_.repeat = RepeatState{};
  }
}

void Controller::PressPad(std::size_t row, std::size_t column)
{
  // While ESC, TAB or CTRL is held the GRID chooses; the pads stay silent.
  for (const KeyRole modifier : {KeyRole::Esc, KeyRole::Tab, KeyRole::Ctrl})
  {
    auto& key = state_.left[LeftIndex(modifier)];
    if (!key.down || (modifier == KeyRole::Ctrl && key.suppressed)) continue;
    key.used = true;
    return;
  }
  if (state_.left[LeftIndex(KeyRole::Fn)].down)
  {
    state_.left[LeftIndex(KeyRole::Fn)].used = true;
    HandleFnKey(row, column);
    return;
  }
  if (HandleSearchKey(row, column)) return;

  int note = 0;
  if (!PadNoteFor(row, column, note)) return;
  const std::size_t index = PadIndex(row, column);
  if (state_.padNote[index] != kNoNote) return;

  // A selection in STEPS turns the pads into a note editor for the selected steps.
  const bool editing = state_.scene == Scene::Sequence && CurrentPage() == 0 && TopModal() == nullptr &&
                       PadForSequence(note, true);
  auto& sequencer = host_.Sequencer();
  const bool audition = !editing || !sequencer.Playing();
  state_.padNote[index] = static_cast<std::uint8_t>(note);
  state_.padTrack[index] = static_cast<std::uint8_t>(state_.track);
  state_.lastTrackNoteAt[state_.track] = state_.now;
  if (audition) host_.NoteOn(state_.track, static_cast<std::uint8_t>(note), kPadVelocity);
  if (!editing && sequencer.RecordArmed() && sequencer.Playing())
  {
    sequencer.RecordNoteOn(state_.track, static_cast<std::uint8_t>(note), kPadVelocity);
    MarkDirty();
  }
}

void Controller::ReleasePad(std::size_t row, std::size_t column)
{
  if (row >= kRows || column < kPadFirstColumn || column >= kPadFirstColumn + kPadColumns) return;
  const std::size_t index = PadIndex(row, column);
  if (state_.padNote[index] == kNoNote) return;
  const std::uint8_t note = state_.padNote[index];
  const std::uint8_t track = state_.padTrack[index];
  state_.padNote[index] = kNoNote;
  host_.NoteOff(track, note);
  host_.Sequencer().RecordNoteOff(track, note);
}

void Controller::PressGrid(std::size_t index)
{
  if (index >= kGridCount) return;
  const std::size_t row = index / kGridColumns;
  const std::size_t column = kGridFirstColumn + index % kGridColumns;
  // Hold maps. ESC: the GRID columns are the three scenes.
  auto& esc = state_.left[LeftIndex(KeyRole::Esc)];
  if (esc.down)
  {
    esc.used = true;
    if (index % kGridColumns < kSceneCount) GoToScene(static_cast<Scene>(index % kGridColumns));
    return;
  }
  // TAB: the columns are the tracks, laid out like MIX (select / mute / solo / sequencer).
  auto& tab = state_.left[LeftIndex(KeyRole::Tab)];
  if (tab.down)
  {
    tab.used = true;
    if (TopModal() == nullptr) MixGrid(index);
    return;
  }
  // CTRL: every page of the current scene, in order.
  auto& ctrl = state_.left[LeftIndex(KeyRole::Ctrl)];
  if (ctrl.down && !ctrl.suppressed && TopModal() == nullptr)
  {
    ctrl.used = true;
    if (index < PageCount(state_.scene)) SetPage(static_cast<std::uint8_t>(index));
    return;
  }
  if (state_.left[LeftIndex(KeyRole::Fn)].down)
  {
    state_.left[LeftIndex(KeyRole::Fn)].used = true;
    HandleFnKey(row, column);
    return;
  }
  if (HandleSearchKey(row, column)) return;
  state_.gridDown[index] = true;
  if (TopModal() != nullptr)
  {
    ModalGrid(index);
    return;
  }
  switch (state_.scene)
  {
    case Scene::Instrument: InstrumentGrid(index); break;
    case Scene::Sequence: SequenceGrid(index); break;
    case Scene::Tape:
      if (CurrentPage() == kTapeSong) SongGrid(index);
      else if (CurrentPage() == kTapeMix) MixGrid(index);
      else if (CurrentPage() == kTapeSends) SendsGrid(index);
      else if (CurrentPage() == kTapeFx) FxGrid(index);
      else ProjectGrid(index);
      break;
  }
}

void Controller::ReleaseGrid(std::size_t index)
{
  if (index >= kGridCount || !state_.gridDown[index]) return;
  state_.gridDown[index] = false;
  if (TopModal() == nullptr && state_.scene == Scene::Sequence) SequenceGridRelease(index);
}

bool Controller::HandleTextKey(std::size_t row, std::size_t column)
{
  Modal* modal = Top();
  if (modal == nullptr) return false;
  // The top-right key carries the "del" legend, so it erases whatever role it has elsewhere.
  if (row == 0 && column == kColumns - 1)
  {
    if (!modal->text.empty()) modal->text.pop_back();
    return true;
  }
  switch (RoleFor(row, column))
  {
    case KeyRole::Enter:
      CommitText();
      return true;
    case KeyRole::Add:
    case KeyRole::Sub:
    case KeyRole::Tab:
    case KeyRole::Fn:
    case KeyRole::Ctrl:
      return true;
    default:
      break;
  }
  const char value = LegendChar(row, column);
  const bool allowed = (value >= 'A' && value <= 'Z') || (value >= '0' && value <= '9') || value == ' ' || value == '-';
  if (!allowed) return true;
  if (value == ' ' && modal->text.empty()) return true;
  if (modal->text.size() < modal->maxLength) modal->text.push_back(value);
  return true;
}

bool Controller::HandleSearchKey(std::size_t row, std::size_t column)
{
  Modal* modal = Top();
  if (modal == nullptr || (modal->kind != ModalKind::PresetBrowser && modal->kind != ModalKind::SampleBrowser)) return false;
  const char value = LegendChar(row, column);
  const bool searchable = (value >= 'A' && value <= 'Z') || (value >= '0' && value <= '9') || value == '-';
  if (!searchable) return true;
  if (modal->search.size() < 16) modal->search.push_back(value);
  modal->kind == ModalKind::PresetBrowser ? RefreshPresetBrowser() : RefreshSampleBrowser();
  return true;
}

bool Controller::HandleFnKey(std::size_t row, std::size_t column)
{
  const char key = LegendChar(row, column);
  switch (key)
  {
    case ';': ArrowKey(Arrow::Up); return true;
    case '.': ArrowKey(Arrow::Down); return true;
    case ',': ArrowKey(Arrow::Left); return true;
    case '/': ArrowKey(Arrow::Right); return true;
    case '-': AdjustOctave(-1); return true;
    case '=': AdjustOctave(1); return true;
    case '1': case '2': case '3': case '4': ToggleMute(static_cast<std::size_t>(key - '1')); return true;
    default: break;
  }
  if (TopModal() != nullptr) return true;
  switch (key)
  {
    // GRID: tempo, octave (above), sound, pattern, layout, song mode, mute this track, arrows (above).
    case '9': NudgeTempo(-1); break;
    case '0': NudgeTempo(1); break;
    case 'O':
    case 'P':
      StepPreset(key == 'O' ? -1 : 1);
      if (const synth::ISynthEngine* engine = ActiveEngine()) ShowToast(engine->CurrentPresetName());
      break;
    case '[': StepTrackPattern(-1); break;
    case ']': StepTrackPattern(1); break;
    case 'L': SongFnEnter(); break;
    case 'M': ToggleMute(state_.track); break;
    case 'C': Copy(); break;
    case 'V': Paste(); break;
    case 'X': Clear(); break;
    case 'Q': Quantize(); break;
    case 'D': Duplicate(); break;
    case 'T': TapTempo(); break;
    case 'S': QuickSave(); break;
    case 'K': CycleLayout(); break;
    default: break;
  }
  return true;
}

void Controller::Tick(std::uint32_t nowMs)
{
  state_.now = nowMs;
  auto& repeat = state_.repeat;
  if (repeat.down && nowMs >= repeat.nextAt)
  {
    ++repeat.count;
    repeat.nextAt = nowMs + kRepeatIntervalMs;
    const int step = repeat.count > kRepeatAccelerateAfter ? 5 : 1;
    Adjust(repeat.direction * step, repeat.coarse);
  }
  if (!state_.toast.text.empty() && nowMs >= state_.toast.until) state_.toast.text.clear();
  // The record modal follows the sampler's own state machine.
  const Modal* modal = TopModal();
  if (modal != nullptr && modal->kind == ModalKind::SampleRecord)
  {
    const auto sampler = host_.Sampler().BuildUiState(state_.track);
    if (host_.EngineKind(state_.track) != synth::SynthEngineKind::Sampler) CloseModal();
    else if (sampler.needsName && !sampler.overwritePending && sampler.phase == synth::SamplerTrackPhase::Naming)
      OpenTextEntry(TextPurpose::SampleName, "NAME SAMPLE", sampler.draftName, 18);
  }
  ClampPages();
}

// ---------------------------------------------------------------------------------------------
// Navigation

void Controller::GoToScene(Scene scene)
{
  if (scene == state_.scene) return;
  state_.modals.clear();
  host_.Sequencer().ClearSelectedSteps();
  state_.previousScene = state_.scene;
  state_.scene = scene;
  state_.sceneChangedAt = state_.now;
  if (scene == Scene::Tape && CurrentPage() == kTapeFiles) RefreshProjects();
  if (scene == Scene::Tape && CurrentPage() == kTapeMix) state_.mixFocus = static_cast<std::uint8_t>(state_.track);
}

void Controller::SetPage(std::uint8_t page)
{
  const std::size_t count = PageCount(state_.scene);
  if (count == 0) return;
  page = static_cast<std::uint8_t>(std::min<std::size_t>(page, count - 1));
  if (page == CurrentPage()) return;
  if (state_.scene == Scene::Sequence) host_.Sequencer().ClearSelectedSteps();
  state_.page[static_cast<std::size_t>(state_.scene)] = page;
  if (state_.scene == Scene::Tape && page == kTapeFiles) RefreshProjects();
  if (state_.scene == Scene::Tape && page == kTapeMix) state_.mixFocus = static_cast<std::uint8_t>(state_.track);
}

void Controller::StepPage(int delta)
{
  const int count = static_cast<int>(PageCount(state_.scene));
  if (count <= 1) return;
  const int next = (static_cast<int>(CurrentPage()) + delta % count + count) % count;
  SetPage(static_cast<std::uint8_t>(next));
}

void Controller::Back()
{
  if (TopModal() != nullptr)
  {
    ModalCancel();
    return;
  }
  auto& sequencer = host_.Sequencer();
  if (state_.scene == Scene::Sequence && sequencer.BuildUiState(state_.track).hasHeldSteps)
  {
    sequencer.ClearSelectedSteps();
    return;
  }
  GoToScene(state_.previousScene);
}

void Controller::SelectTrack(std::size_t track)
{
  if (track >= kTrackCount || track == state_.track) return;
  state_.trackInstrumentPage[state_.track] = state_.page[static_cast<std::size_t>(Scene::Instrument)];
  state_.trackInstrumentFocus[state_.track] = state_.instrumentFocus;
  state_.track = track;
  state_.page[static_cast<std::size_t>(Scene::Instrument)] = state_.trackInstrumentPage[track];
  state_.instrumentFocus = state_.trackInstrumentFocus[track];
  host_.Sequencer().SetActiveTrack(track);
  host_.Sequencer().ClearSelectedSteps();
  if (state_.mixFocus < kTrackCount) state_.mixFocus = static_cast<std::uint8_t>(track);
  state_.clipboardValid = state_.clipboardValid && !state_.clipboardIsPattern;
  ClampPages();
}

void Controller::Adjust(int delta, bool coarse)
{
  if (TopModal() != nullptr)
  {
    ModalAdjust(delta, coarse);
    return;
  }
  switch (state_.scene)
  {
    case Scene::Instrument: InstrumentAdjust(delta, coarse); break;
    case Scene::Sequence: SequenceAdjust(delta, coarse); break;
    case Scene::Tape:
      if (CurrentPage() == kTapeSong) SongAdjust(delta, coarse);
      else if (CurrentPage() == kTapeMix) MixAdjust(delta, coarse);
      else if (CurrentPage() == kTapeSends) SendsAdjust(delta, coarse);
      else if (CurrentPage() == kTapeFx) FxAdjust(delta, coarse);
      else ProjectAdjust(delta, coarse);
      break;
  }
}

void Controller::Enter()
{
  if (TopModal() != nullptr)
  {
    ModalEnter();
    return;
  }
  switch (state_.scene)
  {
    case Scene::Instrument: InstrumentEnter(); break;
    case Scene::Sequence: SequenceEnter(); break;
    case Scene::Tape:
      if (CurrentPage() == kTapeSong) SongEnter();
      else if (CurrentPage() == kTapeMix) MixEnter();
      else if (CurrentPage() == kTapeSends) SendsEnter();
      else if (CurrentPage() == kTapeFx) FxEnter();
      else ProjectEnter();
      break;
  }
}

void Controller::FnEnter()
{
  if (TopModal() != nullptr)
  {
    ModalFnEnter();
    return;
  }
  if (state_.scene == Scene::Instrument) InstrumentReset();
  else if (state_.scene == Scene::Sequence && CurrentPage() == 0) host_.Sequencer().CycleNoteScope();
  else if (state_.scene == Scene::Tape && CurrentPage() == kTapeSong) SongFnEnter();
  else if (state_.scene == Scene::Tape && CurrentPage() == kTapeSends) SendsReset();
  else if (state_.scene == Scene::Tape && CurrentPage() == kTapeFx) FxReset();
}

void Controller::TogglePlay()
{
  const Modal* modal = TopModal();
  if (modal != nullptr && modal->kind == ModalKind::SampleRecord)
  {
    host_.PreviewSample(state_.track);
    return;
  }
  auto& sequencer = host_.Sequencer();
  if (sequencer.Playing())
  {
    sequencer.Stop();
    return;
  }
  sequencer.Play();
}

void Controller::ToggleRecord()
{
  auto& sequencer = host_.Sequencer();
  const bool armed = !sequencer.RecordArmed();
  sequencer.SetRecordArmed(armed);
  ShowToast(armed ? "REC ARMED" : "REC OFF", armed ? ToastKind::Error : ToastKind::Info);
}

void Controller::ArrowKey(Arrow arrow)
{
  const int vertical = arrow == Arrow::Up ? -1 : arrow == Arrow::Down ? 1 : 0;
  const int horizontal = arrow == Arrow::Left ? -1 : arrow == Arrow::Right ? 1 : 0;
  Modal* modal = Top();
  if (modal != nullptr)
  {
    if (vertical != 0) ModalAdjust(vertical, false);
    else if (modal->kind == ModalKind::SampleRecord) modal->trimMarker = horizontal < 0 ? 0 : 1;
    return;
  }
  switch (state_.scene)
  {
    case Scene::Tape:
      if (CurrentPage() == kTapeFiles) ProjectAdjust(vertical == 0 ? horizontal : vertical, false);
      else if (CurrentPage() == kTapeSong) SongArrow(horizontal, vertical);
      else if (CurrentPage() == kTapeSends)
      {
        const int step = vertical != 0 ? vertical * static_cast<int>(kGridColumns) : horizontal;
        SendsGrid(static_cast<std::size_t>((state_.sendsFocus + step + static_cast<int>(kGridCount)) % static_cast<int>(kGridCount)));
      }
      else if (CurrentPage() == kTapeFx)
      {
        const int count = static_cast<int>(synth::fx::kMasterParamCount);
        const int step = vertical != 0 ? vertical * static_cast<int>(kGridColumns) : horizontal;
        state_.fxFocus = static_cast<std::uint8_t>((state_.fxFocus + step + count) % count);
      }
      else if (CurrentPage() == kTapeMix)
      {
        if (horizontal != 0)
        {
          state_.mixFocus = static_cast<std::uint8_t>((state_.mixFocus + 5 + horizontal) % 5);
          if (state_.mixFocus < kTrackCount) SelectTrack(state_.mixFocus);
        }
        else MixAdjust(-vertical, false);
      }
      break;
    case Scene::Instrument:
    {
      const InstrumentPage* page = CurrentInstrumentPage();
      const int step = vertical != 0 ? vertical * static_cast<int>(kGridColumns) : horizontal;
      int focus = InstrumentFocus();
      for (std::size_t attempt = 0; attempt < kGridCount; ++attempt)
      {
        focus = (focus + step + static_cast<int>(kGridCount)) % static_cast<int>(kGridCount);
        if (page->slots[static_cast<std::size_t>(focus)].IsParameter()) break;
      }
      if (page->slots[static_cast<std::size_t>(focus)].IsParameter())
        state_.instrumentFocus[std::min<std::size_t>(CurrentPage(), state_.instrumentFocus.size() - 1)] = static_cast<std::uint8_t>(focus);
      break;
    }
    case Scene::Sequence:
    {
      if (CurrentPage() != 0) break;
      auto& sequencer = host_.Sequencer();
      const auto ui = sequencer.BuildUiState(state_.track);
      const int step = vertical != 0 ? vertical * static_cast<int>(kGridColumns) : horizontal;
      const int from = ui.hasHeldSteps ? static_cast<int>(ui.primaryHeldStep) : -step;
      const int next = (from + step + static_cast<int>(kGridCount)) % static_cast<int>(kGridCount);
      sequencer.ClearSelectedSteps();
      sequencer.SetStepSelected(static_cast<std::size_t>(next), true);
      break;
    }
  }
}

// ---------------------------------------------------------------------------------------------
// LEDs
//
// One vocabulary for every key. Brightness (0..15) says how much:
//   0 no function here     1 exists but empty     2-3 available     4-11 value / content
//   12-14 focus or selection                       15 pressed or playing
// The effect says what kind of key it is: steady = holds a value, breathe = runs an action,
// shimmer = focus / selection, blink = waiting (queued pattern, cued row).
// The block the screen is about stays at full strength; the other block is dimmed.

namespace {

constexpr std::uint8_t kLedOff = 0;
constexpr std::uint8_t kLedTrace = 1;
constexpr std::uint8_t kLedAvailable = 2;
constexpr std::uint8_t kLedAction = 6;
constexpr std::uint8_t kLedFocus = 12;
constexpr std::uint8_t kLedFull = 15;

// Maps a 0..1 value onto the 4..11 band between "available" and "focus".
std::uint8_t ValueLevel(float fraction)
{
  fraction = std::max(0.0f, std::min(1.0f, fraction));
  return static_cast<std::uint8_t>(4 + static_cast<int>(fraction * 7.0f + 0.5f));
}

bool InScale(const KeyboardSettings& keys, int note)
{
  const std::uint16_t mask = kScales[std::min(keys.scale, kScales.size() - 1)].mask;
  const int degree = ((note - keys.root) % 12 + 12) % 12;
  return ((mask >> degree) & 1u) != 0;
}

}  // namespace

void Controller::ComputeLeds(std::array<std::uint8_t, kKeyCount>& levels) const
{
  LedFrame frame{};
  ComputeLeds(frame);
  for (std::size_t index = 0; index < kKeyCount; ++index) levels[index] = frame[index].level;
}

void Controller::ComputeLeds(LedFrame& frame) const
{
  frame.fill(LedCell{});
  const auto& sequencer = host_.Sequencer();
  const auto seq = sequencer.BuildUiState(state_.track);
  const Modal* modal = TopModal();
  const bool textEntry = modal != nullptr && modal->kind == ModalKind::TextEntry;
  const std::size_t page = CurrentPage();
  auto set = [&](std::size_t index, std::uint8_t level, LedEffect effect = LedEffect::Steady) {
    frame[index] = LedCell{level, effect};
  };
  auto gridKey = [](std::size_t index) { return KeyIndex(index / kGridColumns, kGridFirstColumn + index % kGridColumns); };
  auto setGrid = [&](std::size_t index, std::uint8_t level, LedEffect effect = LedEffect::Steady) {
    if (state_.gridDown[index]) set(gridKey(index), kLedFull);
    else set(gridKey(index), level, effect);
  };

  // Left keys: navigation, always usable. FN glows while recording is armed; CTRL only when the
  // screen has pages.
  set(KeyIndex(0, 0), 3);
  set(KeyIndex(1, 0), 3);
  set(KeyIndex(2, 0), sequencer.RecordArmed() ? 8 : 3, sequencer.RecordArmed() ? LedEffect::Breathe : LedEffect::Steady);
  set(KeyIndex(3, 0), PageCount(state_.scene) > 1 ? 3 : kLedTrace);
  for (std::size_t row = 0; row < kRows; ++row)
    if (state_.left[row].down) set(KeyIndex(row, 0), kLedFull);

  // Right keys: ADD / SUB when there is something to change, ENTER when it has an action, PLAY transport.
  bool adjustable = modal != nullptr;
  bool enterAction = modal != nullptr;
  switch (state_.scene)
  {
    case Scene::Instrument:
    {
      const InstrumentPage* instrumentPage = CurrentInstrumentPage();
      adjustable = adjustable || instrumentPage->graphic == PageGraphic::Voice || instrumentPage->slots[InstrumentFocus()].IsParameter();
      enterAction = true;
      break;
    }
    case Scene::Sequence:
      adjustable = true;
      enterAction = enterAction || page != 0 || seq.hasHeldSteps;
      break;
    case Scene::Tape:
      adjustable = adjustable || page != kTapeFiles || !state_.projects.empty();
      enterAction = true;
      break;
  }
  const std::uint8_t adjustLevel = adjustable ? 5 : kLedTrace;
  set(KeyIndex(0, kColumns - 1), state_.repeat.down && state_.repeat.direction > 0 ? kLedFull : adjustLevel);
  set(KeyIndex(1, kColumns - 1), state_.repeat.down && state_.repeat.direction < 0 ? kLedFull : adjustLevel);
  if (modal != nullptr && modal->kind == ModalKind::Confirm) set(KeyIndex(2, kColumns - 1), 10, LedEffect::Breathe);
  else set(KeyIndex(2, kColumns - 1), enterAction ? 5 : kLedTrace);
  set(KeyIndex(3, kColumns - 1), seq.playing ? 8 : 4);

  if (textEntry)
  {
    // Typing: characters even and dim; erase (top right "del") and ENTER stand out.
    for (std::size_t row = 0; row < kRows; ++row)
      for (std::size_t column = 1; column + 1 < kColumns; ++column)
        if (LegendChar(row, column) != 0) set(KeyIndex(row, column), 4);
    set(KeyIndex(3, kColumns - 1), 4);  // space
    set(KeyIndex(0, kColumns - 1), modal->text.empty() ? kLedTrace : 8);
    set(KeyIndex(1, kColumns - 1), kLedOff);
    set(KeyIndex(2, kColumns - 1), modal->text.empty() ? 3 : 11, modal->text.empty() ? LedEffect::Steady : LedEffect::Breathe);
    set(KeyIndex(0, 0), 6);
    return;
  }

  // Hold maps light only the GRID keys they accept (current choice shimmers, the others breathe or
  // show their value); everything else goes dark so the map reads on its own.
  if (EscOverlay())
  {
    for (std::size_t index = 0; index < kGridCount; ++index)
    {
      const std::size_t scene = index % kGridColumns;
      if (scene >= kSceneCount) continue;
      if (scene == static_cast<std::size_t>(state_.scene)) setGrid(index, kLedFocus, LedEffect::Shimmer);
      else setGrid(index, 6, LedEffect::Breathe);
    }
    return;
  }
  if (TabOverlay())
  {
    for (std::size_t track = 0; track < kTrackCount; ++track)
    {
      const float level = static_cast<float>(host_.TrackLevel(track)) / 100.0f;
      if (track == state_.track) setGrid(track, kLedFocus, LedEffect::Shimmer);
      else setGrid(track, host_.TrackAudible(track) ? ValueLevel(level) : kLedTrace);
      setGrid(4 + track, host_.TrackMuted(track) ? kLedFocus : kLedAvailable);
      setGrid(8 + track, host_.TrackSoloed(track) ? kLedFocus : kLedAvailable);
      setGrid(12 + track, seq.tracks[track].enabled ? 7 : kLedTrace);
    }
    return;
  }
  if (CtrlOverlay())
  {
    const std::size_t count = std::min(PageCount(state_.scene), kGridCount);
    for (std::size_t index = 0; index < count; ++index)
      setGrid(index, index == page ? kLedFocus : 4, index == page ? LedEffect::Shimmer : LedEffect::Steady);
    return;
  }
  if (state_.left[2].down)
  {
    // Letter shortcuts on the pads, the function map on the GRID (arrows steady, actions breathing).
    for (std::size_t row = 0; row < kRows; ++row)
      for (std::size_t column = kPadFirstColumn; column < kPadFirstColumn + kPadColumns; ++column)
      {
        const char key = LegendChar(row, column);
        std::uint8_t level = kLedOff;
        LedEffect effect = LedEffect::Breathe;
        if (key >= '1' && key <= '4') { level = host_.TrackMuted(static_cast<std::size_t>(key - '1')) ? 12 : 5; effect = LedEffect::Steady; }
        else if (key == 'C' || key == 'V' || key == 'X' || key == 'Q')
          level = state_.scene == Scene::Sequence || (state_.scene == Scene::Tape && page == kTapeSong) ? 6 : kLedTrace;
        else if (key == 'D' || key == 'T' || key == 'S') level = 6;
        set(KeyIndex(row, column), level, level <= kLedTrace ? LedEffect::Steady : effect);
      }
    const auto functions = FnGridEntries();
    for (std::size_t index = 0; index < kGridCount; ++index)
    {
      const auto& entry = functions[index];
      if (entry.label == nullptr) continue;
      const bool arrow = index == 10 || index >= 13;
      if (!entry.available) setGrid(index, kLedTrace);
      else if (arrow) setGrid(index, 9);
      else setGrid(index, 6, LedEffect::Breathe);
    }
    return;
  }

  // Pads: the musical map. Root brightest, fifth next, scale tones, then the rest. When a step is
  // selected the pads become its note editor: the notes it holds light up.
  const auto kind = ActiveKind();
  const bool searching = modal != nullptr && (modal->kind == ModalKind::PresetBrowser || modal->kind == ModalKind::SampleBrowser);
  const bool chordEditor = state_.scene == Scene::Sequence && page == 0 && modal == nullptr && seq.hasHeldSteps;
  std::array<bool, 128> chord{};
  if (chordEditor)
  {
    std::array<std::uint8_t, 8> notes{};
    const std::size_t count = sequencer.StepNotes(seq.primaryHeldStep, notes);
    for (std::size_t index = 0; index < count; ++index) chord[notes[index] & 0x7F] = true;
  }
  // The block the screen is not about recedes so the active block reads at a glance.
  float padScale = 1.0f;
  if (state_.scene == Scene::Tape) padScale = 0.35f;
  else if (state_.scene == Scene::Sequence && !chordEditor) padScale = 0.6f;
  if (modal != nullptr && !searching) padScale = 0.5f;
  const synth::ISynthEngine* engine = ActiveEngine();
  for (std::size_t row = 0; row < kRows; ++row)
  {
    for (std::size_t column = kPadFirstColumn; column < kPadFirstColumn + kPadColumns; ++column)
    {
      const std::size_t index = KeyIndex(row, column);
      int note = 0;
      if (searching)
      {
        set(index, LegendChar(row, column) != 0 ? 3 : kLedOff);
        continue;
      }
      if (IsPadDown(row, column))
      {
        set(index, kLedFull);
        continue;
      }
      if (!PadNoteFor(row, column, note)) continue;
      if (chordEditor && chord[static_cast<std::size_t>(note)])
      {
        set(index, kLedFocus, LedEffect::Shimmer);
        continue;
      }
      std::uint8_t level = kLedOff;
      if (kind == synth::SynthEngineKind::Drums)
      {
        // Each drum column glows with its kit level.
        static constexpr std::array<const char*, 8> kIds{{"kick.level", "snare.level", "closed_hat.level", "open_hat.level",
                                                          "clap.level", "tom.level", "cowbell.level", "maraca.level"}};
        synth::SynthParameterInfo info;
        const float value = engine != nullptr && engine->GetParameterInfo(kIds[column - kPadFirstColumn], info)
            ? static_cast<float>(info.value) / 127.0f : 0.8f;
        level = static_cast<std::uint8_t>(2 + static_cast<int>(value * 6.0f + 0.5f));
      }
      else
      {
        const KeyboardSettings& keys = state_.keyboard;
        const int pitch = ((note - (keys.layout == KeyboardLayout::Piano ? 0 : keys.root)) % 12 + 12) % 12;
        if (pitch == 0) level = 9;
        else if (pitch == 7) level = 5;
        else if (keys.layout == KeyboardLayout::Piano) level = (pitch == 1 || pitch == 3 || pitch == 6 || pitch == 8 || pitch == 10) ? kLedTrace : 3;
        else if (keys.layout == KeyboardLayout::Chromatic) level = InScale(keys, note) ? 3 : kLedTrace;
        else level = 3;
      }
      set(index, static_cast<std::uint8_t>(std::max(level == 0 ? 0 : 1, static_cast<int>(static_cast<float>(level) * padScale + 0.5f))));
    }
  }

  // GRID mirrors the 4x4 cells on screen, brightness scaled by what each cell holds.
  if (modal != nullptr)
  {
    if (modal->kind == ModalKind::SampleRecord)
      for (std::size_t index = 0; index < 4; ++index)
        setGrid(index, index < 2 && modal->trimMarker == index ? kLedFocus : kLedAction, index < 2 && modal->trimMarker == index ? LedEffect::Shimmer : LedEffect::Breathe);
    if (modal->kind == ModalKind::EnginePicker)
      for (std::size_t index = 0; index < modal->items.size() && index < 4; ++index)
        setGrid(index, index == modal->cursor ? kLedFocus : (modal->items[index].marked ? 8 : 4), index == modal->cursor ? LedEffect::Shimmer : LedEffect::Steady);
    return;
  }
  switch (state_.scene)
  {
    case Scene::Instrument:
    {
      const InstrumentPage* instrumentPage = CurrentInstrumentPage();
      for (std::size_t index = 0; index < kGridCount; ++index)
      {
        const Slot& slot = instrumentPage->slots[index];
        if (slot.Empty()) continue;
        if (slot.IsAction())
        {
          setGrid(index, kLedAction, LedEffect::Breathe);
          continue;
        }
        if (index == InstrumentFocus())
        {
          setGrid(index, kLedFocus, LedEffect::Shimmer);
          continue;
        }
        synth::SynthParameterInfo info;
        std::string text;
        std::uint8_t level = kLedAvailable;
        if (SlotValue(slot, info, text))
        {
          const float span = static_cast<float>(std::max(1, info.maxValue - info.minValue));
          const float fraction = static_cast<float>(info.value - info.minValue) / span;
          level = info.kind == synth::SynthParameterKind::Toggle ? (info.value != 0 ? 9 : kLedAvailable) : ValueLevel(fraction);
        }
        setGrid(index, level);
      }
      break;
    }
    case Scene::Sequence:
      if (page == 0)
      {
        const std::size_t steps = seq.tracks[state_.track].stepCount;
        for (std::size_t index = 0; index < kGridCount; ++index)
        {
          const auto& step = seq.steps[index];
          if (step.playing)
          {
            setGrid(index, kLedFull);
            continue;
          }
          if (step.held)
          {
            setGrid(index, 13, LedEffect::Shimmer);
            continue;
          }
          std::uint8_t level = kLedOff;
          if (index < steps)
          {
            // Empty steps keep the beat visible; filled steps scale with velocity, chords a notch brighter.
            level = index % 4 == 0 ? kLedAvailable : kLedTrace;
            if (step.hasEvents)
              level = static_cast<std::uint8_t>(std::min(11, 4 + step.velocity * 6 / 127 + (step.noteCount > 1 ? 1 : 0)));
          }
          else if (step.hasEvents) level = kLedTrace;
          setGrid(index, level);
        }
      }
      else if (page == 1)
      {
        const auto& track = seq.tracks[state_.track];
        for (std::size_t index = 0; index < kGridCount; ++index)
        {
          if (index == track.queuedPattern) setGrid(index, 13, LedEffect::Blink);
          else if (index == track.activePattern) setGrid(index, kLedFocus, LedEffect::Shimmer);
          else if (index < track.patternCount)
            setGrid(index, seq.patternStepCounts[index] == 0 ? kLedAvailable : ValueLevel(static_cast<float>(seq.patternStepCounts[index]) / 16.0f));
          else if (index == track.patternCount) setGrid(index, 4, LedEffect::Breathe);
        }
      }
      else
      {
        const auto& track = seq.tracks[state_.track];
        const std::array<std::uint8_t, 4> values{{track.enabled ? std::uint8_t{9} : kLedAvailable,
                                                  ValueLevel(static_cast<float>(track.stepCount) / 16.0f),
                                                  ValueLevel(static_cast<float>(seq.swing - 50) / 25.0f),
                                                  ValueLevel(static_cast<float>(seq.bpm - 30) / 270.0f)}};
        for (std::size_t index = 0; index < 4; ++index)
          setGrid(index, index == state_.seqTrackFocus ? kLedFocus : values[index], index == state_.seqTrackFocus ? LedEffect::Shimmer : LedEffect::Steady);
      }
      break;
    case Scene::Tape:
      if (page == kTapeSong)
      {
        for (std::size_t column = 0; column < kGridColumns; ++column)
        {
          const std::size_t row = state_.songScroll + column;
          for (std::size_t track = 0; track < kTrackCount; ++track)
          {
            const std::size_t index = track * kGridColumns + column;
            if (row == state_.songRow && track == state_.songTrack)
            {
              setGrid(index, 13, LedEffect::Shimmer);
              continue;
            }
            if (row < seq.songLength)
            {
              const auto songRow = sequencer.SongRowAt(row);
              const bool playingRow = seq.songPlaying && row == seq.songRow;
              if (playingRow) setGrid(index, songRow.patterns[track] < 0 ? 4 : 11);
              else if (seq.songCueRow == row) setGrid(index, 8, LedEffect::Blink);
              else setGrid(index, songRow.patterns[track] < 0 ? kLedTrace : static_cast<std::uint8_t>(4 + std::min<int>(songRow.repeats, 12) / 3));
            }
            else if (row == seq.songLength && track == 0) setGrid(index, 4, LedEffect::Breathe);
          }
        }
      }
      else if (page == kTapeMix)
      {
        for (std::size_t track = 0; track < kTrackCount; ++track)
        {
          const float level = static_cast<float>(host_.TrackLevel(track)) / 100.0f;
          if (track == state_.mixFocus) setGrid(track, kLedFocus, LedEffect::Shimmer);
          else setGrid(track, host_.TrackAudible(track) ? ValueLevel(level) : kLedTrace);
          setGrid(4 + track, host_.TrackMuted(track) ? kLedFocus : kLedAvailable);
          setGrid(8 + track, host_.TrackSoloed(track) ? kLedFocus : kLedAvailable);
          setGrid(12 + track, seq.tracks[track].enabled ? 7 : kLedTrace);
        }
      }
      else if (page == kTapeSends)
      {
        for (std::size_t index = 0; index < kGridCount; ++index)
        {
          if (index == state_.sendsFocus)
          {
            setGrid(index, kLedFocus, LedEffect::Shimmer);
            continue;
          }
          const synth::ISynthEngine* trackEngine = host_.Engine(index % kGridColumns);
          synth::SynthParameterInfo info;
          if (trackEngine == nullptr || !trackEngine->GetParameterInfo(kSendIds[index / kGridColumns], info))
          {
            setGrid(index, kLedTrace);
            continue;
          }
          // Off reads as "available"; the further from off, the brighter.
          const float span = static_cast<float>(std::max(1, info.maxValue - info.minValue));
          const float amount = info.defaultValue > info.minValue
              ? static_cast<float>(std::abs(info.value - info.defaultValue)) / static_cast<float>(info.defaultValue - info.minValue)
              : static_cast<float>(info.value - info.minValue) / span;
          setGrid(index, info.value == info.defaultValue ? kLedAvailable : ValueLevel(amount));
        }
      }
      else if (page == kTapeFx)
      {
        for (std::size_t param = 0; param < synth::fx::kMasterParamCount; ++param)
        {
          const auto& info = synth::fx::kMasterParams[param];
          const float fraction = static_cast<float>(host_.MasterFx(param) - info.minValue) / static_cast<float>(info.maxValue - info.minValue);
          if (param == state_.fxFocus) setGrid(param, kLedFocus, LedEffect::Shimmer);
          else setGrid(param, ValueLevel(fraction));
        }
      }
      else if (page == kTapeProject)
      {
        for (std::size_t index = 0; index < 4; ++index) setGrid(index, kLedAction, LedEffect::Breathe);
        setGrid(4, state_.projectFocus == 4 ? kLedFocus : ValueLevel(static_cast<float>(seq.bpm - 30) / 270.0f),
                state_.projectFocus == 4 ? LedEffect::Shimmer : LedEffect::Steady);
        setGrid(5, state_.projectFocus == 5 ? kLedFocus : ValueLevel(static_cast<float>(seq.swing - 50) / 25.0f),
                state_.projectFocus == 5 ? LedEffect::Shimmer : LedEffect::Steady);
        setGrid(6, kLedAction, LedEffect::Breathe);
      }
      else if (page == kTapeSystem)
      {
        const std::array<float, 3> values{{static_cast<float>(host_.LedBrightness()) / 15.0f, static_cast<float>(host_.Backlight()) / 255.0f,
                                           static_cast<float>(host_.MasterVolume()) / 100.0f}};
        for (std::size_t index = 0; index < 3; ++index)
          setGrid(index, index == state_.systemFocus ? kLedFocus : ValueLevel(values[index]), index == state_.systemFocus ? LedEffect::Shimmer : LedEffect::Steady);
      }
      break;
  }
}

}  // namespace groove
