#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace synth {

// The Braids engine's own parameters, in the order it registers them (the track effects follow).
inline constexpr std::size_t kBraidsParameterCount = 19;
inline constexpr std::array<const char*, kBraidsParameterCount> kBraidsParameterIds{{
    "osc.model", "osc.timbre", "osc.color", "osc.tune", "osc.fine",
    "env.attack", "env.decay", "env.sustain", "env.release", "amp.level",
    "mod.attack", "mod.decay", "mod.timbre", "mod.color", "mod.pitch",
    "lfo.rate", "lfo.timbre", "lfo.color", "lfo.pitch"}};

// A factory preset: a value for every parameter above. They stay in flash; the engine keeps their
// sorted order only.
struct BraidsFactoryPreset {
  std::string_view id;
  std::string_view name;
  std::array<std::int8_t, kBraidsParameterCount> values;
};

inline constexpr std::size_t kBraidsFactoryPresetCount = 32;
const std::array<BraidsFactoryPreset, kBraidsFactoryPresetCount>& BraidsFactoryPresets();
const BraidsFactoryPreset* FindBraidsFactoryPreset(std::string_view presetId);
// The preset a new Braids track starts with.
inline constexpr const char* kBraidsDefaultPreset = "braids.saw";

}  // namespace synth
