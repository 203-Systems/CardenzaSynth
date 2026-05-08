#pragma once

#include <array>
#include <cstddef>
#include <string>

#include "Library/Synth/SynthTypes.h"

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