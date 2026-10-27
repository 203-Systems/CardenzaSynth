#include "Applications/Synth/UI/Groove/GrooveController.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace groove {

namespace {

constexpr std::size_t kProjectSave = 0;
constexpr std::size_t kProjectSaveAs = 1;
constexpr std::size_t kProjectLoad = 2;
constexpr std::size_t kProjectNew = 3;
constexpr std::size_t kProjectBpm = 4;
constexpr std::size_t kProjectSwing = 5;
constexpr std::size_t kProjectTap = 6;

constexpr std::size_t kTrackSeq = 0;
constexpr std::size_t kTrackSteps = 1;
constexpr std::size_t kTrackSwing = 2;
constexpr std::size_t kTrackBpm = 3;

bool StartsWith(const char* text, const char* prefix)
{
  return text != nullptr && std::strncmp(text, prefix, std::strlen(prefix)) == 0;
}

std::string PatternLabel(std::size_t index)
{
  char text[8];
  std::snprintf(text, sizeof(text), "P%02u", static_cast<unsigned>(index + 1));
  return text;
}

}  // namespace

// ---------------------------------------------------------------------------------------------
// Shared slot presentation

bool Controller::SlotHasValue(const Slot& slot) const
{
  return slot.IsParameter();
}

bool Controller::SlotValue(const Slot& slot, synth::SynthParameterInfo& info, std::string& text) const
{
  if (!slot.IsParameter()) return false;
  if (SettingValue(slot.id, info, text)) return true;
  const synth::ISynthEngine* engine = ActiveEngine();
  if (engine == nullptr || !engine->GetParameterInfo(slot.id, info)) return false;
  text = engine->DescribeParameterValue(slot.id);
  return true;
}

// ---------------------------------------------------------------------------------------------
// Global settings (FN SETUP): keyboard, chord and arpeggiator for every track, plus the system.

namespace {

bool IsSetting(const char* id)
{
  return StartsWith(id, "keys.") || StartsWith(id, "arp.") || StartsWith(id, "proj.") || StartsWith(id, "sys.");
}

bool Is(const char* id, const char* name)
{
  return std::strcmp(id, name) == 0;
}

}  // namespace

bool Controller::SettingValue(const char* id, synth::SynthParameterInfo& info, std::string& text) const
{
  if (id == nullptr || !IsSetting(id)) return false;
  const KeyboardSettings& keys = state_.keyboard;
  const synth::ArpSettings arp = host_.Sequencer().GetArpSettings();
  info = synth::SynthParameterInfo{};
  info.id = id;
  info.kind = synth::SynthParameterKind::Choice;
  info.minValue = 0;
  auto choice = [&](int value, int count, int fallback, const char* name) {
    info.maxValue = count - 1;
    info.value = std::clamp(value, 0, count - 1);
    info.defaultValue = fallback;
    text = name;
  };
  auto number = [&](int value, int minimum, int maximum, int fallback, const std::string& shown) {
    info.kind = synth::SynthParameterKind::Continuous;
    info.minValue = minimum;
    info.maxValue = maximum;
    info.value = value;
    info.defaultValue = fallback;
    text = shown;
  };
  if (Is(id, "keys.layout")) choice(static_cast<int>(keys.layout), 3, 0, kLayoutNames[static_cast<std::size_t>(keys.layout)]);
  else if (Is(id, "keys.scale")) choice(static_cast<int>(keys.scale), static_cast<int>(kScales.size()), 0, kScales[std::min(keys.scale, kScales.size() - 1)].label);
  else if (Is(id, "keys.root")) choice(keys.root, 12, 0, kNoteNames[static_cast<std::size_t>(std::clamp(keys.root, 0, 11))]);
  else if (Is(id, "keys.octave")) number(keys.octave, -3, 3, 0, (keys.octave > 0 ? "+" : "") + std::to_string(keys.octave));
  else if (Is(id, "keys.chord")) choice(static_cast<int>(keys.chord), static_cast<int>(kChordTypeCount), 0, kChordNames[static_cast<std::size_t>(keys.chord)]);
  else if (Is(id, "keys.voicing")) choice(keys.voicing, static_cast<int>(kVoicingNames.size()), 0, kVoicingNames[std::min<std::size_t>(keys.voicing, 3)]);
  else if (Is(id, "keys.lock")) choice(static_cast<int>(keys.lock), 3, 0, kScaleLockNames[static_cast<std::size_t>(keys.lock)]);
  else if (Is(id, "keys.velocity")) number(keys.velocity, 1, 127, 110, std::to_string(keys.velocity));
  else if (Is(id, "arp.mode")) choice(static_cast<int>(arp.mode), static_cast<int>(synth::kArpModeCount), 0, synth::kArpModeNames[static_cast<std::size_t>(arp.mode)]);
  else if (Is(id, "arp.rate")) choice(arp.rate, static_cast<int>(synth::kArpRateCount), 3, synth::kArpRateNames[std::min<std::size_t>(arp.rate, synth::kArpRateCount - 1)]);
  else if (Is(id, "arp.octaves")) number(arp.octaves, 1, 4, 1, std::to_string(arp.octaves) + " OCT");
  else if (Is(id, "arp.gate")) number(arp.gate, 10, 100, 50, std::to_string(arp.gate) + "%");
  else if (Is(id, "arp.latch"))
  {
    choice(arp.latch ? 1 : 0, 2, 0, arp.latch ? "ON" : "OFF");
    info.kind = synth::SynthParameterKind::Toggle;
  }
  else if (StartsWith(id, "proj."))
  {
    // Project settings live in the sequencer and are saved with the project.
    const auto& sequencer = host_.Sequencer();
    if (Is(id, "proj.bpm")) number(sequencer.Tempo(), 30, 300, 120, std::to_string(sequencer.Tempo()));
    else if (Is(id, "proj.swing")) number(sequencer.Swing(), 50, 75, 50, std::to_string(sequencer.Swing()) + "%");
    else if (Is(id, "proj.mode")) choice(sequencer.SongMode() ? 1 : 0, 2, 0, sequencer.SongMode() ? "SONG" : "LOOP");
    else if (Is(id, "proj.steps"))
      number(static_cast<int>(sequencer.TrackStepCount(state_.track)), 1, 16, 16, "T" + std::to_string(state_.track + 1) + " " + std::to_string(sequencer.TrackStepCount(state_.track)));
    else if (Is(id, "proj.rec"))
    {
      choice(sequencer.RecordArmed() ? 1 : 0, 2, 0, sequencer.RecordArmed() ? "ARMED" : "OFF");
      info.kind = synth::SynthParameterKind::Toggle;
    }
    else return false;
  }
  else if (Is(id, "sys.volume")) number(host_.MasterVolume(), 0, 100, 85, std::to_string(host_.MasterVolume()));
  else if (Is(id, "sys.led")) number(host_.LedBrightness(), 0, 15, 15, std::to_string(host_.LedBrightness()));
  else if (Is(id, "sys.screen")) number(host_.Backlight(), 10, 255, 180, std::to_string(host_.Backlight() * 100 / 255) + "%");
  else return false;
  return true;
}

