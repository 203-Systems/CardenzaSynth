#include "Applications/Synth/SynthApp.h"

#include "Library/Memory/Memory.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <iomanip>
#include <new>
#include <sstream>

#include "Applications/Application.h"
#include "Applications/Synth/Runtime/ProjectStorage.h"
#include "Applications/Synth/UI/SynthUiRenderer.h"
#include "Applications/Synth/UI/SynthLedFeedback.h"
#include "Devices/Device.h"
#include "Library/Synth/Audio/SynthAudioRuntime.h"
#include "OS/MIDI/MIDI.h"
#include "OS/UI/Canvas.h"

namespace apps {

namespace {

constexpr std::size_t KeyIndex(std::size_t row, std::size_t col) {
  return row * synth::kKeyboardMatrixColumns + col;
}

constexpr std::uint8_t kActiveLedBrightness = synth::led::kPressed;
constexpr std::uint8_t kHintLedBrightness = synth::led::kAvailable;
constexpr auto kSequencerShiftTapMax = std::chrono::milliseconds(150);

void WriteActiveBarLeds(const synth::UiScreenSnapshot& snapshot, const synth::SynthUiModel& model) {
  for (std::size_t row = 0; row < synth::kKeyboardMatrixRows; ++row)
  {
    auto item = snapshot.leftBarItems[row];
    const bool context = snapshot.mode == synth::UiMode::ContextMenu;
    if (context)
      item.active = model.IsModifierHighlighted(row);
    else if (!snapshot.sequencer.hasHeldSteps && !snapshot.sequencer.shiftActive && !snapshot.sequencer.patternSelectorVisible)
      item.active = row == snapshot.sequencer.activeTrack;
    const std::uint8_t brightness = context && !item.active && !item.pressed
        ? synth::led::kAvailable : synth::led::BarLevel(item, snapshot);
    if (brightness != Device::LED::kBrightnessOff)
    {
      Device::LED::SetKey(row, synth::kHomeLeftBarColumn, brightness);
    }
  }

  if (!snapshot.rightBarVisible)
  {
    return;
  }

  if (snapshot.mode == synth::UiMode::Home)
  {
    for (std::size_t row = 0; row < synth::kKeyboardMatrixRows; ++row)
    {
      const std::uint8_t brightness = synth::led::BarLevel(snapshot.rightBarItems[row], snapshot);
      if (brightness != Device::LED::kBrightnessOff)
      {
        Device::LED::SetKey(row, synth::kHomeRightBarColumn, brightness);
      }
    }
    return;
  }

  const std::size_t columns = snapshot.rightBarColumns;
  const std::size_t firstColumn = synth::kKeyboardMatrixColumns - columns;
  for (std::size_t row = 0; row < synth::kContextMenuRows; ++row)
  {
    for (std::size_t column = 0; column < columns; ++column)
    {
      const std::size_t index = columns == 1 ? row : row * synth::kContextMenuColumns + column;
      const std::uint8_t brightness = synth::led::BarLevel(snapshot.rightBarItems[index], snapshot);
      if (brightness != Device::LED::kBrightnessOff)
      {
        Device::LED::SetKey(row, firstColumn + column, brightness);
      }
    }
  }
}

std::unique_ptr<app::Application> CreateSynthApplication() {
  return std::make_unique<SynthApp>();
}

app::ApplicationInfo BuildSynthInfo() {
  app::ApplicationInfo info;
  info.id = app::MakeApplicationId("Cardenza", "Synth");
  info.name = "Synth";
  info.author = "Cardenza";
  info.factory = &CreateSynthApplication;
  return info;
}

}  // namespace

void RegisterSynthApplication() {
  static const bool registered = app::RegisterApplication(BuildSynthInfo());
  (void)registered;
}

void SynthApp::Setup(const std::vector<std::string>& args) {
  (void)args;
  if (initialized_)
  {
    return;
  }

  os::midi::Initialize();
  sequencer_.SetMidiSink(&SynthApp::SendSequencerMidi, this);
  rack_.AttachEngine(0, synth::SynthEngineKind::Dx7);
  samplerManager_.OnTrackEngineChanged(0, synth::SynthEngineKind::Dx7);
  ui_.InitializeTrack(0, synth::SynthEngineKind::Dx7);
  rack_.AttachEngine(1, synth::SynthEngineKind::Juno);
  samplerManager_.OnTrackEngineChanged(1, synth::SynthEngineKind::Juno);
  ui_.InitializeTrack(1, synth::SynthEngineKind::Juno);
  rack_.AttachEngine(2, synth::SynthEngineKind::Sampler);
  samplerManager_.OnTrackEngineChanged(2, synth::SynthEngineKind::Sampler);
  ui_.InitializeTrack(2, synth::SynthEngineKind::Sampler);
  rack_.AttachEngine(3, synth::SynthEngineKind::Dx7);
  samplerManager_.OnTrackEngineChanged(3, synth::SynthEngineKind::Dx7);
  ui_.InitializeTrack(3, synth::SynthEngineKind::Dx7);
  ui_.SetActiveTrack(0);
  for (auto& trackNotes : activeNotes_)
  {
    trackNotes.fill(0xFF);
  }
  SyncUiTracks();
  SyncSequencerUi();
  initialized_ = true;
}

void SynthApp::Tick() {
  samplerManager_.Tick();
  sequencer_.SetActiveTrack(ui_.ActiveTrack());
  ui_.Tick(ActiveEngine());
  rack_.DispatchPendingMessages();
  SyncUiTracks();
  SyncSequencerUi();
}

void SynthApp::UpdateLeds() {
  Device::LED::Clear();

  const std::size_t activeTrack = ui_.ActiveTrack();
  const synth::UiScreenSnapshot snapshot = ui_.CurrentScreenSnapshot(ActiveEngine());
  const auto keyboardSettings = ui_.KeyboardSettings();
  const int rootNote = keyboardSettings[3] == static_cast<int>(synth::KeyboardLayoutMode::Piano) ? 0 : keyboardSettings[1];
  if (activeTrack < activeKeys_.size())
  {
    const std::size_t hintLastColumn = snapshot.mode == synth::UiMode::Home
        ? synth::kHomePerformanceLastColumn
        : synth::kHomeRightBarColumn - 1;
    for (std::size_t row = 0; row < synth::kKeyboardMatrixRows; ++row)
    {
      for (std::size_t column = synth::kHomePerformanceFirstColumn; column <= hintLastColumn; ++column)
      {
        if (activeKeys_[activeTrack][KeyIndex(row, column)])
        {
          Device::LED::SetKey(row, column, kActiveLedBrightness);
        }
        else
        {
          const bool mapped = ui_.IsHomePerformanceKey(row, column);
          const bool root = snapshot.engineKind == synth::SynthEngineKind::Drums
              ? column == synth::kHomePerformanceFirstColumn || column == synth::kHomePerformanceFirstColumn + 4
              : mapped && ui_.HomePerformanceMidiNote(row, column) % 12 == rootNote;
          Device::LED::SetKey(row, column, synth::led::KeyboardLevel(snapshot, row, ui_.IsKeyboardHintKeyLit(row, column), root));
        }
      }
    }
  }

  if (snapshot.mode == synth::UiMode::Home && snapshot.sequencer.visible)
  {
    if (snapshot.sequencer.hasHeldSteps && snapshot.sequencer.editValueCount > 0 && !snapshot.sequencer.patternSelectorVisible)
    {
      for (std::size_t column = synth::kHomePerformanceFirstColumn; column <= synth::kHomePerformanceLastColumn; ++column)
      {
        const std::size_t valueIndex = column - synth::kHomePerformanceFirstColumn;
        for (std::size_t row = 0; row < synth::kKeyboardMatrixRows; ++row)
        {
          Device::LED::SetKey(row, column, synth::led::EditValueLevel(snapshot.sequencer, row, valueIndex));
        }
      }
    }

    if (snapshot.sequencer.patternSelectorVisible)
    {
      for (std::size_t row = 0; row < 2; ++row)
      {
        for (std::size_t column = synth::kHomePerformanceFirstColumn; column <= synth::kHomePerformanceLastColumn; ++column)
        {
          const std::size_t patternIndex = row * synth::kHomePerformanceColumnCount + (column - synth::kHomePerformanceFirstColumn);
          std::uint8_t brightness = Device::LED::kBrightnessOff;
          const bool existing = patternIndex < snapshot.sequencer.patternCount;
          const bool addSlot = patternIndex == snapshot.sequencer.patternCount && snapshot.sequencer.patternCount < synth::kSequencerMaxPatternCount;
          if (patternIndex < snapshot.sequencer.patternPressed.size() && snapshot.sequencer.patternPressed[patternIndex])
          {
            brightness = kActiveLedBrightness;
          }
          else if (snapshot.sequencer.hasCopySource && snapshot.sequencer.copySourceIsPattern && snapshot.sequencer.copySourceIndex == patternIndex)
          {
            brightness = synth::led::kSelected;
          }
          else if (existing && patternIndex == snapshot.sequencer.activePattern)
          {
            brightness = synth::led::kSelected;
          }
          else if (existing)
          {
            brightness = snapshot.sequencer.patternHasEvents[patternIndex] ? synth::led::kContent : kHintLedBrightness;
          }
          else if (addSlot)
          {
            brightness = synth::led::kFunction;
          }
          Device::LED::SetKey(row, column, brightness);
        }
      }
    }

    for (std::size_t stepIndex = 0; stepIndex < snapshot.sequencer.steps.size(); ++stepIndex)
    {
      const auto& step = snapshot.sequencer.steps[stepIndex];
      const bool copySource = snapshot.sequencer.hasCopySource && !snapshot.sequencer.copySourceIsPattern &&
                              snapshot.sequencer.copySourceIndex == stepIndex;
      const std::uint8_t brightness = synth::led::StepLevel(step, snapshot.sequencer.selectionLatched, copySource);
      Device::LED::SetKey(synth::SequencerStepRow(stepIndex), synth::SequencerStepColumn(stepIndex), brightness);
    }
  }

  WriteActiveBarLeds(snapshot, ui_);
  Device::LED::SetEnabled(true);
}

void SynthApp::RenderFrame() {
  ui::Canvas canvas(Device::Display::MutableFrameBuffer());
  const std::size_t activeTrack = ui_.ActiveTrack();
  const auto& slot = rack_.Slot(activeTrack);
  synth::RenderSynthUiFrame(canvas, ui_, slot.engine.get(), activeKeys_[activeTrack]);
}

void SynthApp::HandleMatrixKeyEvent(std::size_t row, std::size_t col, bool pressed) {
  if (pressed)
  {
    (void)PressMatrixKey(row, col);
    return;
  }

  ReleaseMatrixKey(row, col);
}

void SynthApp::HandleHomeButtonEvent(bool pressed)
{
  if (!pressed)
  {
    return;
  }

  const bool toggleView = ui_.Mode() == synth::UiMode::Home &&
                          !sequencer_.HasHeldSteps() && !IsSequencerShiftActive() && !sequencerViewChordActive_;
  ui_.HandleHomeButton(ActiveEngine());
  ApplyPendingUiActions();
  ClearSequencerHomeGesture();
  if (toggleView) ui_.ToggleHomeView();
  SyncUiTracks();
  SyncSequencerUi();
}

synth::MatrixKeyHandling SynthApp::PressMatrixKey(std::size_t row, std::size_t col) {
  if (HandleSequencerPress(row, col))
  {
    SyncSequencerUi();
    return synth::MatrixKeyHandling::Ignored;
  }

  const auto handling = ui_.PressMatrixKey(ActiveEngineMutable(), row, col);
  ApplyPendingUiActions();
  SyncUiTracks();
  SyncSequencerUi();
  if (handling == synth::MatrixKeyHandling::PerformanceKey)
  {
    PressPerformanceKey(row, col);
  }
  return handling;
}

void SynthApp::ReleaseMatrixKey(std::size_t row, std::size_t col) {
  if (HandleSequencerRelease(row, col))
  {
    SyncSequencerUi();
    return;
  }

  const std::size_t contextMenuColumns = ui_.ContextMenuColumnCount();
  const std::size_t contextMenuFirstColumn = contextMenuColumns == 1 ? synth::kHomeRightBarColumn : synth::kContextMenuFirstColumn;
  const bool isContextMenuKey = ui_.Mode() == synth::UiMode::ContextMenu &&
                                col >= contextMenuFirstColumn &&
                                col < contextMenuFirstColumn + contextMenuColumns;
  if (col == synth::kHomeLeftBarColumn || col == synth::kHomeRightBarColumn || isContextMenuKey)
  {
    ui_.ReleaseMatrixKey(ActiveEngineMutable(), row, col);
    ApplyPendingUiActions();
    SyncUiTracks();
    SyncSequencerUi();
    return;
  }

  ReleasePerformanceKey(row, col);
}

void SynthApp::ApplyPendingUiActions()
{
  synth::SynthEngineKind nextEngineKind = synth::SynthEngineKind::None;
  if (ui_.ConsumePendingEngineKind(nextEngineKind))
  {
    SwitchTrackEngine(ui_.ActiveTrack(), nextEngineKind);
  }

  synth::UiCommand command;
  while (ui_.ConsumePendingCommand(command))
  {
    switch (command.type)
    {
      case synth::UiCommandType::SamplerStartRecord:
        (void)samplerManager_.BeginRecord(command.trackIndex, command.text == "replace");
        break;
      case synth::UiCommandType::SamplerStopRecord:
        (void)samplerManager_.StopRecord(command.trackIndex);
        break;
      case synth::UiCommandType::SamplerCancelRecord:
        samplerManager_.CancelRecord(command.trackIndex);
        break;
      case synth::UiCommandType::SamplerPreview:
      {
        std::uint8_t previewNote = 60;
        if (command.trackIndex < synth::kSynthTrackCount)
        {
          const auto& slot = rack_.Slot(command.trackIndex);
          if (slot.engine != nullptr)
          {
            synth::SynthParameterInfo root;
            if (slot.engine->GetParameterInfo("sample.root", root))
            {
              previewNote = static_cast<std::uint8_t>(std::clamp(root.value, 0, 127));
            }
          }
        }
        (void)samplerManager_.PreviewCurrentSample(command.trackIndex, previewNote);
        break;
      }
      case synth::UiCommandType::SamplerOpenBrowser:
      {
        const bool success = samplerManager_.SelectBrowserSample(command.trackIndex, command.text);
        ui_.SamplerFileResult(success, samplerManager_.BuildUiState(command.trackIndex).statusText);
        break;
      }
      case synth::UiCommandType::SamplerEnterNaming:
        samplerManager_.EnterDraftNaming(command.trackIndex);
        break;
      case synth::UiCommandType::SamplerCommitName:
        (void)samplerManager_.CommitDraftName(command.trackIndex, command.text);
        break;
      case synth::UiCommandType::SamplerConfirmOverwrite:
        (void)samplerManager_.ConfirmOverwriteDraft(command.trackIndex);
        break;
      case synth::UiCommandType::SamplerRenameCurrent:
        (void)samplerManager_.RenameCurrentSample(command.trackIndex, command.text);
        break;
      case synth::UiCommandType::SamplerCancelOverlay:
        samplerManager_.CancelNameEntry(command.trackIndex);
        break;
      case synth::UiCommandType::SequencerTogglePlayback:
        sequencer_.TogglePlayback();
        SyncSequencerUi();
        break;
      case synth::UiCommandType::SequencerToggleView:
        ClearSequencerHomeGesture();
        ui_.HandleHomeButton(ActiveEngine());
        ui_.ToggleHomeView();
        SyncSequencerUi();
        break;
      case synth::UiCommandType::SequencerStop:
        sequencer_.Stop();
        SyncSequencerUi();
        break;
      case synth::UiCommandType::SequencerPatternPrev:
        sequencer_.StepPattern(-1);
        SyncSequencerUi();
        break;
      case synth::UiCommandType::SequencerPatternNext:
        sequencer_.StepPattern(1);
        SyncSequencerUi();
        break;
      case synth::UiCommandType::SequencerPatternCountDec:
        sequencer_.AdjustPatternCount(-1);
        SyncSequencerUi();
        break;
      case synth::UiCommandType::SequencerPatternCountInc:
        sequencer_.AdjustPatternCount(1);
        SyncSequencerUi();
        break;
      case synth::UiCommandType::SequencerPatternDelete:
        if (command.trackIndex == ui_.ActiveTrack())
        {
          sequencer_.SetActiveTrack(command.trackIndex);
          (void)sequencer_.DeletePattern(command.patternIndex);
          SyncSequencerUi();
        }
        break;
      case synth::UiCommandType::SamplerDeleteCurrent:
        (void)samplerManager_.DeleteCurrentSample(command.trackIndex);
        break;
      case synth::UiCommandType::SamplerRenameFile:
      {
        std::string renamedFile;
        const bool success = samplerManager_.RenameBrowserSample(command.trackIndex, command.fileName, command.text, &renamedFile);
        ui_.SamplerFileResult(success, samplerManager_.BuildUiState(command.trackIndex).statusText, renamedFile);
        break;
      }
      case synth::UiCommandType::SamplerDeleteFile:
      {
        const bool success = samplerManager_.DeleteBrowserSample(command.trackIndex, command.text);
        ui_.SamplerFileResult(success, samplerManager_.BuildUiState(command.trackIndex).statusText);
        break;
      }
      case synth::UiCommandType::SequencerResume:
        sequencer_.Resume();
        break;
      case synth::UiCommandType::SequencerTempoDec:
        sequencer_.AdjustTempo(-1);
        break;
      case synth::UiCommandType::SequencerTempoInc:
        sequencer_.AdjustTempo(1);
        break;
      case synth::UiCommandType::SequencerTempoDecLarge:
        sequencer_.AdjustTempo(-10);
        break;
      case synth::UiCommandType::SequencerTempoIncLarge:
        sequencer_.AdjustTempo(10);
        break;
      case synth::UiCommandType::SequencerMute:
        sequencer_.ToggleTrackMuted();
        break;
      case synth::UiCommandType::SequencerEnable:
        sequencer_.ToggleTrackEnabled();
        break;
      case synth::UiCommandType::SequencerTrackPrev:
      case synth::UiCommandType::SequencerTrackNext:
        ui_.SetActiveTrack((ui_.ActiveTrack() +
            (command.type == synth::UiCommandType::SequencerTrackNext ? 1 : synth::kSynthTrackCount - 1)) % synth::kSynthTrackCount);
        sequencer_.SetActiveTrack(ui_.ActiveTrack());
        SyncSequencerUi();
        break;
      case synth::UiCommandType::ProjectList:
        ui_.SetProjectList(synth::ListProjects());
        break;
      case synth::UiCommandType::ProjectSave:
      case synth::UiCommandType::ProjectOverwrite:
        SaveProject(command.text, command.type == synth::UiCommandType::ProjectOverwrite);
        break;
      case synth::UiCommandType::ProjectLoad:
        ChangeProject(command.text, false);
        break;
      case synth::UiCommandType::ProjectNew:
        ChangeProject("", true);
        break;
      case synth::UiCommandType::None:
      default:
        break;
    }
  }
}

void SynthApp::SaveProject(const std::string& name, bool overwrite)
{
  synth::ProjectData project;
  project.sequence = sequencer_.CaptureState();
  project.activeTrack = ui_.ActiveTrack();
  const auto keyboard = ui_.KeyboardSettings();
  project.octave = keyboard[0];
  project.root = keyboard[1];
  project.scale = static_cast<std::size_t>(keyboard[2]);
  project.layout = keyboard[3];
  project.ledBrightness = Device::LED::GlobalBrightness();
  for (std::size_t track = 0; track < synth::kSynthTrackCount; ++track) {
    const auto sample = samplerManager_.BuildUiState(track);
    if (sample.recording || sample.hasDraft) {
      ui_.ProjectResult(name, "SAVE SAMPLE FIRST", false);
      return;
    }
    const auto& slot = rack_.Slot(track);
    if (!slot.engine || !project.sequence) {
      ui_.ProjectResult(name, "SAVE FAILED", false);
      return;
    }
    project.kinds[track] = slot.kind;
    project.engines[track] = slot.engine->CaptureState();
    project.samples[track] = sample.sampleFileName;
  }
  const auto result = synth::SaveProjectFile(name, project, overwrite);
  if (result == synth::ProjectResult::Exists) ui_.ConfirmProjectOverwrite(name);
  else ui_.ProjectResult(name, result == synth::ProjectResult::Ok ? "SAVED" : "SAVE FAILED", result == synth::ProjectResult::Ok);
}

void SynthApp::StopProjectPerformance()
{
  sequencer_.Stop();
  sequencer_.ClearHeldSteps();
  for (std::size_t track = 0; track < synth::kSynthTrackCount; ++track) {
    activeKeys_[track].fill(false);
    activeNotes_[track].fill(0xFF);
  }
  for (std::size_t row = 0; row < synth::kKeyboardMatrixRows; ++row) ui_.SetHomeActionHeld(row, false);
  sequencerShiftKeys_.fill(false);
  sequencerShiftPressedAt_.fill(std::chrono::steady_clock::time_point{});
  sequencerShiftActionHeld_.fill(false);
  sequencerPatternPressed_.fill(false);
  sequencerViewChordActive_ = false;
  sequencerShiftEventOccurred_ = false;
  ClearSequencerCopySource();
}

void SynthApp::ChangeProject(const std::string& name, bool createNew)
{
  // The command is emitted only by the Project YES confirmation. Even after
  // confirmation an unsaved recording is protected until it is saved/discarded.
  for (std::size_t track = 0; track < synth::kSynthTrackCount; ++track) {
    const auto sample = samplerManager_.BuildUiState(track);
    if (sample.recording || sample.hasDraft) {
      ui_.ProjectResult(name, "SAVE SAMPLE FIRST", false);
      return;
    }
    if (!rack_.Slot(track).engine) {
      ui_.ProjectResult(name, "ENGINE OFFLINE", false);
      return;
    }
  }

  synth::ProjectData project;
  if (createNew) {
    project.sequence.reset(new (memory::slow) synth::SynthSequencer::ProjectState);
    if (!project.sequence) {
      ui_.ProjectResult(name, "NO MEMORY", false);
      return;
    }
    project.kinds = {synth::SynthEngineKind::Dx7, synth::SynthEngineKind::Juno,
                     synth::SynthEngineKind::Sampler, synth::SynthEngineKind::Dx7};
  }
  else if (synth::LoadProjectFile(name, project) != synth::ProjectResult::Ok) {
    ui_.ProjectResult(name, "INVALID PROJECT", false);
    return;
  }

  std::array<std::unique_ptr<synth::ISynthEngine>, synth::kSynthTrackCount> engines;
  std::array<synth::SamplerManager::PreparedSample, synth::kSynthTrackCount> samples;
  // Validate incoming state and preload files while the current session stays intact.
  for (std::size_t track = 0; track < synth::kSynthTrackCount; ++track) {
    engines[track] = synth::CreateSynthEngine(project.kinds[track]);
    if (!engines[track]) {
      ui_.ProjectResult(name, "NO MEMORY", false);
      return;
    }
    // A damaged independent preset bank must not prevent restoring the complete
    // sound state stored in the project; the factory catalog remains available.
    (void)engines[track]->RefreshPresetCatalog();
    if ((!createNew && !engines[track]->RestoreState(project.engines[track])) ||
        !engines[track]->CanReplace(*rack_.Slot(track).engine)) {
      ui_.ProjectResult(name, "INVALID SOUND", false);
      return;
    }
    if (project.samples[track].empty()) continue;
    bool shared = false;
    for (std::size_t previous = 0; previous < track; ++previous) {
      if (project.samples[previous] == project.samples[track]) {
        samples[track] = samples[previous];
        shared = true;
        break;
      }
    }
    if (!shared && !synth::SamplerManager::PrepareSample(project.samples[track], samples[track])) {
      ui_.ProjectResult(name, "SAMPLE UNAVAILABLE", false);
      return;
    }
  }

  // No file reads or MIDI port allocation remain. Transfer existing bindings
  // instead of dropping old engines and attempting a fallible Init midway.
  StopProjectPerformance();
  for (std::size_t track = 0; track < synth::kSynthTrackCount; ++track) {
    samplerManager_.OnTrackEngineChanged(track, synth::SynthEngineKind::None);
  }
  for (std::size_t track = 0; track < synth::kSynthTrackCount; ++track) {
    auto& slot = rack_.Slot(track);
    engines[track]->ReplaceBinding(*slot.engine);
    slot.engine = std::move(engines[track]);
    slot.kind = project.kinds[track];
    samplerManager_.OnTrackEngineChanged(track, slot.kind);
    if (!project.samples[track].empty()) (void)samplerManager_.ApplySample(track, samples[track]);
    // The engine already has the project's saved trim. Suppress the reset used
    // for a standalone sample load before SyncUiTracks runs again.
    (void)samplerManager_.ConsumeTrimReset(track);
    ui_.InitializeTrack(track, slot.kind);
  }
  sequencer_.RestoreState(*project.sequence);
  ui_.RestoreKeyboardSettings({project.octave, project.root, static_cast<int>(project.scale), project.layout});
  ui_.SetActiveTrack(project.activeTrack);
  Device::LED::SetGlobalBrightness(static_cast<std::uint8_t>(project.ledBrightness));
  ui_.ProjectResult(name, createNew ? "NEW" : "LOADED", true);
}

synth::ISynthEngine* SynthApp::ActiveEngineMutable() {
  const std::size_t trackIndex = ui_.ActiveTrack();
  return trackIndex < synth::kSynthTrackCount ? rack_.Slot(trackIndex).engine.get() : nullptr;
}

const synth::ISynthEngine* SynthApp::ActiveEngine() const {
  const std::size_t trackIndex = ui_.ActiveTrack();
  return trackIndex < synth::kSynthTrackCount ? rack_.Slot(trackIndex).engine.get() : nullptr;
}

void SynthApp::SwitchTrackEngine(std::size_t trackIndex, synth::SynthEngineKind kind)
{
  if (trackIndex >= synth::kSynthTrackCount || rack_.Slot(trackIndex).kind == kind)
  {
    return;
  }

  const auto sample = samplerManager_.BuildUiState(trackIndex);
  if (sample.recording || sample.hasDraft)
  {
    ui_.EngineChangeFailed("SAVE SAMPLE FIRST");
    return;
  }
  auto& slot = rack_.Slot(trackIndex);
  auto engine = synth::CreateSynthEngine(kind);
  if (!engine || !slot.engine || !engine->CanReplace(*slot.engine))
  {
    ui_.EngineChangeFailed("ENGINE UNAVAILABLE");
    return;
  }
  (void)engine->RefreshPresetCatalog();
  ReleaseTrackPerformanceKeys(trackIndex);
  samplerManager_.OnTrackEngineChanged(trackIndex, synth::SynthEngineKind::None);
  engine->ReplaceBinding(*slot.engine);
  slot.engine = std::move(engine);
  slot.kind = kind;
  samplerManager_.OnTrackEngineChanged(trackIndex, kind);
}

void SynthApp::SyncUiTracks() {
  for (std::size_t index = 0; index < synth::kSynthTrackCount; ++index)
  {
    const auto& slot = rack_.Slot(index);
    if (slot.kind == synth::SynthEngineKind::Sampler && slot.engine != nullptr)
    {
      if (samplerManager_.ConsumeTrimReset(index)) {
        slot.engine->SetParameterValue("sample.start", 0);
        slot.engine->SetParameterValue("sample.end", 127);
      }
      synth::SynthParameterInfo start;
      synth::SynthParameterInfo end;
      synth::SynthParameterInfo loop;
      if (slot.engine->GetParameterInfo("sample.start", start) && slot.engine->GetParameterInfo("sample.end", end))
      {
        samplerManager_.SetDraftTrimWindow(index, start.value, end.value);
      }
      if (slot.engine->GetParameterInfo("sample.loop", loop))
      {
        samplerManager_.SetDraftLoopEnabled(index, loop.value != 0);
      }
    }

    ui_.SyncTrackState(index, slot.engine.get());
    ui_.SyncSamplerTrackState(index, samplerManager_.BuildUiState(index));
  }
}

void SynthApp::SyncSequencerUi()
{
  sequencer_.SetActiveTrack(ui_.ActiveTrack());
  synth::SequencerUiState state = sequencer_.BuildUiState(ui_.ActiveTrack());
  if (sequencerSelectedStep_ < synth::kSequencerStepCount &&
      (state.activeTrack != sequencerSelectionTrack_ || state.activePattern != sequencerSelectionPattern_))
  {
    sequencer_.ClearHeldSteps();
    CancelStepTap();
    state = sequencer_.BuildUiState(ui_.ActiveTrack());
  }
  if (!state.hasHeldSteps) sequencerSelectedStep_ = synth::kSequencerStepCount;
  state.selectionLatched = sequencerSelectedStep_ < synth::kSequencerStepCount;
  state.shiftActive = IsSequencerShiftModifierActive();
  state.patternSelectorVisible = IsSequencerShiftActive();
  state.shiftActionPressed = sequencerShiftActionHeld_;
  state.patternPressed = sequencerPatternPressed_;
  state.clearMode = IsSequencerClearMode();
  state.copyMode = IsSequencerCopyMode();
  state.quantizeMode = IsSequencerQuantizeMode();
  state.hasCopySource = sequencerCopySource_.type != SequencerCopySourceType::None;
  state.copySourceIsPattern = sequencerCopySource_.type == SequencerCopySourceType::Pattern;
  state.copySourceIndex = sequencerCopySource_.index;
  ui_.SetSequencerUiState(state);
}

std::size_t SynthApp::CollectHeldPerformanceNotes(std::size_t trackIndex,
                                                  std::array<std::uint8_t, synth::kKeyboardMatrixRows * synth::kHomePerformanceColumnCount>& notes) const
{
  if (trackIndex >= activeKeys_.size())
  {
    return 0;
  }

  std::size_t noteCount = 0;
  for (std::size_t row = 0; row < synth::kKeyboardMatrixRows; ++row)
  {
    for (std::size_t col = synth::kHomePerformanceFirstColumn; col <= synth::kHomePerformanceLastColumn; ++col)
    {
      const std::size_t keyIndex = KeyIndex(row, col);
      if (!activeKeys_[trackIndex][keyIndex] || noteCount >= notes.size())
      {
        continue;
      }
      notes[noteCount++] = activeNotes_[trackIndex][keyIndex] == 0xFF
          ? ui_.HomePerformanceMidiNote(row, col)
          : activeNotes_[trackIndex][keyIndex];
    }
  }
  return noteCount;
}

bool SynthApp::IsSequencerShiftKey(std::size_t row, std::size_t col) const
{
  return ui_.Mode() == synth::UiMode::Home && col == synth::kHomeRightBarColumn && row < sequencerShiftKeys_.size();
}

bool SynthApp::IsSequencerShiftActive() const
{
  return IsSequencerShiftModifierActive();
}

bool SynthApp::IsSequencerShiftModifierActive() const
{
  return !sequencerViewChordActive_ &&
         (std::any_of(sequencerShiftKeys_.begin(), sequencerShiftKeys_.end(), [](bool held) { return held; }) ||
          std::any_of(sequencerShiftActionHeld_.begin(), sequencerShiftActionHeld_.end(), [](bool held) { return held; }));
}

bool SynthApp::IsSequencerPatternSelectorKey(std::size_t row, std::size_t col) const
{
  return ui_.Mode() == synth::UiMode::Home &&
         IsSequencerShiftActive() &&
         row < 2 &&
         col >= synth::kHomePerformanceFirstColumn &&
         col <= synth::kHomePerformanceLastColumn;
}

bool SynthApp::IsSequencerClearMode() const
{
  return sequencerShiftActionHeld_[0];
}

bool SynthApp::IsSequencerCopyMode() const
{
  return sequencerShiftActionHeld_[1];
}

bool SynthApp::IsSequencerQuantizeMode() const
{
  return sequencerShiftActionHeld_[2];
}

void SynthApp::MarkSequencerShiftEvent()
{
  sequencerShiftEventOccurred_ = true;
}

void SynthApp::ClearSequencerCopySource()
{
  sequencerCopySource_ = SequencerCopySource{};
}

void SynthApp::ClearSequencerHomeGesture()
{
  sequencer_.ClearHeldSteps();
  sequencerSelectedStep_ = synth::kSequencerStepCount;
  sequencerStepKeys_.fill(false);
  CancelStepTap();
  sequencerShiftKeys_.fill(false);
  sequencerShiftPressedAt_.fill(std::chrono::steady_clock::time_point{});
  sequencerShiftActionHeld_.fill(false);
  sequencerPatternPressed_.fill(false);
  sequencerViewChordActive_ = false;
  sequencerShiftEventOccurred_ = false;
  ClearSequencerCopySource();
  for (std::size_t row = 0; row < synth::kKeyboardMatrixRows; ++row)
    ui_.SetHomeActionHeld(row, false);
}

void SynthApp::CancelStepTap()
{
  sequencerTapStep_ = synth::kSequencerStepCount;
}

bool SynthApp::HandleSequencerShiftPress(std::size_t row, std::size_t col)
{
  if (!IsSequencerShiftKey(row, col))
  {
    return false;
  }

  const std::size_t shiftIndex = row;
  const std::size_t otherShiftIndex = shiftIndex == 0 ? 1 : 0;
  if (sequencerShiftKeys_[shiftIndex]) return true;

  sequencerShiftKeys_[shiftIndex] = true;
  CancelStepTap();
  sequencerShiftPressedAt_[shiftIndex] = std::chrono::steady_clock::now();
  ui_.SetHomeActionHeld(row, true);
  if (sequencerShiftKeys_[otherShiftIndex] && !sequencerViewChordActive_)
  {
    // Consume the entire chord until BOTH keys are up. Keeping the physical
    // held state prevents key repeats and either release order from retoggling
    // the view or applying a stray octave change.
    sequencerViewChordActive_ = true;
    MarkSequencerShiftEvent();
    sequencer_.ClearHeldSteps();
    sequencerSelectedStep_ = synth::kSequencerStepCount;
    sequencerShiftActionHeld_.fill(false);
    sequencerPatternPressed_.fill(false);
    ClearSequencerCopySource();
    ui_.ToggleHomeView();
  }
  return true;
}

bool SynthApp::HandleSequencerShiftRelease(std::size_t row, std::size_t col)
{
  if (row >= sequencerShiftKeys_.size() || col != synth::kHomeRightBarColumn ||
      (ui_.Mode() != synth::UiMode::Home && !sequencerShiftKeys_[row]))
  {
    return false;
  }

  const std::size_t shiftIndex = row;
  const auto now = std::chrono::steady_clock::now();
  const bool wasHeld = sequencerShiftKeys_[shiftIndex];
  const bool shortTap = wasHeld && sequencerShiftPressedAt_[shiftIndex] != std::chrono::steady_clock::time_point{} &&
                        (now - sequencerShiftPressedAt_[shiftIndex]) < kSequencerShiftTapMax;

  sequencerShiftKeys_[shiftIndex] = false;
  sequencerShiftPressedAt_[shiftIndex] = std::chrono::steady_clock::time_point{};
  ui_.SetHomeActionHeld(row, false);

  if (sequencerViewChordActive_)
  {
    if (std::none_of(sequencerShiftKeys_.begin(), sequencerShiftKeys_.end(), [](bool held) { return held; }))
    {
      sequencerViewChordActive_ = false;
      sequencerShiftEventOccurred_ = false;
      ClearSequencerCopySource();
    }
    return true;
  }

  const bool anyActionHeld = std::any_of(sequencerShiftActionHeld_.begin(), sequencerShiftActionHeld_.end(), [](bool held) { return held; });
  if (shortTap && !sequencerShiftEventOccurred_ && !anyActionHeld && ui_.Mode() == synth::UiMode::Home)
  {
    ui_.AdjustHomeKeyboardOctave(row == 0 ? 1 : -1);
  }

  if (!IsSequencerShiftModifierActive())
  {
    sequencerShiftEventOccurred_ = false;
    ClearSequencerCopySource();
  }
  return true;
}

bool SynthApp::HandleSequencerShiftActionPress(std::size_t row)
{
  if (ui_.Mode() != synth::UiMode::Home || !IsSequencerShiftActive() || row >= sequencerShiftActionHeld_.size() || row > 2)
  {
    return false;
  }

  sequencerShiftActionHeld_[row] = true;
  MarkSequencerShiftEvent();
  if (row == 0 && sequencer_.HasHeldSteps())
  {
    sequencer_.ClearHeldStepEvents();
  }
  else if (row == 1)
  {
    ClearSequencerCopySource();
  }
  else if (row == 2)
  {
    if (sequencer_.HasHeldSteps())
    {
      sequencer_.QuantizeHeldSteps();
    }
  }
  return true;
}

bool SynthApp::HandleSequencerShiftActionRelease(std::size_t row)
{
  if (row >= sequencerShiftActionHeld_.size() || !sequencerShiftActionHeld_[row])
  {
    return false;
  }

  sequencerShiftActionHeld_[row] = false;
  if (!IsSequencerShiftModifierActive())
  {
    sequencerShiftEventOccurred_ = false;
    ClearSequencerCopySource();
  }
  return true;
}

bool SynthApp::HandleSequencerPatternSelectorPress(std::size_t row, std::size_t col)
{
  if (!IsSequencerPatternSelectorKey(row, col))
  {
    return false;
  }

  const std::size_t patternIndex = row * synth::kHomePerformanceColumnCount + (col - synth::kHomePerformanceFirstColumn);
  if (patternIndex >= sequencerPatternPressed_.size())
  {
    return true;
  }

  sequencerPatternPressed_[patternIndex] = true;
  MarkSequencerShiftEvent();

  if (IsSequencerClearMode())
  {
    if (sequencer_.PatternHasEvents(patternIndex))
    {
      // The confirmation owns input now; release the Home gesture and audition
      // so its eventual key-up events cannot carry CLR into the next screen.
      sequencer_.ClearHeldSteps();
      ReleaseTrackPerformanceKeys(ui_.ActiveTrack());
      sequencerShiftKeys_.fill(false);
      sequencerShiftActionHeld_.fill(false);
      sequencerPatternPressed_.fill(false);
      sequencerShiftEventOccurred_ = false;
      for (std::size_t actionRow = 0; actionRow < synth::kKeyboardMatrixRows; ++actionRow)
        ui_.SetHomeActionHeld(actionRow, false);
      ui_.ConfirmSequencerPatternDelete(patternIndex, true);
    }
    else
    {
      (void)sequencer_.DeletePattern(patternIndex);
    }
    ClearSequencerCopySource();
    return true;
  }

  if (IsSequencerQuantizeMode())
  {
    sequencer_.QuantizePattern(patternIndex);
    return true;
  }

  if (IsSequencerCopyMode())
  {
    const synth::SequencerUiState state = sequencer_.BuildUiState(ui_.ActiveTrack());
    if (sequencerCopySource_.type == SequencerCopySourceType::None)
    {
      if (patternIndex < state.patternCount)
      {
        sequencerCopySource_.type = SequencerCopySourceType::Pattern;
        sequencerCopySource_.index = patternIndex;
      }
      return true;
    }

    if (sequencerCopySource_.type == SequencerCopySourceType::Pattern)
    {
      (void)sequencer_.CopyPattern(sequencerCopySource_.index, patternIndex);
    }
    return true;
  }

  if (sequencer_.SelectOrCreatePattern(patternIndex))
  {
    sequencer_.ClearHeldSteps();
    sequencerSelectedStep_ = synth::kSequencerStepCount;
  }
  ClearSequencerCopySource();
  return true;
}

bool SynthApp::HandleSequencerPatternSelectorRelease(std::size_t row, std::size_t col)
{
  if (row >= 2 || col < synth::kHomePerformanceFirstColumn || col > synth::kHomePerformanceLastColumn)
  {
    return false;
  }

  const std::size_t patternIndex = row * synth::kHomePerformanceColumnCount + (col - synth::kHomePerformanceFirstColumn);
  if (patternIndex < sequencerPatternPressed_.size() && sequencerPatternPressed_[patternIndex])
  {
    sequencerPatternPressed_[patternIndex] = false;
    return true;
  }
  return false;
}

bool SynthApp::HandleSequencerPress(std::size_t row, std::size_t col)
{
  if (ui_.Mode() != synth::UiMode::Home)
  {
    return false;
  }

  if (HandleSequencerShiftPress(row, col))
  {
    return true;
  }

  if (col == synth::kHomeLeftBarColumn && HandleSequencerShiftActionPress(row))
  {
    return true;
  }

  if (HandleSequencerPatternSelectorPress(row, col))
  {
    return true;
  }

  if (synth::IsSequencerStepKey(row, col) && (IsSequencerClearMode() || IsSequencerCopyMode() || IsSequencerQuantizeMode()))
  {
    const std::size_t stepIndex = synth::SequencerStepIndex(row, col);
    MarkSequencerShiftEvent();
    if (IsSequencerClearMode())
    {
      sequencer_.ClearStep(stepIndex);
      ClearSequencerCopySource();
    }
    else if (IsSequencerCopyMode())
    {
      if (sequencerCopySource_.type == SequencerCopySourceType::None)
      {
        if (sequencer_.StepHasEvents(stepIndex))
        {
          sequencerCopySource_.type = SequencerCopySourceType::Step;
          sequencerCopySource_.index = stepIndex;
        }
      }
      else if (sequencerCopySource_.type == SequencerCopySourceType::Step)
      {
        (void)sequencer_.CopyStep(sequencerCopySource_.index, stepIndex);
      }
    }
    else if (IsSequencerQuantizeMode())
    {
      sequencer_.QuantizeStep(stepIndex);
    }
    return true;
  }

  if (col == synth::kHomeLeftBarColumn && sequencer_.HasHeldSteps())
  {
    CancelStepTap();
    return sequencer_.PressEditLane(row);
  }

  if (synth::IsSequencerStepKey(row, col))
  {
    const std::size_t step = synth::SequencerStepIndex(row, col);
    if (sequencerStepKeys_[step]) return true;
    std::array<std::uint8_t, synth::kKeyboardMatrixRows * synth::kHomePerformanceColumnCount> heldNotes{};
    const std::size_t noteCount = CollectHeldPerformanceNotes(ui_.ActiveTrack(), heldNotes);
    const bool otherStepDown = std::any_of(sequencerStepKeys_.begin(), sequencerStepKeys_.end(), [](bool down) { return down; });
    sequencerStepKeys_[step] = true;
    CancelStepTap();
    if (ui_.HomeShowsSequencerGrid() && !otherStepDown)
    {
      const bool deselect = sequencerSelectedStep_ == step && noteCount == 0;
      if (sequencerSelectedStep_ < synth::kSequencerStepCount)
      {
        sequencer_.ClearHeldSteps();
        sequencerSelectedStep_ = synth::kSequencerStepCount;
      }
      if (deselect) return true;
      if (noteCount == 0)
      {
        sequencerTapStep_ = step;
        sequencerStepPressedAt_ = std::chrono::steady_clock::now();
      }
    }
    sequencer_.PressStep(step, heldNotes.data(), noteCount);
    return true;
  }

  if (sequencer_.HasHeldSteps() && row < synth::kKeyboardMatrixRows && col >= 1 && col <= synth::kHomePerformanceColumnCount)
  {
    CancelStepTap();
    // Value lanes select a physical column, including gaps in the Piano layout.
    if (!ui_.IsHomePerformanceKey(row, col) &&
        sequencer_.BuildUiState(ui_.ActiveTrack()).editLane == synth::SequencerEditLane::Note) return true;
    const bool preview = sequencerSelectedStep_ < synth::kSequencerStepCount && !sequencer_.Playing() &&
                         sequencer_.BuildUiState(ui_.ActiveTrack()).editLane == synth::SequencerEditLane::Note;
    const bool handled = sequencer_.HandlePerformanceKeyPress(row, col, ui_.HomePerformanceMidiNote(row, col));
    if (handled && preview)
    {
      const auto velocity = sequencer_.SelectedNoteVelocity(ui_.HomePerformanceMidiNote(row, col));
      if (velocity != 0) PressPerformanceKey(row, col, velocity);
    }
    return handled;
  }

  return false;
}

bool SynthApp::HandleSequencerRelease(std::size_t row, std::size_t col)
{
  if (HandleSequencerShiftRelease(row, col))
  {
    return true;
  }

  if (col == synth::kHomeLeftBarColumn && HandleSequencerShiftActionRelease(row))
  {
    return true;
  }

  if (HandleSequencerPatternSelectorRelease(row, col))
  {
    return true;
  }

  if (synth::IsSequencerStepKey(row, col))
  {
    const std::size_t stepIndex = synth::SequencerStepIndex(row, col);
    const bool wasDown = sequencerStepKeys_[stepIndex];
    sequencerStepKeys_[stepIndex] = false;
    if (wasDown && stepIndex == sequencerTapStep_ && ui_.Mode() == synth::UiMode::Home && ui_.HomeShowsSequencerGrid() &&
        !IsSequencerShiftActive() && std::chrono::steady_clock::now() - sequencerStepPressedAt_ < std::chrono::milliseconds(250))
    {
      sequencerSelectedStep_ = stepIndex;
      const auto state = sequencer_.BuildUiState(ui_.ActiveTrack());
      sequencerSelectionTrack_ = state.activeTrack;
      sequencerSelectionPattern_ = state.activePattern;
      CancelStepTap();
      return true;
    }
    if (ui_.Mode() == synth::UiMode::Home || sequencer_.IsStepHeld(stepIndex))
    {
      sequencer_.ReleaseStep(stepIndex);
      return true;
    }
    return false;
  }

  if (ui_.Mode() == synth::UiMode::Home && col == synth::kHomeLeftBarColumn && sequencer_.HasHeldSteps())
  {
    return sequencer_.ReleaseEditLane(row);
  }

  return false;
}

void SynthApp::ReleaseTrackPerformanceKeys(std::size_t trackIndex)
{
  if (trackIndex >= activeKeys_.size())
  {
    return;
  }

  for (std::size_t row = 0; row < synth::kKeyboardMatrixRows; ++row)
  {
    for (std::size_t col = synth::kHomePerformanceFirstColumn; col <= synth::kHomePerformanceLastColumn; ++col)
    {
      const std::size_t keyIndex = KeyIndex(row, col);
      if (!activeKeys_[trackIndex][keyIndex])
      {
        continue;
      }

      activeKeys_[trackIndex][keyIndex] = false;
        const std::uint8_t note = activeNotes_[trackIndex][keyIndex] == 0xFF
          ? ui_.HomePerformanceMidiNote(row, col)
          : activeNotes_[trackIndex][keyIndex];
        activeNotes_[trackIndex][keyIndex] = 0xFF;
        os::midi::Send(midi::MidiPacket::NoteOff(0, note, 0), rack_.PortForTrack(trackIndex));
    }
  }
}

bool SynthApp::IsKeyActive(std::size_t trackIndex, std::size_t keyIndex) const {
  if (trackIndex >= activeKeys_.size() || keyIndex >= activeKeys_[trackIndex].size())
  {
    return false;
  }

  return activeKeys_[trackIndex][keyIndex];
}

const std::array<bool, synth::kKeyboardMatrixRows * synth::kKeyboardMatrixColumns>& SynthApp::ActiveKeyStates(std::size_t trackIndex) const {
  static const std::array<bool, synth::kKeyboardMatrixRows * synth::kKeyboardMatrixColumns> kEmpty{};
  if (trackIndex >= activeKeys_.size())
  {
    return kEmpty;
  }
  return activeKeys_[trackIndex];
}

void SynthApp::PressPerformanceKey(std::size_t row, std::size_t col, std::uint8_t velocity) {
  const std::size_t keyIndex = KeyIndex(row, col);
  const std::size_t trackIndex = ui_.ActiveTrack();
  if (trackIndex >= activeKeys_.size() || keyIndex >= activeKeys_[trackIndex].size())
  {
    return;
  }

  const std::uint8_t note = ui_.HomePerformanceMidiNote(row, col);
  for (std::size_t heldTrackIndex = 0; heldTrackIndex < activeKeys_.size(); ++heldTrackIndex)
  {
    if (keyIndex >= activeKeys_[heldTrackIndex].size() || !activeKeys_[heldTrackIndex][keyIndex])
    {
      continue;
    }

    activeKeys_[heldTrackIndex][keyIndex] = false;
    const std::uint8_t heldNote = activeNotes_[heldTrackIndex][keyIndex] == 0xFF
        ? note
        : activeNotes_[heldTrackIndex][keyIndex];
    activeNotes_[heldTrackIndex][keyIndex] = 0xFF;
    os::midi::Send(midi::MidiPacket::NoteOff(0, heldNote, 0), rack_.PortForTrack(heldTrackIndex));
  }

  activeKeys_[trackIndex][keyIndex] = true;
  activeNotes_[trackIndex][keyIndex] = note;
  os::midi::Send(midi::MidiPacket::NoteOn(0, note, velocity), rack_.PortForTrack(trackIndex));
}

void SynthApp::ReleasePerformanceKey(std::size_t row, std::size_t col) {
  const std::size_t keyIndex = KeyIndex(row, col);
  for (std::size_t trackIndex = 0; trackIndex < activeKeys_.size(); ++trackIndex)
  {
    if (keyIndex >= activeKeys_[trackIndex].size() || !activeKeys_[trackIndex][keyIndex])
    {
      continue;
    }

    activeKeys_[trackIndex][keyIndex] = false;
    const std::uint8_t note = activeNotes_[trackIndex][keyIndex] == 0xFF
        ? ui_.HomePerformanceMidiNote(row, col)
        : activeNotes_[trackIndex][keyIndex];
    activeNotes_[trackIndex][keyIndex] = 0xFF;
    os::midi::Send(midi::MidiPacket::NoteOff(0, note, 0), rack_.PortForTrack(trackIndex));
  }
}

void SynthApp::SendSequencerMidi(void* context, std::size_t trackIndex, const midi::MidiPacket& packet)
{
  (void)context;
  if (trackIndex >= synth::kSynthTrackCount) return;
  // Bypass the UI queue: the audio runtime synchronizes stable track indices.
  if (packet.status == midi::MidiStatus::NoteOn && packet.data[2] != 0)
    synth::audio::NoteOn(trackIndex, packet.data[1], packet.data[2]);
  else if (packet.status == midi::MidiStatus::NoteOff || packet.status == midi::MidiStatus::NoteOn)
    synth::audio::NoteOff(trackIndex, packet.data[1]);
  else if (packet.status == midi::MidiStatus::ControlChange && (packet.data[1] == 120 || packet.data[1] == 123))
    synth::audio::AllNotesOff(trackIndex);
}

std::string SynthApp::DebugView() const {
  std::ostringstream stream;

  const std::size_t activeTrack = ui_.ActiveTrack();
  const auto& slot = rack_.Slot(activeTrack);
  const synth::ISynthEngine* engine = slot.engine.get();

  stream << ui_.DebugSummary(engine) << "\n";
  stream << ui_.HeaderEngine(engine) << "  " << ui_.HeaderTrack() << "\n";
  stream << ui_.PatchName() << "\n";

  if (ui_.Mode() == synth::UiMode::ContextMenu)
  {
    stream << ui_.ContextSectionTitle() << "\n";
    stream << ui_.ContextParameterName(engine) << " = " << ui_.ContextValueText(engine) << "\n";

    const auto menu = ui_.CurrentMenuCells(engine);
    for (std::size_t row = 0; row < synth::kContextMenuRows; ++row)
    {
      for (std::size_t column = 0; column < synth::kContextMenuColumns; ++column)
      {
        const std::size_t index = row * synth::kContextMenuColumns + column;
        stream << '[' << menu[index].label;
        if (ui_.IsMenuCellActive(row, column))
        {
          stream << '*';
        }
        stream << ']';
        if (column + 1 < synth::kContextMenuColumns)
        {
          stream << ' ';
        }
      }
      stream << "\n";
    }
  }

  for (const std::string& summary : rack_.DescribeSlots())
  {
    stream << summary << "\n";
  }

  return stream.str();
}

}  // namespace apps
