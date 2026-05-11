#include "Applications/Synth/UI/Sampler/SamplerUiModel.h"

#include "Applications/Synth/UI/Menu/SynthUiMenu.h"

#include <string>
#include <vector>

namespace synth::ui_model {

bool IsSamplerParameterEngine(const ISynthEngine* engine)
{
  return engine != nullptr && engine->Descriptor().kind == SynthEngineKind::Sampler;
}

bool IsSamplerParameterGroupId(const std::string& itemId)
{
  return itemId == "par.sampler.group.samp" || itemId == "par.sampler.group.edit" || itemId == "par.sampler.group.trim" ||
         itemId == "par.sampler.group.pitch" || itemId == "par.sampler.group.root" ||
         itemId == "par.sampler.group.loop" ||
         itemId == "par.sampler.group.amp" || itemId == "par.sampler.group.filter" || itemId == "par.sampler.group.env";
}

std::string SamplerGroupIdForParameterId(const std::string& itemId)
{
  if (itemId == "sample.start" || itemId == "sample.end")
  {
    return "par.sampler.group.trim";
  }
  if (itemId == "sample.root") return "par.sampler.group.root";
  if (itemId == "sample.loop") return "par.sampler.group.loop";
  if (itemId == "sample.gain" || itemId == "amp.level") return "par.sampler.group.amp";
  if (itemId.rfind("filter.", 0) == 0) return "par.sampler.group.filter";
  if (itemId.rfind("env.", 0) == 0) return "par.sampler.group.env";
  return {};
}

std::string SamplerGroupLabel(const std::string& groupId)
{
  if (groupId == "par.sampler.group.samp") return "FILE";
  if (groupId == "par.sampler.group.edit") return "EDIT";
  if (groupId == "par.sampler.group.trim") return "TRIM";
  if (groupId == "par.sampler.group.pitch") return "PLAY";
  if (groupId == "par.sampler.group.loop") return "LOOP";
  if (groupId == "par.sampler.group.root") return "ROOT";
  if (groupId == "par.sampler.group.amp") return "GAIN";
  if (groupId == "par.sampler.group.filter") return "FLT";
  if (groupId == "par.sampler.group.env") return "ENV";
  return "Sampler";
}

std::string SamplerGroupTitle(const std::string& groupId)
{
  if (groupId == "par.sampler.group.samp") return "File";
  if (groupId == "par.sampler.group.edit") return "Edit";
  if (groupId == "par.sampler.group.trim") return "Trim";
  if (groupId == "par.sampler.group.pitch") return "Playback";
  if (groupId == "par.sampler.group.loop") return "Loop";
  if (groupId == "par.sampler.group.root") return "Root";
  if (groupId == "par.sampler.group.amp") return "Gain";
  if (groupId == "par.sampler.group.filter") return "Filter";
  if (groupId == "par.sampler.group.env") return "Envelope";
  return "Sampler";
}

std::string SamplerParameterMenuLabel(const SynthParameterInfo& parameter)
{
  if (parameter.id == "sample.start") return "START";
  if (parameter.id == "sample.end") return "END";
  if (parameter.id == "sample.loop") return "LOOP";
  if (parameter.id == "sample.tune") return "TUNE";
  if (parameter.id == "sample.root") return "ROOT";
  if (parameter.id == "sample.gain") return "GAIN";
  if (parameter.id == "amp.level") return "LVL";
  if (parameter.id == "filter.cutoff") return "CUT";
  if (parameter.id == "filter.resonance") return "RES";
  if (parameter.id == "env.attack") return "ATK";
  if (parameter.id == "env.release") return "REL";
  return parameter.label;
}

std::vector<SynthParameterInfo> CollectSamplerGroupParameters(const ISynthEngine* engine, const std::string& groupId)
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

  if (groupId == "par.sampler.group.trim")
  {
    appendIfPresent("sample.start");
    appendIfPresent("sample.end");
    return parameters;
  }

  if (groupId == "par.sampler.group.pitch")
  {
    appendIfPresent("sample.root");
    appendIfPresent("sample.tune");
    appendIfPresent("sample.loop");
    return parameters;
  }

  if (groupId == "par.sampler.group.root")
  {
    appendIfPresent("sample.root");
    return parameters;
  }

  if (groupId == "par.sampler.group.loop")
  {
    appendIfPresent("sample.loop");
    return parameters;
  }

  if (groupId == "par.sampler.group.amp")
  {
    appendIfPresent("sample.gain");
    return parameters;
  }

  if (groupId == "par.sampler.group.filter")
  {
    appendIfPresent("filter.cutoff");
    appendIfPresent("filter.resonance");
    return parameters;
  }

  if (groupId == "par.sampler.group.env")
  {
    appendIfPresent("env.attack");
    appendIfPresent("env.release");
  }

  return parameters;
}