void Controller::AdjustSetting(const char* id, int delta)
{
  synth::SynthParameterInfo info;
  std::string text;
  if (delta == 0 || !SettingValue(id, info, text)) return;
  // Wide ranges move in useful steps: the screen in 5s (of 255), the volume in 2s.
  if (Is(id, "sys.screen")) delta *= 5;
  else if (Is(id, "sys.volume")) delta *= 2;
  const int value = Is(id, "keys.root") ? ((info.value + delta) % 12 + 12) % 12 : std::clamp(info.value + delta, info.minValue, info.maxValue);
  if (value == info.value) return;
  KeyboardSettings& keys = state_.keyboard;
  auto& sequencer = host_.Sequencer();
  synth::ArpSettings arp = sequencer.GetArpSettings();
  if (Is(id, "keys.layout")) keys.layout = static_cast<KeyboardLayout>(value);
  else if (Is(id, "keys.scale")) keys.scale = static_cast<std::size_t>(value);
  else if (Is(id, "keys.root")) keys.root = value;
  else if (Is(id, "keys.octave")) keys.octave = value;
  else if (Is(id, "keys.chord")) keys.chord = static_cast<ChordType>(value);
  else if (Is(id, "keys.voicing")) keys.voicing = static_cast<std::uint8_t>(value);
  else if (Is(id, "keys.lock")) keys.lock = static_cast<ScaleLock>(value);
  else if (Is(id, "keys.velocity")) keys.velocity = static_cast<std::uint8_t>(value);
  else if (StartsWith(id, "arp."))
  {
    if (Is(id, "arp.mode")) arp.mode = static_cast<synth::ArpMode>(value);
    else if (Is(id, "arp.rate")) arp.rate = static_cast<std::uint8_t>(value);
    else if (Is(id, "arp.octaves")) arp.octaves = static_cast<std::uint8_t>(value);
    else if (Is(id, "arp.gate")) arp.gate = static_cast<std::uint8_t>(value);
    else if (Is(id, "arp.latch")) arp.latch = value != 0;
    sequencer.SetArpSettings(arp);
  }
  else if (Is(id, "proj.bpm")) sequencer.SetTempo(static_cast<std::uint16_t>(value));
  else if (Is(id, "proj.swing")) sequencer.SetSwing(static_cast<std::uint8_t>(value));
  else if (Is(id, "proj.mode")) sequencer.SetSongMode(value != 0);
  else if (Is(id, "proj.steps")) sequencer.SetTrackStepCount(state_.track, static_cast<std::size_t>(value));
  else if (Is(id, "proj.rec")) sequencer.SetRecordArmed(value != 0);
  else if (Is(id, "sys.volume")) host_.SetMasterVolume(static_cast<std::uint8_t>(value));
  else if (Is(id, "sys.led")) host_.SetLedBrightness(static_cast<std::uint8_t>(value));
  else if (Is(id, "sys.screen")) host_.SetBacklight(static_cast<std::uint8_t>(value));
  // The key range moves under held pads: let them go before they map to other notes.
  if (Is(id, "keys.layout") || Is(id, "keys.octave")) ReleaseAllPads();
  // Only project data makes the project unsaved; the rest are app and device settings.
  if (Is(id, "proj.bpm") || Is(id, "proj.swing") || Is(id, "proj.steps")) MarkDirty();
}

void Controller::StepSetting(const char* id)
{
  synth::SynthParameterInfo info;
  std::string text;
  if (!SettingValue(id, info, text) || info.kind == synth::SynthParameterKind::Continuous) return;
  AdjustSetting(id, info.value >= info.maxValue ? info.minValue - info.value : 1);
}

void Controller::ResetSetting(const char* id)
{
  synth::SynthParameterInfo info;
  std::string text;
  if (!SettingValue(id, info, text)) return;
  AdjustSetting(id, info.defaultValue - info.value);
}

void Controller::ShowHud(const std::string& label, const std::string& value, float fraction, bool bipolar)
{
  state_.hud.label = label;
  state_.hud.value = value;
  state_.hud.fraction = fraction;
  state_.hud.bipolar = bipolar;
  state_.hud.at = state_.now == 0 ? 1 : state_.now;
}

void Controller::ShowSettingHud(const char* id, const char* label)
{
  synth::SynthParameterInfo info;
  std::string text;
  if (!SettingValue(id, info, text)) return;
  const float span = static_cast<float>(std::max(1, info.maxValue - info.minValue));
  const bool bipolar = info.minValue < 0;
  const float fraction = bipolar ? static_cast<float>(info.value) / static_cast<float>(info.maxValue)
                                 : static_cast<float>(info.value - info.minValue) / span;
  ShowHud(label, text, fraction, bipolar);
}

// ---------------------------------------------------------------------------------------------
// FN panel. Held FN shows it for as long as FN is down; a tap on FN keeps it open as SETUP (a
// modal). Both show the same five pages and remember the page and each page's focused setting:
// FN + 1..5 or CTRL picks the page, the GRID picks a setting, ADD / SUB change it, ENTER steps a
// choice (SETUP), Fn + ENTER restores the default (SETUP). Pads keep playing, so every change can be
// heard at once.

void Controller::SetFnPage(std::size_t page)
{
  if (page >= kSetupPageCount) return;
  state_.fnPage = static_cast<std::uint8_t>(page);
}

void Controller::OpenSetup()
{
  Modal modal;
  modal.kind = ModalKind::Setup;
  modal.title = "SETUP";
  OpenModal(std::move(modal));
}

const InstrumentPage& Controller::SetupPageShown() const
{
  return SetupPage(state_.fnPage);
}

void Controller::SetupGrid(std::size_t index)
{
  if (index >= kGridCount) return;
  const Slot& slot = SetupPageShown().slots[index];
  if (!slot.IsParameter()) return;
  auto& focus = state_.fnFocus[state_.fnPage % kSetupPageCount];
  // A second press steps a choice or flips a switch, like ENTER.
  if (focus == index) StepSetting(slot.id);
  focus = static_cast<std::uint8_t>(index);
}

