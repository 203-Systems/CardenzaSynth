#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "Applications/Synth/Runtime/Sampler/SamplerTrackState.h"
#include "Applications/Synth/Runtime/Sequencer/SynthSequencerTypes.h"
#include "Library/Synth/Core/ISynthEngine.h"
#include "Applications/Synth/UI/Model/SynthUiTypes.h"

namespace synth {

class SynthUiModel {
public:
  SynthUiModel();

  void InitializeTrack(std::size_t trackIndex, SynthEngineKind kind);
  void SyncTrackState(std::size_t trackIndex, const ISynthEngine* engine);
  void SyncSamplerTrackState(std::size_t trackIndex, const SamplerTrackUiState& state);
  void SetSequencerUiState(const SequencerUiState& state);
  void ConfirmSequencerPatternDelete(std::size_t patternIndex, bool returnHome);
  void SetActiveTrack(std::size_t trackIndex);
  void SetHomeActionHeld(std::size_t row, bool held);
  void AdjustHomeKeyboardOctave(int delta);
  void Tick(const ISynthEngine* engine);
  MatrixKeyHandling PressMatrixKey(ISynthEngine* engine, std::size_t row, std::size_t col);
  void ReleaseMatrixKey(ISynthEngine* engine, std::size_t row, std::size_t col);
  void HandleHomeButton(const ISynthEngine* engine);
  void ExitToHome();
  void ToggleHomeView();
  bool HomeShowsSequencerGrid() const { return showSequencerGrid_; }
  std::array<int, 4> KeyboardSettings() const;
  bool RestoreKeyboardSettings(const std::array<int, 4>& settings);
  void SetProjectList(const std::vector<std::string>& names);
  void ProjectResult(const std::string& name, const std::string& status, bool success);
  void ConfirmProjectOverwrite(const std::string& name);
  void EngineChangeFailed(const std::string& status);
  void SamplerFileResult(bool success, const std::string& status, const std::string& renamedFile = {});

  std::size_t ActiveTrack() const { return activeTrack_; }
  UiMode Mode() const { return mode_; }
  UiModifier ActiveModifier() const { return activeModifier_; }
  std::size_t CurrentPage() const { return currentPage_; }
  std::size_t ActiveMenuRow() const { return activeMenuRow_; }
  std::size_t ActiveMenuColumn() const { return activeMenuColumn_; }
  bool HasContextSelection() const { return hasContextSelection_; }

  std::string HeaderEngine(const ISynthEngine* engine) const;
  std::string HeaderTrack() const;
  bool IsHomePerformanceKey(std::size_t row, std::size_t col) const;
  bool IsKeyboardHintKeyLit(std::size_t row, std::size_t col) const;
  bool IsHomePerformanceKeyLit(std::size_t row, std::size_t col) const;
  std::uint8_t HomePerformanceMidiNote(std::size_t row, std::size_t col) const;
  const std::string& PatchName() const;
  float LooperProgress() const;

  std::string ContextSectionTitle() const;
  std::string ContextParameterName(const ISynthEngine* engine) const;
  std::string ContextValueText(const ISynthEngine* engine) const;
  ContextMenuGrid CurrentMenuCells(const ISynthEngine* engine) const;
  std::size_t ContextMenuColumnCount() const;
  UiScreenSnapshot CurrentScreenSnapshot(const ISynthEngine* engine) const;
  bool ConsumePendingEngineKind(SynthEngineKind& kind);
  bool ConsumePendingCommand(UiCommand& command);
  const UiTransitionSnapshot& Transition() const { return transition_; }
  bool IsModifierHighlighted(std::size_t row) const;
  bool IsMenuCellActive(std::size_t row, std::size_t col) const;
  std::string DebugSummary(const ISynthEngine* engine) const;

private:
  enum class ContextSelectionKind {
    None,
    Preset,
    Parameter,
    Status,
  };

  struct TrackState {
    SynthEngineKind kind = SynthEngineKind::None;
    std::string patchName = "INIT";
    float looperProgress = 0.0f;
    SamplerTrackUiState sampler;
  };

  static constexpr std::size_t kPagedMenuItemCount = kContextMenuCellCount - 1;

