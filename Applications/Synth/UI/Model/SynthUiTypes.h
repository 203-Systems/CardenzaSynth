#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "Applications/Synth/Runtime/Sampler/SamplerTrackState.h"
#include "Library/Synth/Core/SynthTypes.h"

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
  JunoChorusOff,
  JunoChorusI,
  JunoChorusII,
  JunoRange16,
  JunoRange8,
  JunoRange4,
  JunoSawOn,
  JunoSawOff,
  JunoHpf0,
  JunoHpf1,
  JunoHpf2,
  JunoHpf3,
  JunoHpfReset,
  SamplerRecord,
  SamplerRecordStart,
  SamplerStop,
  SamplerCancel,
  SamplerPlay,
  SamplerEdit,
  SamplerSelect,
  SamplerName,
  SamplerNameSave,
  SamplerNameBackspace,
  SamplerNameCancel,
  SamplerConfirmOverwrite,
  SamplerRename,
  SamplerDelete,
  SamplerTrimLeft,
  SamplerTrimRight,
  SamplerTrimBegin,
  SamplerTrimEnd,
  SamplerTrimPreview,
  SamplerTrimSet,
  Back,
  SavePreset,
  SavePresetAs,
  PresetNameSave,
  PresetNameBackspace,
  LayoutChromatic,
  LayoutScale,
  KeyboardRootPrev,
  KeyboardRootNext,
  KeyboardScalePrev,
  KeyboardScaleNext,
  KeyboardModePiano,
  KeyboardModeOctave,
  KeyboardModeChromatic,
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

enum class KeyboardLayoutMode {
  Piano,
  Octave,
  Chromatic,
};

enum class UiCommandType {
  None,
  SamplerStartRecord,
  SamplerStopRecord,
  SamplerCancelRecord,
  SamplerPreview,
  SamplerOpenBrowser,
  SamplerEnterNaming,
  SamplerCommitName,
  SamplerConfirmOverwrite,
  SamplerRenameCurrent,
  SamplerDeleteCurrent,
  SamplerCancelOverlay,
};

struct UiCommand {
  UiCommandType type = UiCommandType::None;
  std::size_t trackIndex = 0;
  std::string text;
};

struct ContextMenuCell {
  std::string label;
  std::string fullName;
  std::string description;
  MenuCellType type = MenuCellType::Empty;
  ContextActionId actionId = ContextActionId::None;
  std::string itemId;
  bool disabled = false;
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
  std::string fullName;
  std::string description;
  UiBarIcon icon = UiBarIcon::None;
  bool active = false;
  bool disabled = false;
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
  KeyboardLayoutMode keyboardLayoutMode = KeyboardLayoutMode::Octave;
  std::string keyboardRootName;
  std::string keyboardScaleName;
  bool keyboardRootDisabled = false;
  bool keyboardScaleDisabled = false;
  bool showPresetBrowser = false;
  bool showHelpPopup = false;
  bool showSecondaryText = false;
  std::string helpPopupTitle;
  std::string helpPopupText;
  int dx7Algorithm = 0;
  int dx7Feedback = 0;
  int junoDcoRange = 1;
  int junoDcoPwm = 0;
  int junoDcoSub = 0;
  int junoDcoNoise = 0;
  int junoDcoSaw = 0;
  int junoVcfCutoff = 0;
  int junoVcfResonance = 0;
  int junoVcfEnv = 0;
  int junoVcfLfo = 0;
  int junoHpfMode = 0;
  int junoVcaLevel = 0;
  int junoEnvAttack = 0;
  int junoEnvDecay = 0;
  int junoEnvSustain = 0;
  int junoEnvRelease = 0;
  int junoLfoRate = 0;
  int junoLfoDelay = 0;
  int junoChorusMode = 0;
  int samplerStart = 0;
  int samplerEnd = 127;
  int samplerLoop = 0;
  int samplerTune = 0;
  int samplerTrimCursor = 0;
  std::uint32_t samplerPreviewCursorFrame = 0;
  int samplerRoot = 60;
  int samplerGain = 96;
  int samplerLevel = 96;
  int samplerPan = 64;
  int samplerCutoff = 96;
  int samplerResonance = 18;
  int samplerAttack = 4;
  int samplerRelease = 18;
  std::string samplerSampleName = "EMPTY";
  std::string samplerStatusText = "EMPTY";
  std::string samplerDraftName;
  std::uint32_t samplerSampleRate = 44100;
  std::uint32_t samplerFrameCount = 0;
  std::uint32_t samplerRecordMaxFrameCount = 0;
  std::uint32_t samplerTrimStartFrame = 0;
  std::uint32_t samplerTrimEndFrame = 0;
  std::array<float, kSamplerWaveformBinCount> samplerWaveform{};
  float samplerInputLevel = 0.0f;
  bool samplerHasDraft = false;
  bool samplerNeedsName = false;
  bool samplerOverwritePending = false;
  bool samplerDeleteConfirm = false;
  bool samplerHasSample = false;
  bool samplerRecordConfirmMode = false;
  bool samplerRecording = false;
  bool samplerPreviewing = false;
  float samplerRecordProgress = 0.0f;
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

}  // namespace synth