void Controller::SetupAdjust(int delta)
{
  const Slot& slot = SetupPageShown().slots[FnFocus()];
  if (slot.IsParameter()) AdjustSetting(slot.id, delta);
}

void Controller::SetupEnter()
{
  const Slot& slot = SetupPageShown().slots[FnFocus()];
  if (slot.IsParameter()) StepSetting(slot.id);
}

void Controller::SetupReset()
{
  const Slot& slot = SetupPageShown().slots[FnFocus()];
  if (!slot.IsParameter()) return;
  ResetSetting(slot.id);
  ShowSettingHud(slot.id, slot.label);
}

// ---------------------------------------------------------------------------------------------
// PROJECT// ---------------------------------------------------------------------------------------------
// PROJECT

void Controller::ProjectGrid(std::size_t index)
{
  const std::size_t page = CurrentPage();
  if (page == kTapeProject)
  {
    switch (index)
    {
      case kProjectSave: SaveProject(false); break;
      case kProjectSaveAs: SaveProject(true); break;
      case kProjectLoad: SetPage(kTapeFiles); break;
      case kProjectNew:
        if (state_.dirty) OpenConfirm(ConfirmPurpose::NewProject, "NEW PROJECT?", "UNSAVED CHANGES WILL BE LOST", true);
        else NewProject();
        break;
      case kProjectBpm:
      case kProjectSwing:
        state_.projectFocus = static_cast<std::uint8_t>(index);
        break;
      case kProjectTap:
        state_.projectFocus = static_cast<std::uint8_t>(index);
        TapTempo();
        break;
      default: break;
    }
  }
}

void Controller::ProjectAdjust(int delta, bool coarse)
{
  auto& sequencer = host_.Sequencer();
  switch (CurrentPage())
  {
    case kTapeProject:
    {
      const auto ui = sequencer.BuildUiState(state_.track);
      if (state_.projectFocus == kProjectBpm)
      {
        sequencer.SetTempo(static_cast<std::uint16_t>(std::clamp(static_cast<int>(ui.bpm) + delta * (coarse ? 10 : 1), 30, 300)));
        MarkDirty();
      }
      else if (state_.projectFocus == kProjectSwing)
      {
        sequencer.SetSwing(static_cast<std::uint8_t>(std::clamp(static_cast<int>(ui.swing) + delta * (coarse ? 5 : 1), 50, 75)));
        MarkDirty();
      }
      break;
    }
    case kTapeFiles:
    {
      if (state_.projects.empty()) break;
      const int count = static_cast<int>(state_.projects.size());
      const int next = std::clamp(static_cast<int>(state_.fileCursor) + delta * (coarse ? 8 : 1), 0, count - 1);
      state_.fileCursor = static_cast<std::size_t>(next);
      break;
    }
    default: break;
  }
}

void Controller::ProjectEnter()
{
  const std::size_t page = CurrentPage();
  if (page == kTapeProject && state_.projectFocus == kProjectTap) TapTempo();
  else if (page == kTapeProject) SaveProject(false);
  else if (page == kTapeFiles && state_.fileCursor < state_.projects.size())
  {
    const std::string& name = state_.projects[state_.fileCursor];
    if (state_.dirty) OpenConfirm(ConfirmPurpose::LoadProject, "LOAD " + name + "?", "UNSAVED CHANGES WILL BE LOST", true, name);
    else LoadProjectNamed(name);
  }
}

void Controller::SaveProject(bool saveAs)
{
  if (saveAs || state_.projectName.empty())
  {
    OpenTextEntry(TextPurpose::ProjectSaveAs, "SAVE PROJECT AS", state_.projectName.empty() ? "SONG" : state_.projectName, 24);
    return;
  }
  SaveProjectNamed(state_.projectName, true);
}

void Controller::SaveProjectNamed(const std::string& name, bool overwrite)
{
  std::string message;
  switch (host_.SaveProject(name, overwrite, Settings(), message))
  {
    case SaveResult::Ok:
      state_.projectName = name;
      state_.dirty = false;
      ShowToast("SAVED " + name, ToastKind::Ok);
      break;
    case SaveResult::Exists:
      OpenConfirm(ConfirmPurpose::OverwriteProject, "OVERWRITE " + name + "?", "THE SAVED PROJECT WILL BE REPLACED", true, name);
      break;
    case SaveResult::Failed:
    default:
      ShowToast(message.empty() ? "SAVE FAILED" : message, ToastKind::Error);
      break;
  }
}

void Controller::LoadProjectNamed(const std::string& name)
{
  ProjectUiSettings settings;
  std::string message;
  ReleaseAllPads();
  if (!host_.LoadProject(name, settings, message))
  {
    ShowToast(message.empty() ? "LOAD FAILED" : message, ToastKind::Error);
    return;
  }
  Restore(settings, name);
  ShowToast("LOADED " + name, ToastKind::Ok);
}

void Controller::NewProject()
{
  ProjectUiSettings settings;
  std::string message;
  ReleaseAllPads();
  if (!host_.NewProject(settings, message))
  {
    ShowToast(message.empty() ? "NEW FAILED" : message, ToastKind::Error);
    return;
  }
  Restore(settings, "");
  ShowToast("NEW PROJECT", ToastKind::Ok);
}

void Controller::RefreshProjects()
{
  state_.projects = host_.ListProjects();
  std::sort(state_.projects.begin(), state_.projects.end());
  const auto current = std::find(state_.projects.begin(), state_.projects.end(), state_.projectName);
  if (current != state_.projects.end()) state_.fileCursor = static_cast<std::size_t>(current - state_.projects.begin());
  if (state_.fileCursor >= state_.projects.size()) state_.fileCursor = state_.projects.empty() ? 0 : state_.projects.size() - 1;
}

// ---------------------------------------------------------------------------------------------
// INSTR

void Controller::InstrumentGrid(std::size_t index)
{
  const InstrumentPage* page = CurrentInstrumentPage();
  const Slot& slot = page->slots[index];
  if (slot.Empty()) return;
  if (slot.IsAction())
  {
    RunSlotAction(slot.action);
    return;
  }
  auto& focus = state_.instrumentFocus[std::min<std::size_t>(CurrentPage(), state_.instrumentFocus.size() - 1)];
  synth::SynthParameterInfo info;
  std::string text;
  // A second press on a focused switch flips it and steps a choice (wrapping), like ENTER.
  if (focus == index && SlotValue(slot, info, text) && info.kind != synth::SynthParameterKind::Continuous)
  {
    InstrumentEnter();
    return;
  }
  focus = static_cast<std::uint8_t>(index);
}

