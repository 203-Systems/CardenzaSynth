#pragma once

#include <array>
#include <string_view>

namespace synth {

struct JunoFactoryPresetDefinition {
  std::string_view id;
  std::string_view name;
  int patchNumber;
};

const std::array<JunoFactoryPresetDefinition, 128>& JunoFactoryPresets();
const JunoFactoryPresetDefinition* FindJunoFactoryPreset(std::string_view presetId);

}  // namespace synth
