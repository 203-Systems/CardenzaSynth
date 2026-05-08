#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "Library/Synth/Core/ISynthEngine.h"
#include "Applications/Synth/UI/Model/SynthUiTypes.h"

namespace synth::ui_model {

bool IsJunoParameterEngine(const ISynthEngine* engine);
bool IsJunoParameterGroupId(const std::string& itemId);
std::string JunoGroupIdForParameterId(const std::string& itemId);
std::string JunoGroupLabel(const std::string& groupId);
std::string JunoGroupTitle(const std::string& groupId);
std::string JunoParameterMenuLabel(const SynthParameterInfo& parameter);
std::vector<SynthParameterInfo> CollectJunoGroupParameters(const ISynthEngine* engine, const std::string& groupId);
std::string JunoGroupSummary(const ISynthEngine* engine, const std::string& groupId);
std::string JunoOverviewSummary(const ISynthEngine* engine);
bool IsJunoDirectAdjustGroupId(const std::string& groupId);
bool IsJunoDirectActionGroupId(const std::string& groupId);
bool IsJunoDiscreteAdjustParameterId(const std::string& parameterId);
void PopulateJunoSnapshotValues(UiScreenSnapshot& snapshot, const ISynthEngine* engine);
std::size_t JunoParameterPageCount(const ISynthEngine* engine, const std::string& groupId);

}  // namespace synth::ui_model