void Controller::InstrumentAdjust(int delta, bool coarse)
{
  const InstrumentPage* page = CurrentInstrumentPage();
  if (page->graphic == PageGraphic::Voice)
  {
    StepPreset(delta * (coarse ? 10 : 1));
    return;
  }
  const Slot& slot = page->slots[InstrumentFocus()];
  if (!slot.IsParameter()) return;
  if (IsSetting(slot.id))
  {
    AdjustSetting(slot.id, delta * (coarse ? 3 : 1));
    return;
  }
  synth::ISynthEngine* engine = ActiveEngine();
  synth::SynthParameterInfo info;
  if (engine == nullptr || !engine->GetParameterInfo(slot.id, info)) return;
  int scaled = delta;
  if (coarse) scaled *= info.maxValue - info.minValue >= 40 ? 10 : 3;
  if (engine->AdjustParameterValue(slot.id, scaled)) MarkDirty();
}

void Controller::InstrumentEnter()
{
  const InstrumentPage* page = CurrentInstrumentPage();
  if (page->graphic == PageGraphic::Voice)
  {
    OpenPresetBrowser();
    return;
  }
  const Slot& slot = page->slots[InstrumentFocus()];
  synth::SynthParameterInfo info;
  std::string text;
  if (!SlotValue(slot, info, text)) return;
  // ENTER steps switches and choices; continuous values stay on SUB / ADD.
  if (info.kind == synth::SynthParameterKind::Toggle) InstrumentAdjust(info.value == info.maxValue ? -1 : 1, false);
  else if (info.kind == synth::SynthParameterKind::Choice)
  {
    if (info.value >= info.maxValue)
    {
      if (IsSetting(slot.id)) StepSetting(slot.id);
      else if (synth::ISynthEngine* engine = ActiveEngine(); engine != nullptr && engine->SetParameterValue(slot.id, info.minValue)) MarkDirty();
    }
    else InstrumentAdjust(1, false);
  }
}

void Controller::InstrumentReset()
{
  const InstrumentPage* page = CurrentInstrumentPage();
  const Slot& slot = page->slots[InstrumentFocus()];
  if (!slot.IsParameter()) return;
  if (IsSetting(slot.id))
  {
    ResetSetting(slot.id);
    return;
  }
  synth::ISynthEngine* engine = ActiveEngine();
  synth::SynthParameterInfo info;
  if (engine != nullptr && engine->GetParameterInfo(slot.id, info) && engine->SetParameterValue(slot.id, info.defaultValue))
  {
    MarkDirty();
    ShowToast(std::string(slot.label) + " DEFAULT");
  }
}

void Controller::StepPreset(int delta)
{
  synth::ISynthEngine* engine = ActiveEngine();
  if (engine == nullptr || delta == 0) return;
  constexpr std::size_t kPage = 32;
  std::size_t total = 0;
  std::size_t current = 0;
  bool found = false;
  for (std::size_t offset = 0;; offset += kPage)
  {
    const auto page = engine->ListPresets({offset, kPage});
    total = page.totalCount;
    for (std::size_t index = 0; index < page.items.size() && !found; ++index)
      if (page.items[index].active) { current = offset + index; found = true; }
    if (found || page.items.empty() || offset + kPage >= total) break;
  }
  if (total == 0) return;
  const int next = (static_cast<int>(current) + delta % static_cast<int>(total) + static_cast<int>(total)) % static_cast<int>(total);
  const auto target = engine->ListPresets({static_cast<std::size_t>(next), 1});
  if (target.items.empty()) return;
  ReleaseAllPads();
  if (engine->LoadPreset(target.items.front().id)) MarkDirty();
}

void Controller::RunSlotAction(SlotAction action)
{
  synth::ISynthEngine* engine = ActiveEngine();
  switch (action)
  {
    case SlotAction::Engine: OpenEnginePicker(); break;
    case SlotAction::Preset: OpenPresetBrowser(); break;
    case SlotAction::SavePreset:
    {
      if (engine == nullptr) break;
      const std::string id = engine->CurrentPresetId();
      if (id.rfind("user.", 0) != 0)
      {
        OpenTextEntry(TextPurpose::PresetSaveAs, "SAVE PRESET AS", engine->CurrentPresetName(), 18);
        break;
      }
      if (engine->SavePreset(id, engine->CurrentPresetName())) ShowToast("PRESET SAVED", ToastKind::Ok);
      else ShowToast("PRESET SAVE FAILED", ToastKind::Error);
      break;
    }
    case SlotAction::SavePresetAs:
      if (engine != nullptr) OpenTextEntry(TextPurpose::PresetSaveAs, "SAVE PRESET AS", engine->CurrentPresetName(), 18);
      break;
    case SlotAction::SampleFile: OpenSampleBrowser(); break;
    case SlotAction::SampleRecord:
    {
      const auto sampler = host_.Sampler().BuildUiState(state_.track);
      if (sampler.hasDraft && !sampler.recording)
        OpenConfirm(ConfirmPurpose::ReplaceTake, "RECORD AGAIN?", "THE UNSAVED TAKE WILL BE REPLACED", true);
      else OpenSampleRecord();
      break;
    }
    case SlotAction::SamplePreview: host_.PreviewSample(state_.track); break;
    case SlotAction::None:
    default: break;
  }
}

// ---------------------------------------------------------------------------------------------
// SEQ

bool Controller::PadForSequence(int note, bool pressed)
{
  if (!pressed) return false;
  auto& sequencer = host_.Sequencer();
  if (!sequencer.BuildUiState(state_.track).hasHeldSteps) return false;
  if (sequencer.ToggleNoteOnSelectedSteps(static_cast<std::uint8_t>(note))) MarkDirty();
  return true;
}

