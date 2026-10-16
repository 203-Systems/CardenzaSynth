#include "Applications/Synth/UI/Menu/SynthUiMenu.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

namespace synth::ui_model {

namespace {
constexpr std::array<SynthEngineKind, 4> kSelectableEngineKinds = {
  SynthEngineKind::Dx7,
  SynthEngineKind::Juno,
  SynthEngineKind::Sampler,
  SynthEngineKind::Drums,
};
constexpr std::size_t kPresetBrowserVisibleRows = 7;
constexpr std::size_t kPresetBrowserPageSize = 32;
}  // namespace

const char* ModifierDisplayName(UiModifier modifier)
{
  switch (modifier)
  {
    case UiModifier::Sound:
      return "Track";
    case UiModifier::Parameter:
      return "Sound";
    case UiModifier::Layout:
      return "Keyboard";
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
  cell.label = label.empty() ? "NEXT" : label;
  cell.fullName = "Next Page";
  cell.description = "Show more controls";
  cell.type = MenuCellType::NextPage;
  return cell;
}

ContextMenuGrid BlankGrid()
{
  return ContextMenuGrid{};
}

std::size_t PagedMenuPageCount(std::size_t itemCount)
{
  return std::max<std::size_t>(1, (itemCount + kPagedMenuItemCountWithBack - 1) / kPagedMenuItemCountWithBack);
}

ContextMenuGrid BuildPagedMenuWithBack(const std::vector<ContextMenuCell>& items, std::size_t currentPage)
{
  ContextMenuGrid menu = BlankGrid();
  const std::size_t pageCount = PagedMenuPageCount(items.size());
  const std::size_t pageIndex = pageCount > 0 ? currentPage % pageCount : 0;
  const std::size_t pageStart = pageIndex * kPagedMenuItemCountWithBack;
  const std::size_t pageEnd = std::min(items.size(), pageStart + kPagedMenuItemCountWithBack);

  for (std::size_t itemIndex = pageStart, menuIndex = 0; itemIndex < pageEnd && menuIndex < kPagedMenuItemCountWithBack;
       ++itemIndex, ++menuIndex)
  {
    menu[menuIndex] = items[itemIndex];
  }

  if (pageCount > 1)
  {
    menu[7] = MakeNextPageCell(std::to_string(pageIndex + 1) + "/" + std::to_string(pageCount));
  }
  else
  {
    menu[7] = MakeActionCell("DONE", ContextActionId::Back);
  }
  return menu;
}

SynthEngineKind StepSelectableEngineKind(SynthEngineKind currentKind, int delta)
{
  auto iterator = std::find(kSelectableEngineKinds.begin(), kSelectableEngineKinds.end(), currentKind);
  std::size_t index = iterator != kSelectableEngineKinds.end() ? static_cast<std::size_t>(iterator - kSelectableEngineKinds.begin()) : 0;
  index = static_cast<std::size_t>((static_cast<int>(index) + delta + static_cast<int>(kSelectableEngineKinds.size())) %
                                   static_cast<int>(kSelectableEngineKinds.size()));
  return kSelectableEngineKinds[index];
}

std::string ToLower(std::string value)
{
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

bool ContainsCaseInsensitive(const std::string& haystack, const std::string& needle)
{
  return needle.empty() || ToLower(haystack).find(needle) != std::string::npos;
}

std::vector<SynthPresetInfo> CollectPresetMatches(const ISynthEngine* engine, const std::string& searchText)
{
  if (engine == nullptr)
  {
    return {};
  }

  const std::string normalizedSearch = ToLower(searchText);
  std::vector<SynthPresetInfo> matches;
  for (std::size_t offset = 0;;)
  {
    const SynthPresetPage page = engine->ListPresets({offset, kPresetBrowserPageSize});
    for (const SynthPresetInfo& preset : page.items)
    {
      if (ContainsCaseInsensitive(preset.name, normalizedSearch))
      {
        matches.push_back(preset);
      }
    }

    offset += page.items.size();
    if (page.items.empty() || offset >= page.totalCount)
    {
      break;
    }
  }

  return matches;
}

std::size_t ActivePresetMatchIndex(const std::vector<SynthPresetInfo>& presets)
{
  const auto iterator = std::find_if(presets.begin(), presets.end(), [](const SynthPresetInfo& preset) {
    return preset.active;
  });
  return iterator != presets.end() ? static_cast<std::size_t>(std::distance(presets.begin(), iterator)) : 0;
}

std::string BuildPresetBrowserListText(const std::vector<SynthPresetInfo>& presets, std::size_t selectedIndex)
{
  if (presets.empty())
  {
    return "No presets";
  }

  const std::size_t clampedSelection = std::min(selectedIndex, presets.size() - 1);
  const std::size_t firstVisible = clampedSelection == 0 ? 0 : clampedSelection - 1;
  std::ostringstream stream;
  const std::size_t lastVisible = std::min(presets.size(), firstVisible + kPresetBrowserVisibleRows);
  for (std::size_t index = firstVisible; index < lastVisible; ++index)
  {
    if (index > firstVisible)
    {
      stream << '\n';
    }

    stream << (index == clampedSelection ? ">" : (presets[index].active ? "*" : " ")) << presets[index].name;
  }

  return stream.str();
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
                                             const std::string& fallback)
{
  return engine != nullptr ? engine->DescribeParameterValue(parameterId) : fallback;
}

}  // namespace synth::ui_model
