#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "Library/Synth/Core/ISynthEngine.h"
#include "Applications/Synth/UI/Model/SynthUiTypes.h"

namespace synth::ui_model {

std::string FormatDx7OperatorSummary(const ISynthEngine* engine, int op);
std::string FormatDx7AlgorithmSummary(const ISynthEngine* engine);
std::string FormatDx7GlobalSummary(const ISynthEngine* engine);
bool IsDx7ParameterEngine(const ISynthEngine* engine);
bool IsDx7ParameterGroupId(const std::string& itemId);
std::string Dx7GroupLabel(const std::string& groupId);
std::string Dx7GroupSummary(const ISynthEngine* engine, const std::string& groupId);
std::string Dx7ParameterMenuLabel(const SynthParameterInfo& parameter);
std::vector<SynthParameterInfo> CollectDx7GroupParameters(const ISynthEngine* engine, const std::string& groupId);
std::size_t Dx7ParameterPageCount(const ISynthEngine* engine, const std::string& groupId);
std::size_t Dx7ParameterMenuIndex(const ISynthEngine* engine,
                                  const std::string& groupId,
                                  const std::string& parameterId,
                                  std::size_t pageIndex);

}  // namespace synth::ui_model