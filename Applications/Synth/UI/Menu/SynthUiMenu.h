#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "Library/Synth/Core/ISynthEngine.h"
#include "Applications/Synth/UI/Model/SynthUiTypes.h"

namespace synth::ui_model {

inline constexpr std::size_t kPagedMenuItemCountWithBack = kContextMenuCellCount - 1;

const char* ModifierDisplayName(UiModifier modifier);
UiModifier ModifierForRow(std::size_t row);
std::size_t RowForModifier(UiModifier modifier);
ContextMenuCell MakeItemCell(const std::string& label, const std::string& itemId);
ContextMenuCell MakeActionCell(const std::string& label, ContextActionId actionId);
ContextMenuCell MakeNextPageCell(const std::string& label);
ContextMenuGrid BlankGrid();
std::size_t PagedMenuPageCount(std::size_t itemCount);
ContextMenuGrid BuildPagedMenuWithBack(const std::vector<ContextMenuCell>& items, std::size_t currentPage);
SynthEngineKind StepSelectableEngineKind(SynthEngineKind currentKind, int delta);
std::string ToLower(std::string value);
bool ContainsCaseInsensitive(const std::string& haystack, const std::string& needle);
std::vector<SynthPresetInfo> CollectPresetMatches(const ISynthEngine* engine, const std::string& searchText);
std::size_t ActivePresetMatchIndex(const std::vector<SynthPresetInfo>& presets);
std::string BuildPresetBrowserListText(const std::vector<SynthPresetInfo>& presets, std::size_t selectedIndex);
bool ReadParameter(const ISynthEngine* engine, const std::string& parameterId, SynthParameterInfo& parameter);
int ParameterValueOrDefault(const ISynthEngine* engine, const std::string& parameterId, int fallback);
std::string DescribeParameterValueOrFallback(const ISynthEngine* engine,
                                             const std::string& parameterId,
                                             const std::string& fallback = "--");

}  // namespace synth::ui_model