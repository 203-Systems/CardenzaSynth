#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "Applications/Synth/Runtime/Sequencer/SynthSequencer.h"
#include "Library/Synth/Core/ISynthEngine.h"

namespace synth {

struct ProjectData {
  std::array<SynthEngineKind, kSynthTrackCount> kinds{};
  std::array<SynthEngineState, kSynthTrackCount> engines{};
  std::array<std::string, kSynthTrackCount> samples{};
  std::unique_ptr<SynthSequencer::ProjectState> sequence;
  std::size_t activeTrack = 0;
  int octave = 0;
  int root = 0;
  std::size_t scale = 1;
  int layout = 1;
  int ledBrightness = 15;
};

enum class ProjectResult { Ok, Exists, Invalid, StorageError };

bool IsProjectNameValid(const std::string& name);
std::vector<std::string> ListProjects();
ProjectResult SaveProjectFile(const std::string& name, const ProjectData& project, bool overwrite);
ProjectResult LoadProjectFile(const std::string& name, ProjectData& project);

}  // namespace synth
