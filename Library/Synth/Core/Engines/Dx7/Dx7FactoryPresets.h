#pragma once

#include <array>
#include <string_view>

namespace synth {

struct Dx7FactoryPresetDefinition {
  std::string_view id;
  std::string_view name;
  int patchNumber;
};

const std::array<Dx7FactoryPresetDefinition, 128>& Dx7FactoryPresets();
const Dx7FactoryPresetDefinition* FindDx7FactoryPreset(std::string_view presetId);

}  // namespace synth
