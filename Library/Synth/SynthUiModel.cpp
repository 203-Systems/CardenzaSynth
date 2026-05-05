#include "Library/Synth/SynthUiModel.h"

#include <algorithm>
#include <chrono>
#include <sstream>
#include <utility>

namespace synth {

namespace {
constexpr std::size_t kPagedMenuItemCount = kContextMenuCellCount - 1;
constexpr float kTransitionStep = 0.18f;
constexpr auto kModifierHoldDelay = std::chrono::milliseconds(200);
constexpr std::array<const char*, 4> kModifierBarLabels = {"TRK", "SND", "LAY", "SYS"};
constexpr std::array<UiBarIcon, 4> kHomeActionBarIcons = {
  UiBarIcon::Plus,
  UiBarIcon::Minus,
  UiBarIcon::Circle,
  UiBarIcon::Play,
};

const char* ModifierDisplayName(UiModifier modifier)
{
  switch (modifier)
  {
    case UiModifier::Sound:
      return "Track";
    case UiModifier::Parameter:
      return "Sound";
    case UiModifier::Layout:
      return "Layout";
    case UiModifier::Other:
      return "System";
    case UiModifier::None:
    default:
      return "Home";
  }
}

UiModifier ModifierForRow(std::size_t row)
{
  switch (row)
  {
    case 0:
      return UiModifier::Sound;
    case 1:
      return UiModifier::Parameter;
    case 2:
      return UiModifier::Layout;
    case 3:
      return UiModifier::Other;
    default:
      return UiModifier::None;
  }
}

std::size_t RowForModifier(UiModifier modifier)
{
  switch (modifier)
  {
    case UiModifier::Sound:
      return 0;
    case UiModifier::Parameter:
      return 1;
    case UiModifier::Layout:
      return 2;
    case UiModifier::Other:
      return 3;
    case UiModifier::None:
    default:
      return 0;
  }
}

ContextMenuCell MakeItemCell(const std::string& label, const std::string& itemId)
{
  ContextMenuCell cell;
  cell.label = label;
  cell.type = MenuCellType::Item;
  cell.itemId = itemId;
  return cell;
}

ContextMenuCell MakeActionCell(const std::string& label, ContextActionId actionId)
{
  ContextMenuCell cell;
  cell.label = label;
  cell.type = MenuCellType::UtilityAction;
  cell.actionId = actionId;
  return cell;
}

ContextMenuCell MakeNextPageCell(const std::string& label)
{
  ContextMenuCell cell;
  cell.label = label;
  cell.type = MenuCellType::NextPage;
  return cell;
}

ContextMenuGrid BlankGrid()
{
  return ContextMenuGrid{};
}

bool ReadParameter(const ISynthEngine* engine, const std::string& parameterId, SynthParameterInfo& parameter)
{
  return engine != nullptr && engine->GetParameterInfo(parameterId, parameter);
}

int ParameterValueOrDefault(const ISynthEngine* engine, const std::string& parameterId, int fallback)
{
  SynthParameterInfo parameter;
  return ReadParameter(engine, parameterId, parameter) ? parameter.value : fallback;
}

std::string DescribeParameterValueOrFallback(const ISynthEngine* engine,
                                             const std::string& parameterId,
                                             const std::string& fallback = "--")
{
  return engine != nullptr ? engine->DescribeParameterValue(parameterId) : fallback;
}

std::string FormatDx7OperatorSummary(const ISynthEngine* engine, int op)
{
  const std::string prefix = "op" + std::to_string(op) + ".";
  return "OUT " + DescribeParameterValueOrFallback(engine, prefix + "output") +
         " CRS " + DescribeParameterValueOrFallback(engine, prefix + "coarse");
}

std::string FormatDx7AlgorithmSummary(const ISynthEngine* engine)
{
  return DescribeParameterValueOrFallback(engine, "global.algorithm", "ALG --") +
         " FB " + DescribeParameterValueOrFallback(engine, "global.feedback");
}

std::string FormatDx7GlobalSummary(const ISynthEngine* engine)
{
  return "TR " + DescribeParameterValueOrFallback(engine, "global.transpose") +
         " LFO " + DescribeParameterValueOrFallback(engine, "global.lfo_speed");
}

bool IsDx7ParameterEngine(const ISynthEngine* engine)
{
  return engine != nullptr && engine->Descriptor().kind == SynthEngineKind::Dx7;
}

bool IsDx7ParameterGroupId(const std::string& itemId)
{
  return itemId == "par.dx7.group.op1" || itemId == "par.dx7.group.op2" || itemId == "par.dx7.group.op3" ||
         itemId == "par.dx7.group.op4" || itemId == "par.dx7.group.op5" || itemId == "par.dx7.group.op6" ||
         itemId == "par.dx7.group.alg" || itemId == "par.dx7.group.glb";
}

std::string Dx7GroupLabel(const std::string& groupId)
{
  if (groupId == "par.dx7.group.op1") return "Operator 1";
  if (groupId == "par.dx7.group.op2") return "Operator 2";
  if (groupId == "par.dx7.group.op3") return "Operator 3";
  if (groupId == "par.dx7.group.op4") return "Operator 4";
  if (groupId == "par.dx7.group.op5") return "Operator 5";
  if (groupId == "par.dx7.group.op6") return "Operator 6";
  if (groupId == "par.dx7.group.alg") return "Algorithm";
  if (groupId == "par.dx7.group.glb") return "Global";
  return "DX7";
}

std::string Dx7GroupSummary(const ISynthEngine* engine, const std::string& groupId)
{
  if (groupId == "par.dx7.group.op1") return FormatDx7OperatorSummary(engine, 1);
  if (groupId == "par.dx7.group.op2") return FormatDx7OperatorSummary(engine, 2);
  if (groupId == "par.dx7.group.op3") return FormatDx7OperatorSummary(engine, 3);
  if (groupId == "par.dx7.group.op4") return FormatDx7OperatorSummary(engine, 4);
  if (groupId == "par.dx7.group.op5") return FormatDx7OperatorSummary(engine, 5);
  if (groupId == "par.dx7.group.op6") return FormatDx7OperatorSummary(engine, 6);
  if (groupId == "par.dx7.group.alg") return FormatDx7AlgorithmSummary(engine);
  if (groupId == "par.dx7.group.glb") return FormatDx7GlobalSummary(engine);
  return "SELECT GROUP";
}

std::string Dx7ParameterMenuLabel(const SynthParameterInfo& parameter)
{
  if (parameter.id == "global.algorithm") return "ALG";
  if (parameter.id == "global.feedback") return "FB";
  if (parameter.id == "global.transpose") return "TRNS";
  if (parameter.id == "global.lfo_speed") return "SPD";
  if (parameter.id == "global.lfo_delay") return "DLY";
  if (parameter.id == "global.lfo_pitch") return "PMOD";
  if (parameter.id == "global.lfo_amp") return "AMOD";
  if (parameter.label == "Output") return "OUT";
  if (parameter.label == "Coarse") return "CRS";
  if (parameter.label == "Fine") return "FINE";
  if (parameter.label == "Detune") return "DTN";
  if (parameter.label == "Attack") return "ATK";
  if (parameter.label == "Decay") return "DEC";
  if (parameter.label == "Sustain") return "SUS";
  if (parameter.label == "Release") return "REL";
  if (parameter.label == "Velocity") return "VEL";
  if (parameter.label == "Amp Mod") return "AMOD";
  return parameter.label;
}

std::vector<SynthParameterInfo> CollectDx7GroupParameters(const ISynthEngine* engine, const std::string& groupId)
{
  std::vector<SynthParameterInfo> parameters;
  if (engine == nullptr)
  {
    return parameters;
  }

  auto appendIfPresent = [&](const std::string& parameterId) {
    SynthParameterInfo parameter;
    if (engine->GetParameterInfo(parameterId, parameter))
    {
      parameters.push_back(parameter);
    }
  };

  if (groupId == "par.dx7.group.alg")
  {
    appendIfPresent("global.algorithm");
    appendIfPresent("global.feedback");
    return parameters;
  }

  if (groupId == "par.dx7.group.glb")
  {
    appendIfPresent("global.transpose");
    appendIfPresent("global.lfo_speed");
    appendIfPresent("global.lfo_delay");
    appendIfPresent("global.lfo_pitch");
    appendIfPresent("global.lfo_amp");
    return parameters;
  }

  const SynthParameterPage page = engine->ListParameters({0, 128});
  const std::string groupLabel = Dx7GroupLabel(groupId);
  for (const SynthParameterInfo& parameter : page.items)
  {
    if (parameter.group == groupLabel)
    {
      parameters.push_back(parameter);
    }
  }
  return parameters;
}

std::size_t Dx7ParameterPageCount(const ISynthEngine* engine, const std::string& groupId)
{
  constexpr std::size_t kPageCapacity = 6;
  const std::size_t count = CollectDx7GroupParameters(engine, groupId).size();
  return std::max<std::size_t>(1, (count + kPageCapacity - 1) / kPageCapacity);
}

std::size_t Dx7ParameterMenuIndex(const ISynthEngine* engine,
                                  const std::string& groupId,
                                  const std::string& parameterId,
                                  std::size_t pageIndex)
{
  constexpr std::size_t kPageCapacity = 6;
  const auto parameters = CollectDx7GroupParameters(engine, groupId);
  const auto iterator = std::find_if(parameters.begin(), parameters.end(), [&](const SynthParameterInfo& parameter) {
    return parameter.id == parameterId;
  });
  if (iterator == parameters.end())
  {
    return 0;
  }

  const std::size_t absoluteIndex = static_cast<std::size_t>(std::distance(parameters.begin(), iterator));
  const std::size_t pageStart = pageIndex * kPageCapacity;
  if (absoluteIndex < pageStart || absoluteIndex >= (pageStart + kPageCapacity))
  {
    return 0;
  }

  return absoluteIndex - pageStart;
}

std::pair<std::string, std::string> MenuItemPresentation(UiModifier modifier,
                                                         const std::string& itemId,
                                                         const std::string& patchName,
                                                         const ISynthEngine* engine)
{
  const std::string engineName = engine != nullptr ? engine->Descriptor().displayName : "SYNTH";

  if (modifier == UiModifier::Sound)
  {
    if (itemId == "snd.dx7.op1") return {"Operator 1", FormatDx7OperatorSummary(engine, 1)};
    if (itemId == "snd.dx7.op2") return {"Operator 2", FormatDx7OperatorSummary(engine, 2)};
    if (itemId == "snd.dx7.op3") return {"Operator 3", FormatDx7OperatorSummary(engine, 3)};
    if (itemId == "snd.dx7.op4") return {"Operator 4", FormatDx7OperatorSummary(engine, 4)};
    if (itemId == "snd.dx7.op5") return {"Operator 5", FormatDx7OperatorSummary(engine, 5)};
    if (itemId == "snd.dx7.op6") return {"Operator 6", FormatDx7OperatorSummary(engine, 6)};
    if (itemId == "snd.dx7.alg") return {"Algorithm", FormatDx7AlgorithmSummary(engine)};
    if (itemId == "snd.dx7.glb") return {"Global", FormatDx7GlobalSummary(engine)};
    if (itemId == "snd.engine") return {"Engine", engineName};
    if (itemId == "snd.preset") return {"Preset", patchName};
    if (itemId == "snd.volume") return {"Volume", "92"};
    if (itemId == "snd.filter") return {"Filter", "72"};
  }

  if (modifier == UiModifier::Parameter)
  {
    if (itemId == "par.prev") return {"Previous Parameter", "STEP"};
    if (itemId == "par.next") return {"Next Parameter", "STEP"};
    if (itemId == "par.fine") return {"Fine Adjust", "+12"};
    if (itemId == "par.rst") return {"Reset", "DEFAULT"};
    if (itemId == "par.min") return {"Minimum", "0"};
    if (itemId == "par.max") return {"Maximum", "127"};
    if (itemId == "par.lock") return {"Lock", "OFF"};
    if (itemId == "par.m1") return {"Macro 1", "UNMAPPED"};
    if (itemId == "par.m2") return {"Macro 2", "UNMAPPED"};
    if (itemId == "par.m3") return {"Macro 3", "UNMAPPED"};
    if (itemId == "par.m4") return {"Macro 4", "UNMAPPED"};
    if (itemId == "par.map") return {"Map Macro", "READY"};
    if (itemId == "par.clr") return {"Clear Macro", "READY"};
    if (itemId == "par.save") return {"Save Mapping", "READY"};
  }

  if (modifier == UiModifier::Layout)
  {
    if (itemId == "lay.scl") return {"Scale", "C MIN"};
    if (itemId == "lay.root") return {"Root", "C"};
    if (itemId == "lay.mode") return {"Mode", "IONIAN"};
    if (itemId == "lay.chrd") return {"Chord", "TRIAD"};
    if (itemId == "lay.oct") return {"Octave", "+0"};
    if (itemId == "lay.trns") return {"Transpose", "0"};
    if (itemId == "lay.key") return {"Key Layout", "GRID"};
    if (itemId == "lay.grid") return {"Grid", "4X14"};
    if (itemId == "lay.keyb") return {"Keyboard", "PIANO"};
    if (itemId == "lay.drum") return {"Drum", "OFF"};
    if (itemId == "lay.mpe") return {"MPE", "OFF"};
    if (itemId == "lay.latch") return {"Latch", "OFF"};
    if (itemId == "lay.arp") return {"Arp", "OFF"};
    if (itemId == "lay.save") return {"Save Layout", "DONE"};
  }

  if (modifier == UiModifier::Other)
  {
    if (itemId == "sys.save") return {"Save", patchName};
    if (itemId == "sys.load") return {"Load", "BROWSE"};
    if (itemId == "sys.midi") return {"MIDI", "LOCAL"};
    if (itemId == "sys.sync") return {"Sync", "INT"};
    if (itemId == "sys.usb") return {"USB", "DEVICE"};
    if (itemId == "sys.sd") return {"SD Card", "READY"};
    if (itemId == "sys.set") return {"System", "80%"};
    if (itemId == "sys.brgt") return {"Brightness", "80%"};
    if (itemId == "sys.info") return {"Info", engineName};
    if (itemId == "sys.cal") return {"Calibration", "READY"};
    if (itemId == "sys.test") return {"Test", "OK"};
    if (itemId == "sys.rst") return {"Reset", "HOLD"};
    if (itemId == "sys.upd") return {"Update", "IDLE"};
    if (itemId == "sys.back") return {"Back", "HOME"};
  }

  return {"READY", patchName.empty() ? engineName : patchName};
}

}  // namespace

SynthUiModel::SynthUiModel() = default;

void SynthUiModel::InitializeTrack(std::size_t trackIndex, SynthEngineKind kind)
{
  if (trackIndex >= tracks_.size())
  {
    return;
  }

  tracks_[trackIndex].kind = kind;
  tracks_[trackIndex].patchName = ToString(kind);
  tracks_[trackIndex].looperProgress = 0.0f;
}

void SynthUiModel::SyncTrackState(std::size_t trackIndex, const ISynthEngine* engine)
{
  if (trackIndex >= tracks_.size() || engine == nullptr)
  {
    return;
  }

  if (!engine->CurrentPresetName().empty())
  {
    tracks_[trackIndex].patchName = engine->CurrentPresetName();
  }
}

void SynthUiModel::SetActiveTrack(std::size_t trackIndex)
{
  if (trackIndex >= tracks_.size())
  {
    return;
  }

  activeTrack_ = trackIndex;
}

void SynthUiModel::Tick(const ISynthEngine* engine)
{
  const auto now = std::chrono::steady_clock::now();
  for (std::size_t row = 0; row < heldModifiers_.size(); ++row)
  {
    if (!heldModifiers_[row] || modifierContextActivated_[row])
    {
      continue;
    }

    if ((now - modifierPressStartedAt_[row]) < kModifierHoldDelay)
    {
      continue;
    }

    const UiScreenSnapshot from = BuildScreenSnapshot(engine);
    ActivateModifier(ModifierForRow(row));
    modifierContextActivated_[row] = true;
    StartTransition(from, BuildScreenSnapshot(engine));
    break;
  }

  if (transition_.active)
  {
    transition_.progress = std::min(1.0f, transition_.progress + kTransitionStep);
    if (transition_.progress >= 1.0f)
    {
      transition_.active = false;
    }
  }
}

MatrixKeyHandling SynthUiModel::PressMatrixKey(ISynthEngine* engine, std::size_t row, std::size_t col)
{
  if (row >= kKeyboardMatrixRows || col >= kKeyboardMatrixColumns)
  {
    return MatrixKeyHandling::Ignored;
  }

  if (col == kHomeLeftBarColumn)
  {
    if (mode_ == UiMode::ContextMenu && contextLatched_)
    {
      const UiScreenSnapshot from = BuildScreenSnapshot(engine);
      ClearHeldModifiers();
      ExitToHome();
      StartTransition(from, BuildScreenSnapshot(engine));
      return MatrixKeyHandling::Ignored;
    }

    if (!heldModifiers_[row])
    {
      heldModifiers_[row] = true;
      modifierContextActivated_[row] = false;
      modifierPressStartedAt_[row] = std::chrono::steady_clock::now();
    }
    return MatrixKeyHandling::ModifierHold;
  }

  if (mode_ == UiMode::Home && col == kHomeRightBarColumn)
  {
    return MatrixKeyHandling::Ignored;
  }

  if (mode_ == UiMode::ContextMenu && col >= kContextMenuFirstColumn)
  {
    const auto menu = CurrentMenuCells(engine);
    const std::size_t menuIndex = ContextMenuColumnCount() == 1 ? row : row * kContextMenuColumns + (col - kContextMenuFirstColumn);
    if (menuIndex < menu.size())
    {
      ApplyMenuCell(engine, menu[menuIndex], menuIndex);
    }
    return MatrixKeyHandling::MenuAction;
  }

  if (mode_ == UiMode::Home && (col < kHomePerformanceFirstColumn || col > kHomePerformanceLastColumn))
  {
    return MatrixKeyHandling::Ignored;
  }

  return MatrixKeyHandling::PerformanceKey;
}

void SynthUiModel::ReleaseMatrixKey(const ISynthEngine* engine, std::size_t row, std::size_t col)
{
  if (row >= kKeyboardMatrixRows || col != kHomeLeftBarColumn || !heldModifiers_[row])
  {
    return;
  }

  const bool activatedContext = modifierContextActivated_[row];
  const auto heldDuration = std::chrono::steady_clock::now() - modifierPressStartedAt_[row];
  heldModifiers_[row] = false;
  modifierContextActivated_[row] = false;

  if (!activatedContext)
  {
    if (heldDuration < kModifierHoldDelay && row < tracks_.size())
    {
      SetActiveTrack(row);
      CancelTransition();
    }
    return;
  }

  if (contextLatched_)
  {
    return;
  }

  const UiScreenSnapshot from = BuildScreenSnapshot(engine);
  ExitToHome();
  StartTransition(from, BuildScreenSnapshot(engine));
}

void SynthUiModel::ExitToHome()
{
  mode_ = UiMode::Home;
  activeModifier_ = UiModifier::None;
  currentPage_ = 0;
  activeMenuRow_ = 0;
  activeMenuColumn_ = 0;
  contextLatched_ = false;
  selectedParameterGroupId_.clear();
  ClearContextSelection();
}

std::string SynthUiModel::HeaderEngine(const ISynthEngine* engine) const
{
  if (engine != nullptr)
  {
    return engine->Descriptor().displayName;
  }
  return ToString(tracks_[activeTrack_].kind);
}

std::string SynthUiModel::HeaderTrack() const
{
  return "Track " + std::to_string(activeTrack_ + 1);
}

const std::string& SynthUiModel::PatchName() const
{
  return tracks_[activeTrack_].patchName;
}

float SynthUiModel::LooperProgress() const
{
  return tracks_[activeTrack_].looperProgress;
}

std::string SynthUiModel::ContextSectionTitle() const
{
  if (mode_ != UiMode::ContextMenu)
  {
    return "HOME";
  }

  switch (activeModifier_)
  {
    case UiModifier::Sound:
      return "SOUND";
    case UiModifier::Parameter:
      return "PARAMETER";
    case UiModifier::Layout:
      return "LAYOUT";
    case UiModifier::Other:
      return "SYSTEM";
    case UiModifier::None:
    default:
      return "HOME";
  }
}

std::string SynthUiModel::ContextParameterName(const ISynthEngine* engine) const
{
  (void)engine;
  if (mode_ != UiMode::ContextMenu || !hasContextSelection_)
  {
    return "";
  }

  switch (selectionKind_)
  {
    case ContextSelectionKind::Preset:
      return "Preset";
    case ContextSelectionKind::Parameter:
    case ContextSelectionKind::Status:
      return selectedItemLabel_;
    case ContextSelectionKind::None:
    default:
      return "";
  }
}

std::string SynthUiModel::ContextValueText(const ISynthEngine* engine) const
{
  if (mode_ != UiMode::ContextMenu || !hasContextSelection_)
  {
    return "";
  }

  switch (selectionKind_)
  {
    case ContextSelectionKind::Preset:
      return engine != nullptr ? engine->CurrentPresetName() : PatchName();
    case ContextSelectionKind::Parameter:
      return engine != nullptr ? engine->DescribeParameterValue(selectedItemId_) : "--";
    case ContextSelectionKind::Status:
      return selectedValueText_;
    case ContextSelectionKind::None:
    default:
      return "";
  }
}

ContextMenuGrid SynthUiModel::CurrentMenuCells(const ISynthEngine* engine) const
{
  if (mode_ != UiMode::ContextMenu)
  {
    return BlankGrid();
  }

  switch (activeModifier_)
  {
    case UiModifier::Sound:
      return BuildSoundMenu(engine);
    case UiModifier::Parameter:
      if (parameterAdjustMode_)
      {
        return BuildParameterAdjustMenu();
      }
      return BuildParameterBrowseMenu(engine);
    case UiModifier::Layout:
      return BuildLayoutMenu();
    case UiModifier::Other:
      return BuildOtherMenu();
    case UiModifier::None:
    default:
      return BlankGrid();
  }
}

std::size_t SynthUiModel::ContextMenuColumnCount() const
{
  return activeModifier_ == UiModifier::Sound ? 1 : 2;
}

UiScreenSnapshot SynthUiModel::CurrentScreenSnapshot(const ISynthEngine* engine) const
{
  return BuildScreenSnapshot(engine);
}

bool SynthUiModel::IsModifierHighlighted(std::size_t row) const
{
  return mode_ == UiMode::ContextMenu && row == RowForModifier(activeModifier_);
}

bool SynthUiModel::IsMenuCellActive(std::size_t row, std::size_t col) const
{
  return mode_ == UiMode::ContextMenu && hasMenuHighlight_ && row == activeMenuRow_ && col == activeMenuColumn_;
}

std::string SynthUiModel::DebugSummary(const ISynthEngine* engine) const
{
  std::ostringstream stream;
  stream << (mode_ == UiMode::Home ? "HOME" : "CONTEXT")
         << "  engine=" << HeaderEngine(engine)
         << "  track=" << (activeTrack_ + 1)
         << "  patch=" << PatchName();
  if (mode_ == UiMode::ContextMenu)
  {
    stream << "  section=" << ContextSectionTitle()
           << "  page=" << (currentPage_ + 1);
    if (hasContextSelection_)
    {
      stream << "  param=" << ContextParameterName(engine)
             << "  value=" << ContextValueText(engine);
    }
  }
  return stream.str();
}

UiBarSnapshot SynthUiModel::BuildModifierBarSnapshot(UiModifier highlightedModifier) const
{
  UiBarSnapshot items{};
  for (std::size_t row = 0; row < kModifierBarLabels.size(); ++row)
  {
    items[row].label = kModifierBarLabels[row];
    items[row].active = highlightedModifier != UiModifier::None && row == RowForModifier(highlightedModifier);
  }
  return items;
}

UiBarSnapshot SynthUiModel::BuildHomeActionBarSnapshot() const
{
  UiBarSnapshot items{};
  for (std::size_t row = 0; row < kHomeActionBarIcons.size(); ++row)
  {
    items[row].icon = kHomeActionBarIcons[row];
  }
  return items;
}

UiBarSnapshot SynthUiModel::BuildContextBarSnapshot(const ISynthEngine* engine) const
{
  UiBarSnapshot items{};
  const auto menu = CurrentMenuCells(engine);
  const std::size_t columns = ContextMenuColumnCount();
  for (std::size_t row = 0; row < kContextMenuRows; ++row)
  {
    for (std::size_t column = 0; column < columns; ++column)
    {
      const std::size_t index = columns == 1 ? row : row * kContextMenuColumns + column;
      items[index].label = menu[index].label;
      items[index].active = hasMenuHighlight_ && row == activeMenuRow_ && column == activeMenuColumn_;
    }
  }
  return items;
}

UiScreenSnapshot SynthUiModel::BuildScreenSnapshot(const ISynthEngine* engine) const
{
  UiScreenSnapshot snapshot;
  snapshot.mode = mode_;
  snapshot.modifier = activeModifier_;
  snapshot.engineKind = engine != nullptr ? engine->Descriptor().kind : tracks_[activeTrack_].kind;
  snapshot.focusItemId = selectedItemId_.empty() ? selectedParameterGroupId_ : selectedItemId_;
  snapshot.dx7Algorithm = ParameterValueOrDefault(engine, "global.algorithm", 0);
  snapshot.dx7Feedback = ParameterValueOrDefault(engine, "global.feedback", 0);

  if (mode_ == UiMode::Home)
  {
    snapshot.headerPrimary = HeaderEngine(engine);
    snapshot.headerSecondary = HeaderTrack();
    snapshot.primaryText = PatchName();
    snapshot.looperProgress = LooperProgress();
    snapshot.leftBarItems = BuildModifierBarSnapshot(UiModifier::None);
    snapshot.leftBarVisible = true;
    snapshot.leftBarColumns = 1;
    snapshot.leftBarWidthScale = 1.0f;
    snapshot.rightBarItems = BuildHomeActionBarSnapshot();
    snapshot.rightBarVisible = true;
    snapshot.rightBarColumns = 1;
    snapshot.rightBarWidthScale = 1.0f;
    return snapshot;
  }

  snapshot.headerPrimary = std::string(ModifierDisplayName(activeModifier_));
  if (snapshot.engineKind == SynthEngineKind::Dx7 && activeModifier_ == UiModifier::Parameter)
  {
    if (parameterAdjustMode_ && hasContextSelection_)
    {
      snapshot.primaryText = ContextParameterName(engine);
      snapshot.secondaryText = ContextValueText(engine);
    }
    else if (!selectedParameterGroupId_.empty())
    {
      snapshot.primaryText = Dx7GroupLabel(selectedParameterGroupId_);
      snapshot.secondaryText = Dx7GroupSummary(engine, selectedParameterGroupId_);
    }
    else
    {
      snapshot.primaryText = "DX7";
      snapshot.secondaryText = FormatDx7AlgorithmSummary(engine);
    }
    snapshot.showSecondaryText = true;
  }
  else
  {
    snapshot.primaryText = hasContextSelection_ ? ContextParameterName(engine) : std::string{"Settings"};
    snapshot.secondaryText = hasContextSelection_ ? ContextValueText(engine) : std::string{};
    snapshot.showSecondaryText = hasContextSelection_;
  }
  snapshot.leftBarItems = BuildModifierBarSnapshot(activeModifier_);
  snapshot.leftBarVisible = false;
  snapshot.leftBarColumns = 1;
  snapshot.leftBarWidthScale = 1.0f;
  snapshot.rightBarItems = BuildContextBarSnapshot(engine);
  snapshot.rightBarVisible = true;
  snapshot.rightBarColumns = ContextMenuColumnCount();
  snapshot.rightBarWidthScale = snapshot.rightBarColumns == 1 ? 1.5f : 1.0f;
  return snapshot;
}

void SynthUiModel::StartTransition(const UiScreenSnapshot& from, const UiScreenSnapshot& to)
{
  transition_.from = from;
  transition_.to = to;

  if (from.mode == to.mode && from.modifier == to.modifier && from.primaryText == to.primaryText &&
      from.secondaryText == to.secondaryText && from.leftBarVisible == to.leftBarVisible &&
      from.leftBarWidthScale == to.leftBarWidthScale && from.rightBarColumns == to.rightBarColumns &&
      from.rightBarWidthScale == to.rightBarWidthScale)
  {
    transition_.active = false;
    transition_.progress = 1.0f;
    return;
  }

  transition_.active = true;
  transition_.progress = 0.0f;
}

void SynthUiModel::CancelTransition()
{
  transition_.active = false;
  transition_.progress = 1.0f;
}

void SynthUiModel::ActivateModifier(UiModifier modifier)
{
  if (modifier == UiModifier::None)
  {
    ExitToHome();
    return;
  }

  if (mode_ == UiMode::ContextMenu && activeModifier_ == modifier)
  {
    return;
  }

  mode_ = UiMode::ContextMenu;
  activeModifier_ = modifier;
  currentPage_ = 0;
  activeMenuRow_ = 0;
  activeMenuColumn_ = 0;
  contextLatched_ = false;
  selectedParameterGroupId_.clear();
  ClearContextSelection();
}

void SynthUiModel::ClearContextSelection()
{
  hasContextSelection_ = false;
  hasMenuHighlight_ = false;
  parameterAdjustMode_ = false;
  selectionKind_ = ContextSelectionKind::None;
  selectedItemId_.clear();
  selectedItemLabel_.clear();
  selectedValueText_.clear();
  selectedParameterEntryValue_ = 0;
}

void SynthUiModel::HighlightMenuIndex(std::size_t index)
{
  hasMenuHighlight_ = true;
  if (ContextMenuColumnCount() == 1)
  {
    activeMenuRow_ = index;
    activeMenuColumn_ = 0;
    return;
  }

  activeMenuRow_ = index / kContextMenuColumns;
  activeMenuColumn_ = index % kContextMenuColumns;
}

void SynthUiModel::SelectStatus(const std::string& label, const std::string& value, const std::string& itemId)
{
  hasContextSelection_ = true;
  selectionKind_ = ContextSelectionKind::Status;
  selectedItemId_ = itemId;
  selectedItemLabel_ = label;
  selectedValueText_ = value;
}

void SynthUiModel::EnterParameterAdjustMode(const SynthParameterInfo& parameter, std::size_t menuIndex)
{
  hasContextSelection_ = true;
  parameterAdjustMode_ = true;
  selectionKind_ = ContextSelectionKind::Parameter;
  selectedItemId_ = parameter.id;
  selectedItemLabel_ = parameter.label;
  selectedParameterEntryValue_ = parameter.value;
  hasMenuHighlight_ = false;
  HighlightMenuIndex(menuIndex);
  hasMenuHighlight_ = false;
}

void SynthUiModel::ApplyMenuCell(ISynthEngine* engine, const ContextMenuCell& cell, std::size_t index)
{
  if (cell.type == MenuCellType::Empty)
  {
    return;
  }

  if (FirstHeldModifier() != UiModifier::None)
  {
    contextLatched_ = true;
  }

  HighlightMenuIndex(index);
  if (cell.type == MenuCellType::NextPage)
  {
    currentPage_ = (currentPage_ + 1) % std::max<std::size_t>(1, PageCountForModifier(engine));
    ClearContextSelection();
    return;
  }

  if (cell.type == MenuCellType::Item)
  {
    if (activeModifier_ == UiModifier::Parameter && IsDx7ParameterEngine(engine))
    {
      if (IsDx7ParameterGroupId(cell.itemId))
      {
        selectedParameterGroupId_ = cell.itemId;
        currentPage_ = 0;
        ClearContextSelection();
        return;
      }

      if (cell.itemId == "par.dx7.back")
      {
        selectedParameterGroupId_.clear();
        currentPage_ = 0;
        ClearContextSelection();
        return;
      }

      if (!selectedParameterGroupId_.empty() && engine != nullptr)
      {
        SynthParameterInfo parameter;
        if (engine->GetParameterInfo(cell.itemId, parameter))
        {
          EnterParameterAdjustMode(parameter, index);
          return;
        }
      }
    }

    const auto presentation = MenuItemPresentation(activeModifier_, cell.itemId, tracks_[activeTrack_].patchName, engine);
    SelectStatus(presentation.first, presentation.second, cell.itemId);
    return;
  }

  if (cell.type != MenuCellType::UtilityAction)
  {
    return;
  }

  switch (cell.actionId)
  {
    case ContextActionId::ParameterDecLarge:
      if (engine != nullptr)
      {
        (void)engine->AdjustParameterValue(selectedItemId_, -10);
        hasContextSelection_ = true;
        selectionKind_ = ContextSelectionKind::Parameter;
      }
      break;
    case ContextActionId::ParameterDecSmall:
      if (engine != nullptr)
      {
        (void)engine->AdjustParameterValue(selectedItemId_, -1);
        hasContextSelection_ = true;
        selectionKind_ = ContextSelectionKind::Parameter;
      }
      break;
    case ContextActionId::ParameterIncSmall:
      if (engine != nullptr)
      {
        (void)engine->AdjustParameterValue(selectedItemId_, 1);
        hasContextSelection_ = true;
        selectionKind_ = ContextSelectionKind::Parameter;
      }
      break;
    case ContextActionId::ParameterIncLarge:
      if (engine != nullptr)
      {
        (void)engine->AdjustParameterValue(selectedItemId_, 10);
        hasContextSelection_ = true;
        selectionKind_ = ContextSelectionKind::Parameter;
      }
      break;
    case ContextActionId::ParameterMin:
      if (engine != nullptr)
      {
        SynthParameterInfo parameter;
        if (engine->GetParameterInfo(selectedItemId_, parameter))
        {
          (void)engine->SetParameterValue(selectedItemId_, parameter.minValue);
        }
      }
      break;
    case ContextActionId::ParameterMax:
      if (engine != nullptr)
      {
        SynthParameterInfo parameter;
        if (engine->GetParameterInfo(selectedItemId_, parameter))
        {
          (void)engine->SetParameterValue(selectedItemId_, parameter.maxValue);
        }
      }
      break;
    case ContextActionId::ParameterReset:
      if (engine != nullptr)
      {
        (void)engine->SetParameterValue(selectedItemId_, selectedParameterEntryValue_);
      }
      break;
    case ContextActionId::Back:
      if (activeModifier_ == UiModifier::Parameter && parameterAdjustMode_)
      {
        parameterAdjustMode_ = false;
        selectionKind_ = ContextSelectionKind::None;
        hasContextSelection_ = false;
        selectedItemId_.clear();
        selectedItemLabel_.clear();
        selectedValueText_.clear();
        selectedParameterEntryValue_ = 0;
        hasMenuHighlight_ = false;
      }
      else
      {
        currentPage_ = 0;
        ClearContextSelection();
      }
      break;
    case ContextActionId::SavePreset:
      if (engine != nullptr)
      {
        (void)engine->SavePreset(engine->CurrentPresetId(), engine->CurrentPresetName());
        tracks_[activeTrack_].patchName = engine->CurrentPresetName();
        SelectStatus("Save Preset", tracks_[activeTrack_].patchName);
      }
      break;
    case ContextActionId::SavePresetAs:
      if (engine != nullptr)
      {
        const std::string name = tracks_[activeTrack_].patchName + " Copy";
        (void)engine->SavePreset("", name);
        tracks_[activeTrack_].patchName = engine->CurrentPresetName();
        SelectStatus("Save As", tracks_[activeTrack_].patchName);
      }
      break;
    case ContextActionId::LayoutChromatic:
      SelectStatus("Layout", "CHROMATIC");
      break;
    case ContextActionId::LayoutScale:
      SelectStatus("Layout", "SCALE");
      break;
    case ContextActionId::LayoutArp:
      SelectStatus("Arp", "ON");
      break;
    case ContextActionId::LayoutLatch:
      SelectStatus("Latch", "ON");
      break;
    case ContextActionId::LayoutOctaveDown:
      SelectStatus("Octave", "-");
      break;
    case ContextActionId::LayoutOctaveUp:
      SelectStatus("Octave", "+");
      break;
    case ContextActionId::LayoutGrid:
      SelectStatus("Grid", "4x14");
      break;
    case ContextActionId::LayoutSave:
      SelectStatus("Layout", "SAVED");
      break;
    case ContextActionId::ShowInfo:
      SelectStatus("Info", HeaderTrack());
      break;
    case ContextActionId::AudioSetup:
      SelectStatus("Audio", "ROUTING");
      break;
    case ContextActionId::MidiSetup:
      SelectStatus("MIDI", "LOCAL");
      break;
    case ContextActionId::ResetPatch:
      SelectStatus("Patch", "RESET");
      break;
    case ContextActionId::None:
    default:
      break;
  }

  hasMenuHighlight_ = false;
}

void SynthUiModel::ClearHeldModifiers()
{
  heldModifiers_.fill(false);
  modifierContextActivated_.fill(false);
  modifierPressStartedAt_.fill(std::chrono::steady_clock::time_point{});
}

std::size_t SynthUiModel::PageCountForModifier(const ISynthEngine* engine) const
{
  if (engine == nullptr)
  {
    return 1;
  }

  switch (activeModifier_)
  {
    case UiModifier::Sound:
      return 1;
    case UiModifier::Parameter:
      if (parameterAdjustMode_)
      {
        return 1;
      }
      if (IsDx7ParameterEngine(engine) && !selectedParameterGroupId_.empty())
      {
        return Dx7ParameterPageCount(engine, selectedParameterGroupId_);
      }
      return IsDx7ParameterEngine(engine) ? 1 : 2;
    case UiModifier::Layout:
      return 2;
    case UiModifier::Other:
      return 2;
    case UiModifier::None:
    default:
      return 1;
  }
}

ContextMenuGrid SynthUiModel::BuildSoundMenu(const ISynthEngine* engine) const
{
  (void)engine;
  return ContextMenuGrid{
      MakeItemCell("ENGINE", "snd.engine"),
      MakeItemCell("PRESET", "snd.preset"),
      MakeItemCell("VOLUME", "snd.volume"),
      MakeItemCell("FILTER", "snd.filter"),
      ContextMenuCell{},
      ContextMenuCell{},
      ContextMenuCell{},
      ContextMenuCell{},
  };
}

ContextMenuGrid SynthUiModel::BuildParameterBrowseMenu(const ISynthEngine* engine) const
{
  if (IsDx7ParameterEngine(engine))
  {
    if (selectedParameterGroupId_.empty())
    {
      return ContextMenuGrid{
          MakeItemCell("OP1", "par.dx7.group.op1"),
          MakeItemCell("OP2", "par.dx7.group.op2"),
          MakeItemCell("OP3", "par.dx7.group.op3"),
          MakeItemCell("OP4", "par.dx7.group.op4"),
          MakeItemCell("OP5", "par.dx7.group.op5"),
          MakeItemCell("OP6", "par.dx7.group.op6"),
          MakeItemCell("ALG", "par.dx7.group.alg"),
          MakeItemCell("GLB", "par.dx7.group.glb"),
      };
    }

    constexpr std::size_t kPageCapacity = 6;
    ContextMenuGrid menu = BlankGrid();
    const auto parameters = CollectDx7GroupParameters(engine, selectedParameterGroupId_);
    const std::size_t pageCount = Dx7ParameterPageCount(engine, selectedParameterGroupId_);
    const std::size_t pageStart = currentPage_ * kPageCapacity;
    const std::size_t pageEnd = std::min(parameters.size(), pageStart + kPageCapacity);
    std::size_t menuIndex = 0;
    for (std::size_t parameterIndex = pageStart; parameterIndex < pageEnd && menuIndex < kPageCapacity; ++parameterIndex, ++menuIndex)
    {
      menu[menuIndex] = MakeItemCell(Dx7ParameterMenuLabel(parameters[parameterIndex]), parameters[parameterIndex].id);
    }
    menu[6] = MakeItemCell("BACK", "par.dx7.back");
    if (pageCount > 1)
    {
      menu[7] = MakeNextPageCell(std::to_string(currentPage_ + 1) + "/" + std::to_string(pageCount));
    }
    return menu;
  }

  (void)engine;
  if ((currentPage_ % 2) == 0)
  {
    return ContextMenuGrid{
        MakeItemCell("PREV", "par.prev"),
        MakeItemCell("NEXT", "par.next"),
        MakeItemCell("FINE", "par.fine"),
        MakeItemCell("RST", "par.rst"),
        MakeItemCell("MIN", "par.min"),
        MakeItemCell("MAX", "par.max"),
        MakeItemCell("LOCK", "par.lock"),
        MakeNextPageCell("1/2"),
    };
  }

  return ContextMenuGrid{
      MakeItemCell("M1", "par.m1"),
      MakeItemCell("M2", "par.m2"),
      MakeItemCell("M3", "par.m3"),
      MakeItemCell("M4", "par.m4"),
      MakeItemCell("MAP", "par.map"),
      MakeItemCell("CLR", "par.clr"),
      MakeItemCell("SAVE", "par.save"),
      MakeNextPageCell("2/2"),
  };
}

ContextMenuGrid SynthUiModel::BuildParameterAdjustMenu() const
{
  return ContextMenuGrid{
      MakeActionCell("-1", ContextActionId::ParameterDecSmall),
      MakeActionCell("+1", ContextActionId::ParameterIncSmall),
  MakeActionCell("-10", ContextActionId::ParameterDecLarge),
      MakeActionCell("+10", ContextActionId::ParameterIncLarge),
      MakeActionCell("MIN", ContextActionId::ParameterMin),
      MakeActionCell("MAX", ContextActionId::ParameterMax),
      MakeActionCell("RST", ContextActionId::ParameterReset),
      MakeActionCell("BACK", ContextActionId::Back),
  };
}

ContextMenuGrid SynthUiModel::BuildLayoutMenu() const
{
  if ((currentPage_ % 2) == 0)
  {
    return ContextMenuGrid{
        MakeItemCell("SCL", "lay.scl"),
        MakeItemCell("ROOT", "lay.root"),
        MakeItemCell("MODE", "lay.mode"),
        MakeItemCell("CHRD", "lay.chrd"),
        MakeItemCell("OCT", "lay.oct"),
        MakeItemCell("TRNS", "lay.trns"),
        MakeItemCell("KEY", "lay.key"),
        MakeNextPageCell("1/2"),
    };
  }

  return ContextMenuGrid{
      MakeItemCell("GRID", "lay.grid"),
      MakeItemCell("KEYB", "lay.keyb"),
      MakeItemCell("DRUM", "lay.drum"),
      MakeItemCell("MPE", "lay.mpe"),
      MakeItemCell("LATCH", "lay.latch"),
      MakeItemCell("ARP", "lay.arp"),
      MakeItemCell("SAVE", "lay.save"),
      MakeNextPageCell("2/2"),
  };
}

ContextMenuGrid SynthUiModel::BuildOtherMenu() const
{
  if ((currentPage_ % 2) == 0)
  {
    return ContextMenuGrid{
        MakeItemCell("SAVE", "sys.save"),
        MakeItemCell("LOAD", "sys.load"),
        MakeItemCell("MIDI", "sys.midi"),
        MakeItemCell("SYNC", "sys.sync"),
        MakeItemCell("USB", "sys.usb"),
        MakeItemCell("SD", "sys.sd"),
        MakeItemCell("SET", "sys.set"),
        MakeNextPageCell("1/2"),
    };
  }

  return ContextMenuGrid{
      MakeItemCell("BRGT", "sys.brgt"),
      MakeItemCell("INFO", "sys.info"),
      MakeItemCell("CAL", "sys.cal"),
      MakeItemCell("TEST", "sys.test"),
      MakeItemCell("RST", "sys.rst"),
      MakeItemCell("UPD", "sys.upd"),
      MakeItemCell("BACK", "sys.back"),
      MakeNextPageCell("2/2"),
  };
}

UiModifier SynthUiModel::FirstHeldModifier() const
{
  for (std::size_t row = 0; row < heldModifiers_.size(); ++row)
  {
    if (heldModifiers_[row])
    {
      return ModifierForRow(row);
    }
  }
  return UiModifier::None;
}

}  // namespace synth