void Controller::SequenceGrid(std::size_t index)
{
  auto& sequencer = host_.Sequencer();
  switch (CurrentPage())
  {
    case 0:
    {
      const auto ui = sequencer.BuildUiState(state_.track);
      std::array<std::uint8_t, kRows * kPadColumns> notes{};
      const std::size_t count = CollectHeldNotes(notes);
      if (state_.record.view == SeqView::Step)
      {
        // STEP REC: a step key moves the write cursor; with pads held it writes them there.
        if (index >= sequencer.TrackStepCount(state_.track)) return;
        state_.record.cursor = static_cast<std::uint8_t>(index);
        if (count > 0)
        {
          (void)StepRecordPress();
          StepRecordRelease();
        }
        return;
      }
      if (!ui.hasHeldSteps && count > 0)
      {
        // Hold pads, tap steps: step input without selecting.
        sequencer.PressStep(index, notes.data(), count);
        sequencer.ReleaseStep(index);
        MarkDirty();
        return;
      }
      sequencer.SetStepSelected(index, !ui.steps[index].held);
      break;
    }
    case 1:
    {
      const auto ui = sequencer.BuildUiState(state_.track);
      const auto& track = ui.tracks[state_.track];
      if (index > track.patternCount) return;
      const bool creating = index == track.patternCount;
      if (sequencer.QueuePattern(state_.track, index))
      {
        if (creating) MarkDirty();
        if (sequencer.Playing() && index != track.activePattern) ShowToast("NEXT " + PatternLabel(index));
      }
      break;
    }
    case 2:
      if (index == kTrackSeq)
      {
        sequencer.SetTrackEnabled(state_.track, !sequencer.TrackEnabled(state_.track));
        MarkDirty();
      }
      if (index <= kTrackBpm) state_.seqTrackFocus = static_cast<std::uint8_t>(index);
      break;
    default: break;
  }
}

void Controller::SequenceGridRelease(std::size_t index)
{
  (void)index;
}

void Controller::SequenceAdjust(int delta, bool coarse)
{
  auto& sequencer = host_.Sequencer();
  const auto ui = sequencer.BuildUiState(state_.track);
  switch (CurrentPage())
  {
    case 0:
      if (ui.hasHeldSteps)
      {
        if (sequencer.AdjustSelectedSteps(delta, coarse)) MarkDirty();
        return;
      }
      if (state_.record.view == SeqView::Step)
      {
        StepCursorMove(delta * (coarse ? 4 : 1));
        return;
      }
      [[fallthrough]];
    case 1:
    {
      const auto& track = ui.tracks[state_.track];
      const std::size_t base = track.queuedPattern < synth::kSequencerMaxPatternCount ? track.queuedPattern : track.activePattern;
      const int count = static_cast<int>(track.patternCount);
      const int next = (static_cast<int>(base) + (delta > 0 ? 1 : -1) + count) % count;
      sequencer.QueuePattern(state_.track, static_cast<std::size_t>(next));
      break;
    }
    case 2:
      switch (state_.seqTrackFocus)
      {
        case kTrackSeq:
          sequencer.SetTrackEnabled(state_.track, delta > 0);
          break;
        case kTrackSteps:
          sequencer.SetTrackStepCount(state_.track, static_cast<std::size_t>(std::clamp(
              static_cast<int>(sequencer.TrackStepCount(state_.track)) + delta * (coarse ? 4 : 1), 1, 16)));
          break;
        case kTrackSwing:
          sequencer.SetSwing(static_cast<std::uint8_t>(std::clamp(static_cast<int>(ui.swing) + delta * (coarse ? 5 : 1), 50, 75)));
          break;
        case kTrackBpm:
        default:
          sequencer.SetTempo(static_cast<std::uint16_t>(std::clamp(static_cast<int>(ui.bpm) + delta * (coarse ? 10 : 1), 30, 300)));
          break;
      }
      MarkDirty();
      break;
    default: break;
  }
}

void Controller::SequenceEnter()
{
  auto& sequencer = host_.Sequencer();
  switch (CurrentPage())
  {
    case 0:
      if (sequencer.BuildUiState(state_.track).hasHeldSteps) sequencer.CycleEditLane();
      break;
    case 1:
      SetPage(0);
      break;
    case 2:
      if (state_.seqTrackFocus == kTrackSeq)
      {
        sequencer.SetTrackEnabled(state_.track, !sequencer.TrackEnabled(state_.track));
        MarkDirty();
      }
      break;
    default: break;
  }
}

// ---------------------------------------------------------------------------------------------
// TAPE: song arrangement. GRID columns are four consecutive song rows (left to right in time),
// GRID rows are the four tracks.

void Controller::SongGrid(std::size_t index)
{
  auto& sequencer = host_.Sequencer();
  const std::size_t track = index / kGridColumns;
  const std::size_t row = state_.songScroll + index % kGridColumns;
  const std::size_t length = sequencer.SongLength();
  if (row > length || row >= synth::kSongMaxRows) return;
  if (row == length)
  {
    // The first empty column appends a row: a copy of the last row, or every track on its active pattern.
    synth::SongRow fresh{};
    if (length > 0) fresh = sequencer.SongRowAt(length - 1);
    else
      for (std::size_t t = 0; t < kTrackCount; ++t) fresh.patterns[t] = static_cast<std::int8_t>(sequencer.ActivePattern(t));
    if (!sequencer.InsertSongRow(length, fresh)) return;
    MarkDirty();
    ShowToast("ROW " + std::to_string(row + 1) + " ADDED");
  }
  // A second tap on the focused cell switches between editing its pattern and the row length.
  if (row == state_.songRow && track == state_.songTrack && row < length)
  {
    state_.songLane = static_cast<std::uint8_t>(state_.songLane ^ 1u);
    ShowHud("EDIT", state_.songLane == 0 ? "PATTERN" : "BARS");
  }
  state_.songRow = row;
  state_.songTrack = track;
}

void Controller::SongAdjust(int delta, bool coarse)
{
  auto& sequencer = host_.Sequencer();
  if (state_.songRow >= sequencer.SongLength()) return;
  const auto row = sequencer.SongRowAt(state_.songRow);
  if (coarse || state_.songLane == 1)
  {
    // The BARS lane changes how many bars the row lasts.
    const int repeats = std::clamp(static_cast<int>(row.repeats) + (delta > 0 ? 1 : -1), 1, 16);
    if (sequencer.SetSongRepeats(state_.songRow, static_cast<std::uint8_t>(repeats))) MarkDirty();
    return;
  }
  // Step through "silent" (-1) and the track's patterns.
  const int count = static_cast<int>(sequencer.PatternCount(state_.songTrack));
  const int current = row.patterns[state_.songTrack];
  const int next = ((current + 1 + delta) % (count + 1) + count + 1) % (count + 1) - 1;
  if (sequencer.SetSongPattern(state_.songRow, state_.songTrack, next)) MarkDirty();
}

void Controller::SongEnter()
{
  auto& sequencer = host_.Sequencer();
  if (state_.songRow >= sequencer.SongLength()) return;
  sequencer.CueSongRow(state_.songRow);
  if (!sequencer.SongMode())
  {
    sequencer.SetSongMode(true);
    ShowToast("SONG MODE");
  }
  else if (sequencer.Playing()) ShowToast("JUMP TO ROW " + std::to_string(state_.songRow + 1));
  else ShowToast("START AT ROW " + std::to_string(state_.songRow + 1));
}

