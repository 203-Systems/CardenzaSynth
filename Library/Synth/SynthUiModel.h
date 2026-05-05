#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <string>

#include "Library/Synth/ISynthEngine.h"

namespace synth {

enum class MenuCellType {
  Empty,
  Item,
  NextPage,
  UtilityAction,
};

enum class ContextActionId {
  None,
  EngineNext,
  EnginePrev,
  EngineReset,
  EngineLoad,
  PresetLoad,
  PresetPrev,
  PresetNext,
  PresetDelete,
  PresetClear,
  ParameterDecLarge,
  ParameterDecSmall,
  ParameterIncSmall,
  ParameterIncLarge,
  ParameterMin,
  ParameterMax,
  ParameterReset,
  Back,
  SavePreset,
  SavePresetAs,
  LayoutChromatic,
  LayoutScale,
  LayoutArp,
  LayoutLatch,
  LayoutOctaveDown,
  LayoutOctaveUp,
  LayoutGrid,
  LayoutSave,
  ShowInfo,
  AudioSetup,
  MidiSetup,
  ResetPatch,
};

struct ContextMenuCell {
  std::string label;
  MenuCellType type = MenuCellType::Empty;
  ContextActionId actionId = ContextActionId::None;
  std::string itemId;
};

using ContextMenuGrid = std::array<ContextMenuCell, kContextMenuCellCount>;

enum class UiBarIcon {
  None,
  Plus,
  Minus,
  Circle,
  Play,
};

struct UiBarItemSnapshot {
  std::string label;
  UiBarIcon icon = UiBarIcon::None;
  bool active = false;
};

using UiBarSnapshot = std::array<UiBarItemSnapshot, kContextMenuCellCount>;

struct UiScreenSnapshot {
  UiMode mode = UiMode::Home;
  UiModifier modifier = UiModifier::None;
  SynthEngineKind engineKind = SynthEngineKind::None;
  std::string headerPrimary;
  std::string headerSecondary;
  std::string primaryText;
  std::string secondaryText;
  std::string focusItemId;
  bool showPresetBrowser = false;
  bool showSecondaryText = false;
  int dx7Algorithm = 0;
  int dx7Feedback = 0;
  float looperProgress = 0.0f;
  UiBarSnapshot leftBarItems{};
  bool leftBarVisible = true;
  std::size_t leftBarColumns = 1;
  float leftBarWidthScale = 1.0f;
  UiBarSnapshot rightBarItems{};
  bool rightBarVisible = true;
  std::size_t rightBarColumns = 1;
  float rightBarWidthScale = 1.0f;
};

struct UiTransitionSnapshot {
  bool active = false;
  float progress = 1.0f;
  UiScreenSnapshot from;
  UiScreenSnapshot to;
};

enum class MatrixKeyHandling {
  Ignored,
  ModifierHold,
  MenuAction,
  PerformanceKey,
};

class SynthUiModel {
public:
  SynthUiModel();

  void InitializeTrack(std::size_t trackIndex, SynthEngineKind kind);
  void SyncTrackState(std::size_t trackIndex, const ISynthEngine* engine);
  void SetActiveTrack(std::size_t trackIndex);
  void Tick(const ISynthEngine* engine);
  MatrixKeyHandling PressMatrixKey(ISynthEngine* engine, std::size_t row, std::size_t col);
  void ReleaseMatrixKey(const ISynthEngine* engine, std::size_t row, std::size_t col);
  void ExitToHome();

  std::size_t ActiveTrack() const { return activeTrack_; }
  UiMode Mode() const { return mode_; }
  UiModifier ActiveModifier() const { return activeModifier_; }
  std::size_t CurrentPage() const { return currentPage_; }
  std::size_t ActiveMenuRow() const { return activeMenuRow_; }
  std::size_t ActiveMenuColumn() const { return activeMenuColumn_; }
  bool HasContextSelection() const { return hasContextSelection_; }

  std::string HeaderEngine(const ISynthEngine* engine) const;
  std::string HeaderTrack() const;
  const std::string& PatchName() const;
  float LooperProgress() const;

  std::string ContextSectionTitle() const;
  std::string ContextParameterName(const ISynthEngine* engine) const;
  std::string ContextValueText(const ISynthEngine* engine) const;
  ContextMenuGrid CurrentMenuCells(const ISynthEngine* engine) const;
  std::size_t ContextMenuColumnCount() const;
  UiScreenSnapshot CurrentScreenSnapshot(const ISynthEngine* engine) const;
  bool ConsumePendingEngineKind(SynthEngineKind& kind);
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
  };

  static constexpr std::size_t kPagedMenuItemCount = kContextMenuCellCount - 1;

  void ActivateModifier(UiModifier modifier);
  void ClearContextSelection();
  void HighlightMenuIndex(std::size_t index);
  void SelectStatus(const std::string& label, const std::string& value, const std::string& itemId = "");
  void EnterEnginePicker();
  void EnterPresetBrowser(const ISynthEngine* engine);
  void EnterParameterAdjustMode(const SynthParameterInfo& parameter, std::size_t menuIndex);
  void ApplyMenuCell(ISynthEngine* engine, const ContextMenuCell& cell, std::size_t index);
  std::size_t PageCountForModifier(const ISynthEngine* engine) const;
  ContextMenuGrid BuildSoundMenu(const ISynthEngine* engine) const;
  ContextMenuGrid BuildParameterBrowseMenu(const ISynthEngine* engine) const;
  ContextMenuGrid BuildParameterAdjustMenu() const;
  ContextMenuGrid BuildLayoutMenu() const;
  ContextMenuGrid BuildOtherMenu() const;
  UiBarSnapshot BuildModifierBarSnapshot(UiModifier highlightedModifier) const;
  UiBarSnapshot BuildHomeActionBarSnapshot() const;
  UiBarSnapshot BuildContextBarSnapshot(const ISynthEngine* engine) const;
  UiScreenSnapshot BuildScreenSnapshot(const ISynthEngine* engine) const;
  void StartTransition(const UiScreenSnapshot& from, const UiScreenSnapshot& to);
  void CancelTransition();
  void ClearHeldModifiers();
  UiModifier FirstHeldModifier() const;

  std::array<TrackState, kSynthTrackCount> tracks_{};
  std::array<bool, kKeyboardMatrixRows> heldModifiers_{};
  std::array<bool, kKeyboardMatrixRows> heldHomeActionRows_{};
  std::array<bool, kKeyboardMatrixRows> modifierContextActivated_{};
  std::array<std::chrono::steady_clock::time_point, kKeyboardMatrixRows> modifierPressStartedAt_{};
  std::size_t activeTrack_ = 0;
  UiMode mode_ = UiMode::Home;
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
  bool enginePickerMode_ = false;
  SynthEngineKind enginePickerInitialKind_ = SynthEngineKind::None;
  SynthEngineKind enginePickerPreviewKind_ = SynthEngineKind::None;
  bool hasPendingEngineKind_ = false;
  SynthEngineKind pendingEngineKind_ = SynthEngineKind::None;
  bool presetBrowserMode_ = false;
  std::string presetSearchText_;
  std::size_t presetBrowserSelectionIndex_ = 0;
  bool contextLatched_ = false;
  UiTransitionSnapshot transition_{};
};

}  // namespace synth