  void ActivateModifier(UiModifier modifier);
  void ClearContextSelection();
  void HighlightMenuIndex(std::size_t index);
  void SelectStatus(const std::string& label, const std::string& value, const std::string& itemId = "");
  void AdjustKeyboardOctave(int delta);
  void AdjustKeyboardRoot(int delta);
  void AdjustKeyboardScale(int delta);
  void SetKeyboardLayoutMode(KeyboardLayoutMode mode);
  bool KeyboardMidiNoteForKey(std::size_t row, std::size_t col, int& note) const;
  bool IsKeyboardScaleNote(int note) const;
  std::string KeyboardRootName() const;
  std::string KeyboardScaleName() const;
  std::string KeyboardLayoutName() const;
  std::string KeyboardSummaryText() const;
  void EnterEnginePicker();
  void EnterPresetBrowser(const ISynthEngine* engine);
  void EnterSamplerBrowser();
  void ExitSamplerBrowser();
  void RefreshSamplerBrowserEntries();
  void EnterParameterAdjustMode(const SynthParameterInfo& parameter, std::size_t menuIndex);
  void QueueCommand(UiCommandType type, const std::string& text = {}, const std::string& fileName = {});
  void ExitPresetNameEntry();
  void ExitSamplerNameEntry();
  void ExitSamplerRecordConfirm();
  void MoveSamplerTrimCursor(int delta);
  void SetSamplerTrimBoundary(ISynthEngine* engine, bool begin);
  bool HandlePresetBrowserAutoScroll(const ISynthEngine* engine, std::chrono::steady_clock::time_point now);
  void StepPresetBrowserSelection(const ISynthEngine* engine, int delta);
  bool IsContextRoot() const;
  void ApplyMenuCell(ISynthEngine* engine, const ContextMenuCell& cell, std::size_t index);
  std::size_t PageCountForModifier(const ISynthEngine* engine) const;
  ContextMenuGrid BuildSoundMenu(const ISynthEngine* engine) const;
  ContextMenuGrid BuildParameterBrowseMenu(const ISynthEngine* engine) const;
  ContextMenuGrid BuildParameterAdjustMenu() const;
  ContextMenuGrid BuildLayoutMenu() const;
  ContextMenuGrid BuildOtherMenu() const;
  UiBarSnapshot BuildModifierBarSnapshot(UiModifier highlightedModifier) const;
  UiBarSnapshot BuildSequencerEditBarSnapshot() const;
  UiBarSnapshot BuildSequencerShiftBarSnapshot() const;
  UiBarSnapshot BuildHomeActionBarSnapshot() const;
  UiBarSnapshot BuildContextBarSnapshot(const ISynthEngine* engine) const;
  std::pair<std::string, float> CellPreview(const ContextMenuCell& cell, const ISynthEngine* engine) const;
  void PopulatePerformanceKeys(UiScreenSnapshot& snapshot) const;
  UiScreenSnapshot BuildScreenSnapshot(const ISynthEngine* engine) const;
  void StartTransition(const UiScreenSnapshot& from, const UiScreenSnapshot& to);
  void CancelTransition();
  void ClearHeldModifiers();
  void CloseSequencerPatternDelete();
  UiModifier FirstHeldModifier() const;

  std::array<TrackState, kSynthTrackCount> tracks_{};
  std::array<bool, kKeyboardMatrixRows> heldModifiers_{};
  std::array<bool, kKeyboardMatrixRows> heldHomeActionRows_{};
  std::array<bool, kKeyboardMatrixRows> modifierContextActivated_{};
  std::array<std::chrono::steady_clock::time_point, kKeyboardMatrixRows> modifierPressStartedAt_{};
  std::size_t activeTrack_ = 0;
  int keyboardOctaveOffset_ = 0;
  int keyboardRootNote_ = 0;
  std::size_t keyboardScaleIndex_ = 1;
  KeyboardLayoutMode keyboardLayoutMode_ = KeyboardLayoutMode::Octave;
  std::chrono::steady_clock::time_point homeOctaveStatusUntil_{};
  UiMode mode_ = UiMode::Home;
  bool showSequencerGrid_ = false;
  UiModifier activeModifier_ = UiModifier::None;
  std::size_t currentPage_ = 0;
  std::size_t activeMenuRow_ = 0;
  std::size_t activeMenuColumn_ = 0;
  bool hasContextSelection_ = false;
  bool hasMenuHighlight_ = false;
  bool parameterAdjustMode_ = false;
  ContextSelectionKind selectionKind_ = ContextSelectionKind::None;
  std::string selectedItemId_;
  std::string selectedItemLabel_;
  std::string selectedValueText_;
  int selectedParameterEntryValue_ = 0;
  std::string selectedParameterGroupId_;
  int heldContextMenuIndex_ = -1;
  std::chrono::steady_clock::time_point contextMenuPressStartedAt_{};
  std::chrono::steady_clock::time_point presetBrowserAutoScrollLastAt_{};
  bool heldContextMenuActionConsumed_ = false;
  bool contextMenuHintVisible_ = false;
  std::string heldContextMenuHintTitle_;
  std::string heldContextMenuHintText_;
  bool enginePickerMode_ = false;
  SynthEngineKind enginePickerInitialKind_ = SynthEngineKind::None;
  SynthEngineKind enginePickerPreviewKind_ = SynthEngineKind::None;
  bool hasPendingEngineKind_ = false;
  SynthEngineKind pendingEngineKind_ = SynthEngineKind::None;
  std::vector<UiCommand> pendingCommands_;
  bool presetBrowserMode_ = false;
  std::string presetSearchText_;
  std::size_t presetBrowserSelectionIndex_ = 0;
  bool presetNameEntryMode_ = false;
  std::string presetNameText_;
  bool samplerBrowserMode_ = false;
  std::vector<std::string> samplerBrowserAllEntries_;
  std::vector<std::string> samplerBrowserEntries_;
  std::string samplerBrowserSearchText_;
  std::size_t samplerBrowserSelectionIndex_ = 0;
  bool samplerNameEntryMode_ = false;
  bool samplerRenameEntryMode_ = false;
  std::string samplerNameText_;
  bool samplerRecordConfirmMode_ = false;
  bool samplerEditFromBrowser_ = false;
  std::string samplerFileTarget_;
  std::string samplerFileStatus_;
  int samplerTrimCursor_ = 0;
  bool contextLatched_ = false;
  std::string projectName_;
  std::string projectNameText_;
  bool projectNameEntryMode_ = false;
  std::vector<std::string> projectNames_;
  std::size_t projectSelectionIndex_ = 0;
  UiCommandType projectConfirmCommand_ = UiCommandType::None;
  std::string projectConfirmName_;
  std::string projectStatus_;
  SequencerUiState sequencer_{};
  bool sequencerDeleteConfirm_ = false;
  bool sequencerDeleteReturnHome_ = false;
  std::size_t sequencerDeleteTrack_ = 0;
  std::size_t sequencerDeletePattern_ = 0;
  UiTransitionSnapshot transition_{};
};

}  // namespace synth