void Controller::SongFnEnter()
{
  auto& sequencer = host_.Sequencer();
  const bool song = !sequencer.SongMode();
  sequencer.SetSongMode(song);
  ShowToast(song ? "SONG MODE" : "LOOP MODE");
}

void Controller::SongArrow(int horizontal, int vertical)
{
  const std::size_t length = host_.Sequencer().SongLength();
  if (horizontal != 0)
  {
    const int next = std::clamp(static_cast<int>(state_.songRow) + horizontal, 0, static_cast<int>(length));
    state_.songRow = static_cast<std::size_t>(next);
  }
  if (vertical != 0)
    state_.songTrack = static_cast<std::size_t>((static_cast<int>(state_.songTrack) + vertical + static_cast<int>(kTrackCount)) % static_cast<int>(kTrackCount));
  // Keep the cursor inside the four visible columns.
  if (state_.songRow < state_.songScroll) state_.songScroll = state_.songRow;
  if (state_.songRow >= state_.songScroll + kGridColumns) state_.songScroll = state_.songRow - kGridColumns + 1;
}

// ---------------------------------------------------------------------------------------------
// SESSION: a clip grid. GRID columns are the tracks (as on MIX), rows are pattern slots from the
// scroll position down. A tap launches that pattern on that track: at once when stopped, at the
// track's loop end while playing. ENTER launches the whole row (a scene), Fn + ENTER captures what
// plays now as a new SONG row, ADD / SUB move through the patterns.

void Controller::SessionGrid(std::size_t index)
{
  if (index >= kGridCount) return;
  auto& sequencer = host_.Sequencer();
  const std::size_t track = index % kGridColumns;
  const std::size_t pattern = state_.sessionScroll + index / kGridColumns;
  state_.sessionTrack = track;
  state_.sessionPattern = std::min(pattern, synth::kSequencerMaxPatternCount - 1);
  const std::size_t count = sequencer.PatternCount(track);
  if (pattern > count || pattern >= synth::kSequencerMaxPatternCount) return;
  // Launching takes the track out of the arrangement, like playing a clip in a session.
  if (sequencer.SongMode())
  {
    sequencer.SetSongMode(false);
    ShowHud("SESSION", "LOOP MODE");
  }
  const bool creating = pattern == count;
  if (!sequencer.QueuePattern(track, pattern)) return;
  if (creating) MarkDirty();
  SelectTrack(track);
  ShowHud("T" + std::to_string(track + 1) + (creating ? " NEW" : sequencer.Playing() ? " NEXT" : ""), PatternLabel(pattern));
}

void Controller::SessionAdjust(int delta)
{
  const int last = static_cast<int>(synth::kSequencerMaxPatternCount) - 1;
  state_.sessionPattern = static_cast<std::size_t>(std::clamp(static_cast<int>(state_.sessionPattern) + delta, 0, last));
  // Keep the cursor row on screen.
  if (state_.sessionPattern < state_.sessionScroll) state_.sessionScroll = state_.sessionPattern;
  if (state_.sessionPattern >= state_.sessionScroll + kRows) state_.sessionScroll = state_.sessionPattern - (kRows - 1);
}

void Controller::SessionLaunchRow()
{
  auto& sequencer = host_.Sequencer();
  if (sequencer.SongMode()) sequencer.SetSongMode(false);
  std::size_t launched = 0;
  for (std::size_t track = 0; track < kTrackCount; ++track)
    if (state_.sessionPattern < sequencer.PatternCount(track) && sequencer.QueuePattern(track, state_.sessionPattern)) ++launched;
  ShowHud(sequencer.Playing() ? "NEXT ROW" : "ROW", PatternLabel(state_.sessionPattern) + "  " + std::to_string(launched) + "/4 TRACKS");
}

void Controller::SessionCapture()
{
  // What every track plays right now (or has queued) becomes the next SONG row.
  auto& sequencer = host_.Sequencer();
  const auto ui = sequencer.BuildUiState(state_.track);
  synth::SongRow row{};
  for (std::size_t track = 0; track < kTrackCount; ++track)
  {
    const auto& summary = ui.tracks[track];
    const std::size_t pattern = summary.queuedPattern < summary.patternCount ? summary.queuedPattern : summary.activePattern;
    row.patterns[track] = static_cast<std::int8_t>(pattern);
  }
  const std::size_t at = sequencer.SongLength();
  if (!sequencer.InsertSongRow(at, row))
  {
    ShowHud("SONG", "FULL", 1.0f);
    return;
  }
  MarkDirty();
  ShowHud("CAPTURED", "SONG ROW " + std::to_string(at + 1));
}

// ---------------------------------------------------------------------------------------------
// MIX

void Controller::MixGrid(std::size_t index)
{
  const std::size_t row = index / kGridColumns;
  const std::size_t track = index % kGridColumns;
  auto& sequencer = host_.Sequencer();
  switch (row)
  {
    case 0:
      state_.mixFocus = static_cast<std::uint8_t>(track);
      SelectTrack(track);
      break;
    case 1:
      ToggleMute(track);
      break;
    case 2:
      host_.SetTrackSoloed(track, !host_.TrackSoloed(track));
      MarkDirty();
      break;
    case 3:
      sequencer.SetTrackEnabled(track, !sequencer.TrackEnabled(track));
      MarkDirty();
      break;
    default: break;
  }
}

void Controller::MixAdjust(int delta, bool coarse)
{
  const int step = delta * (coarse ? 10 : 1);
  if (state_.mixFocus >= kTrackCount)
  {
    host_.SetMasterVolume(static_cast<std::uint8_t>(std::clamp(static_cast<int>(host_.MasterVolume()) + step, 0, 100)));
    return;
  }
  const std::size_t track = state_.mixFocus;
  host_.SetTrackLevel(track, static_cast<std::uint8_t>(std::clamp(static_cast<int>(host_.TrackLevel(track)) + step, 0, 100)));
  MarkDirty();
}

void Controller::MixEnter()
{
  state_.mixFocus = state_.mixFocus >= kTrackCount ? static_cast<std::uint8_t>(state_.track) : static_cast<std::uint8_t>(kTrackCount);
}

// ---------------------------------------------------------------------------------------------
// SENDS: GRID rows are the track effects (filter, chorus, delay send, reverb send), columns the
// tracks. The values are the "fx.*" parameters of each track's engine.

