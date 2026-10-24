#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "Applications/Synth/Runtime/Sequencer/SynthSequencer.h"
#include "Library/Synth/Audio/SynthFx.h"
#include "Library/Synth/Core/ISynthEngine.h"

namespace synth {

static_assert(kSynthTrackCount == 4, "Mixer defaults list one level per track.");

struct ProjectData {
  // Project file v4. Version 1 files load with these mixer and backlight defaults; versions 1
  // and 2 load with an empty song; versions 1 to 3 load with the default master effects.
  struct MixerData {
    std::array<std::uint8_t, kSynthTrackCount> level{{100, 100, 100, 100}};  // 0..100
    std::array<bool, kSynthTrackCount> muted{};
    std::array<bool, kSynthTrackCount> soloed{};
    std::uint8_t master = 80;  // 0..100
  };

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
  MixerData mixer;
  std::uint8_t backlight = 180;  // 0..255
  // Master bus effects, indexed by fx::MasterParam.
  std::array<std::uint8_t, fx::kMasterParamCount> masterFx = DefaultMasterFx();

  static std::array<std::uint8_t, fx::kMasterParamCount> DefaultMasterFx()
  {
    std::array<std::uint8_t, fx::kMasterParamCount> values{};
    for (std::size_t index = 0; index < values.size(); ++index) values[index] = static_cast<std::uint8_t>(fx::kMasterParams[index].defaultValue);
    return values;
  }
};

enum class ProjectResult { Ok, Exists, Invalid, StorageError };

bool IsProjectNameValid(const std::string& name);
std::vector<std::string> ListProjects();
ProjectResult SaveProjectFile(const std::string& name, const ProjectData& project, bool overwrite);
ProjectResult LoadProjectFile(const std::string& name, ProjectData& project);

}  // namespace synth