std::string SamplerGroupSummary(const ISynthEngine* engine, const SamplerTrackUiState& state, const std::string& groupId)
{
  if (groupId == "par.sampler.group.samp")
  {
    if (state.recording)
    {
      return "REC " + std::to_string(static_cast<int>(state.recordProgress * 100.0f)) + "%";
    }
    if (state.hasDraft)
    {
      return "DRAFT  TRIM";
    }
    return state.hasSample ? state.sampleName + "  READY" : std::string{"EMPTY  FILE"};
  }
  if (groupId == "par.sampler.group.edit")
  {
    return state.hasSample ? state.sampleName : std::string{"NO SAMPLE"};
  }
  if (groupId == "par.sampler.group.trim")
  {
    return "S " + DescribeParameterValueOrFallback(engine, "sample.start") +
           "  E " + DescribeParameterValueOrFallback(engine, "sample.end");
  }
  if (groupId == "par.sampler.group.pitch")
  {
      return "ROOT " + DescribeParameterValueOrFallback(engine, "sample.root") +
        "  " + DescribeParameterValueOrFallback(engine, "sample.loop");
  }
  if (groupId == "par.sampler.group.loop")
  {
    return DescribeParameterValueOrFallback(engine, "sample.loop");
  }
  if (groupId == "par.sampler.group.root")
  {
    return DescribeParameterValueOrFallback(engine, "sample.root");
  }
  if (groupId == "par.sampler.group.amp")
  {
    return "GAIN " + DescribeParameterValueOrFallback(engine, "sample.gain");
  }
  if (groupId == "par.sampler.group.filter")
  {
    return "CUT " + DescribeParameterValueOrFallback(engine, "filter.cutoff") +
           "  RES " + DescribeParameterValueOrFallback(engine, "filter.resonance");
  }
  if (groupId == "par.sampler.group.env")
  {
    return "A " + DescribeParameterValueOrFallback(engine, "env.attack") +
           "  R " + DescribeParameterValueOrFallback(engine, "env.release");
  }
  return "SELECT SECTION";
}

std::string SamplerOverviewSummary(const ISynthEngine* engine, const SamplerTrackUiState& state)
{
  if (!state.hasSample && !state.hasDraft)
  {
    return "FILE OR REC";
  }

  return DescribeParameterValueOrFallback(engine, "sample.root") +
         "  " + DescribeParameterValueOrFallback(engine, "sample.loop");
}

bool IsSamplerDirectActionGroupId(const std::string& groupId)
{
  (void)groupId;
  return false;
}

bool IsSamplerDirectAdjustGroupId(const std::string& groupId)
{
  return groupId == "par.sampler.group.root" ||
         groupId == "par.sampler.group.loop" || groupId == "par.sampler.group.amp";
}

std::string SamplerDirectAdjustParameterId(const std::string& groupId)
{
  if (groupId == "par.sampler.group.root") return "sample.root";
  if (groupId == "par.sampler.group.loop") return "sample.loop";
  if (groupId == "par.sampler.group.amp") return "sample.gain";
  return {};
}

void PopulateSamplerSnapshotValues(UiScreenSnapshot& snapshot, const ISynthEngine* engine, const SamplerTrackUiState& state)
{
  snapshot.samplerStart = ParameterValueOrDefault(engine, "sample.start", 0);
  snapshot.samplerEnd = ParameterValueOrDefault(engine, "sample.end", 127);
  snapshot.samplerLoop = ParameterValueOrDefault(engine, "sample.loop", 0);
  snapshot.samplerTune = ParameterValueOrDefault(engine, "sample.tune", 0);
  snapshot.samplerRoot = ParameterValueOrDefault(engine, "sample.root", 60);
  snapshot.samplerGain = ParameterValueOrDefault(engine, "sample.gain", 96);
  snapshot.samplerLevel = ParameterValueOrDefault(engine, "amp.level", 96);
  snapshot.samplerPan = ParameterValueOrDefault(engine, "amp.pan", 64);
  snapshot.samplerCutoff = ParameterValueOrDefault(engine, "filter.cutoff", 96);
  snapshot.samplerResonance = ParameterValueOrDefault(engine, "filter.resonance", 18);
  snapshot.samplerAttack = ParameterValueOrDefault(engine, "env.attack", 4);
  snapshot.samplerRelease = ParameterValueOrDefault(engine, "env.release", 18);
  snapshot.samplerSampleName = state.sampleName;
  snapshot.samplerStatusText = state.statusText;
  snapshot.samplerDraftName = state.draftName;
  snapshot.samplerSampleRate = state.sampleRate;
  snapshot.samplerFrameCount = state.frameCount;
  snapshot.samplerRecordMaxFrameCount = state.recordMaxFrameCount;
  snapshot.samplerTrimStartFrame = state.trimStartFrame;
  snapshot.samplerTrimEndFrame = state.trimEndFrame;
  snapshot.samplerPreviewCursorFrame = state.previewCursorFrame;
  snapshot.samplerWaveform = state.waveform;
  snapshot.samplerInputLevel = state.inputLevel;
  snapshot.samplerHasDraft = state.hasDraft;
  snapshot.samplerNeedsName = state.needsName;
  snapshot.samplerOverwritePending = state.overwritePending;
  snapshot.samplerDeleteConfirm = state.deleteConfirm;
  snapshot.samplerHasSample = state.hasSample;
  snapshot.samplerRecording = state.recording;
  snapshot.samplerPreviewing = state.previewing;
  snapshot.samplerRecordProgress = state.recordProgress;
}

std::size_t SamplerParameterPageCount(const ISynthEngine* engine, const std::string& groupId)
{
  if (IsSamplerDirectActionGroupId(groupId))
  {
    return 1;
  }

  const std::size_t count = CollectSamplerGroupParameters(engine, groupId).size();
  return PagedMenuPageCount(count);
}

}  // namespace synth::ui_model