void Controller::SetSendValue(std::size_t track, const char* id, int value)
{
  synth::ISynthEngine* engine = host_.Engine(track);
  if (engine != nullptr && engine->SetParameterValue(id, value)) MarkDirty();
}

void Controller::SendsGrid(std::size_t index)
{
  if (index >= kGridCount) return;
  const std::size_t track = index % kGridColumns;
  // A second press on a focused chorus cell steps its mode, like a switch on an INSTR page.
  if (state_.sendsFocus == index && index / kGridColumns == 1)
  {
    SendsEnter();
    return;
  }
  state_.sendsFocus = static_cast<std::uint8_t>(index);
  SelectTrack(track);
}

void Controller::SendsAdjust(int delta, bool coarse)
{
  const std::size_t track = state_.sendsFocus % kGridColumns;
  const char* id = kSendIds[state_.sendsFocus / kGridColumns];
  synth::ISynthEngine* engine = host_.Engine(track);
  synth::SynthParameterInfo info;
  if (engine == nullptr || !engine->GetParameterInfo(id, info)) return;
  const int scale = coarse && info.kind == synth::SynthParameterKind::Continuous ? 10 : 1;
  if (engine->AdjustParameterValue(id, delta * scale)) MarkDirty();
}

void Controller::SendsEnter()
{
  // ENTER steps the chorus mode and switches a filter or send off and back on.
  const std::size_t track = state_.sendsFocus % kGridColumns;
  const std::size_t row = state_.sendsFocus / kGridColumns;
  const char* id = kSendIds[row];
  synth::ISynthEngine* engine = host_.Engine(track);
  synth::SynthParameterInfo info;
  if (engine == nullptr || !engine->GetParameterInfo(id, info)) return;
  if (info.kind == synth::SynthParameterKind::Choice)
  {
    SetSendValue(track, id, info.value >= info.maxValue ? info.minValue : info.value + 1);
    return;
  }
  // The memory holds value + 1; 0 means nothing was switched off yet.
  auto& memory = state_.sendsMemory[state_.sendsFocus];
  if (info.value != info.defaultValue)
  {
    memory = static_cast<std::uint8_t>(info.value + 1);
    SetSendValue(track, id, info.defaultValue);
  }
  else if (memory != 0) SetSendValue(track, id, memory - 1);
}

void Controller::SendsReset()
{
  const std::size_t track = state_.sendsFocus % kGridColumns;
  const char* id = kSendIds[state_.sendsFocus / kGridColumns];
  synth::ISynthEngine* engine = host_.Engine(track);
  synth::SynthParameterInfo info;
  if (engine == nullptr || !engine->GetParameterInfo(id, info)) return;
  SetSendValue(track, id, info.defaultValue);
  ShowToast("T" + std::to_string(track + 1) + " " + kSendTags[state_.sendsFocus / kGridColumns] + " DEFAULT");
}

// ---------------------------------------------------------------------------------------------
// FX: the master bus. GRID rows: delay, reverb, master; values are project settings.

void Controller::FxGrid(std::size_t index)
{
  if (index < synth::fx::kMasterParamCount) state_.fxFocus = static_cast<std::uint8_t>(index);
}

void Controller::FxAdjust(int delta, bool coarse)
{
  const std::size_t param = state_.fxFocus;
  if (param >= synth::fx::kMasterParamCount) return;
  const auto& info = synth::fx::kMasterParams[param];
  const int scale = coarse && info.maxValue - info.minValue >= 40 ? 10 : 1;
  const int value = std::clamp(host_.MasterFx(param) + delta * scale, info.minValue, info.maxValue);
  if (value == host_.MasterFx(param)) return;
  host_.SetMasterFx(param, value);
  MarkDirty();
}

void Controller::FxEnter()
{
  // The delay time is a list of note divisions; ENTER walks it.
  if (state_.fxFocus != static_cast<std::uint8_t>(synth::fx::MasterParam::DelayTime)) return;
  const auto& info = synth::fx::kMasterParams[state_.fxFocus];
  const int value = host_.MasterFx(state_.fxFocus);
  host_.SetMasterFx(state_.fxFocus, value >= info.maxValue ? info.minValue : value + 1);
  MarkDirty();
}

void Controller::FxReset()
{
  if (state_.fxFocus >= synth::fx::kMasterParamCount) return;
  host_.SetMasterFx(state_.fxFocus, synth::fx::kMasterParams[state_.fxFocus].defaultValue);
  MarkDirty();
  ShowToast(std::string(synth::fx::kMasterParams[state_.fxFocus].label) + " DEFAULT");
}

// ---------------------------------------------------------------------------------------------
// Fn functions

bool Controller::SongPage() const
{
  return state_.scene == Scene::Tape && CurrentPage() == kTapeSong;
}

void Controller::Copy()
{
  if (SongPage())
  {
    auto& sequencer = host_.Sequencer();
    if (state_.songRow >= sequencer.SongLength()) return;
    state_.songClipboard = sequencer.SongRowAt(state_.songRow);
    state_.songClipboardValid = true;
    ShowToast("COPIED ROW " + std::to_string(state_.songRow + 1));
    return;
  }
  if (state_.scene != Scene::Sequence)
  {
    ShowToast("COPY WORKS IN SEQ AND TAPE");
    return;
  }
  const auto ui = host_.Sequencer().BuildUiState(state_.track);
  if (CurrentPage() == 0 && ui.hasHeldSteps)
  {
    state_.clipboardIsPattern = false;
    state_.clipboardIndex = ui.primaryHeldStep;
    state_.clipboardValid = true;
    ShowToast("COPIED STEP " + std::to_string(ui.primaryHeldStep + 1));
    return;
  }
  state_.clipboardIsPattern = true;
  state_.clipboardIndex = ui.tracks[state_.track].activePattern;
  state_.clipboardValid = true;
  ShowToast("COPIED " + PatternLabel(state_.clipboardIndex));
}

