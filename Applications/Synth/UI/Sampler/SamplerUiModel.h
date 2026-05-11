#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "Applications/Synth/Runtime/Sampler/SamplerTrackState.h"
#include "Library/Synth/Core/ISynthEngine.h"
#include "Applications/Synth/UI/Model/SynthUiTypes.h"

namespace synth::ui_model {

bool IsSamplerParameterEngine(const ISynthEngine* engine);
bool IsSamplerParameterGroupId(const std::string& itemId);
std::string SamplerGroupIdForParameterId(const std::string& itemId);
std::string SamplerGroupLabel(const std::string& groupId);
std::string SamplerGroupTitle(const std::string& groupId);
std::string SamplerParameterMenuLabel(const SynthParameterInfo& parameter);
std::vector<SynthParameterInfo> CollectSamplerGroupParameters(const ISynthEngine* engine, const std::string& groupId);
std::string SamplerGroupSummary(const ISynthEngine* engine, const SamplerTrackUiState& state, const std::string& groupId);
std::string SamplerOverviewSummary(const ISynthEngine* engine, const SamplerTrackUiState& state);
bool IsSamplerDirectAdjustGroupId(const std::string& groupId);
std::string SamplerDirectAdjustParameterId(const std::string& groupId);
bool IsSamplerDirectActionGroupId(const std::string& groupId);
void PopulateSamplerSnapshotValues(UiScreenSnapshot& snapshot, const ISynthEngine* engine, const SamplerTrackUiState& state);
std::size_t SamplerParameterPageCount(const ISynthEngine* engine, const std::string& groupId);

}  // namespace synth::ui_model