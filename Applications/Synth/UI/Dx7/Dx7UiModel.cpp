#include "Applications/Synth/UI/Dx7/Dx7UiModel.h"

#include "Applications/Synth/UI/Menu/SynthUiMenu.h"

#include <algorithm>
#include <string>
#include <vector>

namespace synth::ui_model {

namespace {

std::string DescribeDx7AlgorithmPage()
{
  return "FM routing and feedback";
}

}  // namespace

std::string FormatDx7OperatorSummary(const ISynthEngine* engine, int op)
{
  (void)engine;
  (void)op;
  return "Level, ratio, and envelope";
}

std::string FormatDx7AlgorithmSummary(const ISynthEngine* engine)
{
  return DescribeParameterValueOrFallback(engine, "global.algorithm", "ALG --") +
         " FB " + DescribeParameterValueOrFallback(engine, "global.feedback");
}

std::string FormatDx7GlobalSummary(const ISynthEngine* engine)
{
  (void)engine;
  return "Transpose and shared LFO";
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
  if (groupId == "par.dx7.group.alg") return DescribeDx7AlgorithmPage();
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

std::string Dx7ParameterHelpTitle(const SynthParameterInfo& parameter)
{
  return parameter.label;
}

std::string Dx7ParameterHelpDescription(const SynthParameterInfo& parameter)
{
  if (parameter.id == "global.algorithm") return "Choose the FM routing algorithm for all six operators";
  if (parameter.id == "global.feedback") return "Set how much the feedback loop reinforces the selected operator";
  if (parameter.id == "global.transpose") return "Shift the full patch pitch in semitones";
  if (parameter.id == "global.lfo_speed") return "Set the speed of the shared DX7 LFO";
  if (parameter.id == "global.lfo_delay") return "Delay the shared LFO fade-in after note start";
  if (parameter.id == "global.lfo_pitch") return "Set how strongly the shared LFO bends pitch";
  if (parameter.id == "global.lfo_amp") return "Set how strongly the shared LFO modulates level";
  if (parameter.label == "Output") return "Set this operator's output level in the FM stack";
  if (parameter.label == "Coarse") return "Set the operator's coarse frequency ratio";
  if (parameter.label == "Fine") return "Fine-tune the operator frequency around the coarse ratio";
  if (parameter.label == "Detune") return "Offset the operator slightly sharp or flat for thickness";
  if (parameter.label == "Attack") return "Control how quickly the operator envelope rises";
  if (parameter.label == "Decay") return "Control how quickly the operator falls after attack";
  if (parameter.label == "Sustain") return "Set the held level of the operator envelope";
  if (parameter.label == "Release") return "Control how quickly the operator fades after key release";
  if (parameter.label == "Velocity") return "Set how much note velocity affects this operator";
  if (parameter.label == "Amp Mod") return "Set how strongly the shared LFO modulates this operator level";
  return {};
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
  const std::size_t count = CollectDx7GroupParameters(engine, groupId).size();
  return std::max<std::size_t>(1, (count + kDx7ParameterMenuItemCount - 1) / kDx7ParameterMenuItemCount);
}

std::size_t Dx7ParameterMenuIndex(const ISynthEngine* engine,
                                  const std::string& groupId,
                                  const std::string& parameterId,
                                  std::size_t pageIndex)
{
  const auto parameters = CollectDx7GroupParameters(engine, groupId);
  const auto iterator = std::find_if(parameters.begin(), parameters.end(), [&](const SynthParameterInfo& parameter) {
    return parameter.id == parameterId;
  });
  if (iterator == parameters.end())
  {
    return 0;
  }

  const std::size_t absoluteIndex = static_cast<std::size_t>(std::distance(parameters.begin(), iterator));
  const std::size_t pageStart = pageIndex * kDx7ParameterMenuItemCount;
  if (absoluteIndex < pageStart || absoluteIndex >= (pageStart + kDx7ParameterMenuItemCount))
  {
    return 0;
  }

  return absoluteIndex - pageStart;
}

}  // namespace synth::ui_model