void Controller::Paste()
{
  if (SongPage())
  {
    auto& sequencer = host_.Sequencer();
    if (!state_.songClipboardValid) return;
    // Paste inserts the copied row after the cursor.
    const std::size_t at = std::min(state_.songRow + 1, sequencer.SongLength());
    if (sequencer.InsertSongRow(at, state_.songClipboard))
    {
      state_.songRow = at;
      SongArrow(0, 0);
      MarkDirty();
      ShowToast("PASTED ROW " + std::to_string(at + 1), ToastKind::Ok);
    }
    return;
  }
  if (state_.scene != Scene::Sequence || !state_.clipboardValid)
  {
    ShowToast("NOTHING TO PASTE");
    return;
  }
  auto& sequencer = host_.Sequencer();
  const auto ui = sequencer.BuildUiState(state_.track);
  if (!state_.clipboardIsPattern)
  {
    if (!ui.hasHeldSteps)
    {
      ShowToast("SELECT STEPS TO PASTE");
      return;
    }
    for (std::size_t step = 0; step < kGridCount; ++step)
      if (ui.steps[step].held && step != state_.clipboardIndex) (void)sequencer.CopyStep(state_.clipboardIndex, step);
    MarkDirty();
    ShowToast("PASTED", ToastKind::Ok);
    return;
  }
  const std::size_t destination = ui.tracks[state_.track].activePattern;
  if (destination == state_.clipboardIndex) return;
  if (sequencer.CopyPattern(state_.clipboardIndex, destination))
  {
    MarkDirty();
    ShowToast("PASTED TO " + PatternLabel(destination), ToastKind::Ok);
  }
}

void Controller::Clear()
{
  if (SongPage())
  {
    auto& sequencer = host_.Sequencer();
    if (state_.songRow >= sequencer.SongLength()) return;
    if (sequencer.DeleteSongRow(state_.songRow))
    {
      MarkDirty();
      ShowToast("ROW " + std::to_string(state_.songRow + 1) + " DELETED");
      state_.songRow = std::min(state_.songRow, sequencer.SongLength());
      SongArrow(0, 0);
    }
    return;
  }
  if (state_.scene != Scene::Sequence) return;
  auto& sequencer = host_.Sequencer();
  const auto ui = sequencer.BuildUiState(state_.track);
  if (CurrentPage() == 0 && ui.hasHeldSteps)
  {
    sequencer.ClearHeldStepEvents();
    MarkDirty();
    ShowToast("STEPS CLEARED");
    return;
  }
  const std::size_t pattern = ui.tracks[state_.track].activePattern;
  if (!ui.patternHasEvents[pattern]) return;
  OpenConfirm(ConfirmPurpose::ClearPattern, "CLEAR " + PatternLabel(pattern) + "?", "ALL NOTES IN THIS PATTERN", true);
}

void Controller::Quantize()
{
  if (state_.scene != Scene::Sequence) return;
  auto& sequencer = host_.Sequencer();
  const auto ui = sequencer.BuildUiState(state_.track);
  if (CurrentPage() == 0 && ui.hasHeldSteps) sequencer.QuantizeHeldSteps();
  else sequencer.QuantizeActivePattern();
  MarkDirty();
  ShowToast("QUANTIZED");
}

void Controller::Duplicate()
{
  auto& sequencer = host_.Sequencer();
  if (SongPage())
  {
    if (state_.songRow >= sequencer.SongLength()) return;
    if (sequencer.InsertSongRow(state_.songRow + 1, sequencer.SongRowAt(state_.songRow)))
    {
      ++state_.songRow;
      SongArrow(0, 0);
      MarkDirty();
      ShowToast("ROW DUPLICATED", ToastKind::Ok);
    }
    return;
  }
  const auto ui = sequencer.BuildUiState(state_.track);
  const auto& track = ui.tracks[state_.track];
  if (track.patternCount >= synth::kSequencerMaxPatternCount)
  {
    ShowToast("16 PATTERNS MAX", ToastKind::Error);
    return;
  }
  if (sequencer.CopyPattern(track.activePattern, track.patternCount))
  {
    MarkDirty();
    ShowToast("DUPLICATED TO " + PatternLabel(track.patternCount), ToastKind::Ok);
  }
}

void Controller::TapTempo()
{
  const std::uint32_t now = state_.now;
  if (state_.tapCount > 0 && now - state_.taps[(state_.tapCount - 1) % state_.taps.size()] > 2000) state_.tapCount = 0;
  state_.taps[state_.tapCount % state_.taps.size()] = now;
  ++state_.tapCount;
  if (state_.tapCount < 2)
  {
    ShowToast("TAP");
    return;
  }
  const std::size_t samples = std::min(state_.tapCount, state_.taps.size());
  const std::uint32_t first = state_.taps[(state_.tapCount - samples) % state_.taps.size()];
  const std::uint32_t interval = (now - first) / static_cast<std::uint32_t>(samples - 1);
  if (interval == 0) return;
  const int bpm = std::clamp(static_cast<int>((60000 + interval / 2) / interval), 30, 300);
  host_.Sequencer().SetTempo(static_cast<std::uint16_t>(bpm));
  MarkDirty();
  ShowHud("TAP TEMPO", std::to_string(bpm), static_cast<float>(bpm - 30) / 270.0f);
}

void Controller::QuickSave()
{
  SaveProject(false);
}

void Controller::ToggleMute(std::size_t track, bool announce)
{
  if (track >= kTrackCount) return;
  const bool muted = !host_.TrackMuted(track);
  host_.SetTrackMuted(track, muted);
  MarkDirty();
  if (announce) ShowHud("T" + std::to_string(track + 1), muted ? "MUTED" : "ON");
}

// What FN does besides the panel: the letter shortcuts on the pads and the keys around them.
std::vector<Controller::FnEntry> Controller::FnEntries() const
{
  const bool seq = state_.scene == Scene::Sequence;
  const bool instr = state_.scene == Scene::Instrument;
  const bool song = SongPage();
  const bool tapeEdit = state_.scene == Scene::Tape && (CurrentPage() == kTapeSends || CurrentPage() == kTapeFx);
  const bool session = state_.scene == Scene::Tape && CurrentPage() == kTapeSession;
  // In priority order: the FN panel shows as many as fit on its text line.
  return {
      {"6/Y", "BPM+-", true},
      {"7/U", "VOL+-", true},
      {"8/I", "OCT+-", true},
      {"1-5", "PAGE", true},
      {"+-", "ADJUST", true},
      {"C", song ? "COPY ROW" : "COPY", seq || song},
      {"V", "PASTE", seq || song},
      {"X", song ? "DEL ROW" : "CLEAR", seq || song},
      {"D", song ? "DUP ROW" : "DUP", true},
      {"Q", "QUANT", seq},
      {"S", "SAVE", true},
      {"T", "TAP", true},
      {"PLAY", "REC", true},
      {"ESC", "STOP", true},
      {"TAB", "PREV", true},
      {"ENT", instr || tapeEdit ? "DEFAULT" : seq ? "SCOPE" : song ? "SONG/LOOP" : session ? "CAPTURE" : "OPTIONS", instr || seq || song || tapeEdit || session},
  };
}

}  // namespace groove
