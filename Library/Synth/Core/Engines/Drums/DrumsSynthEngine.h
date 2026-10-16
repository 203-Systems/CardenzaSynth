#pragma once

#include <array>
#include "Library/Synth/Core/PagedSynthEngine.h"

namespace synth {

inline constexpr std::array<std::uint8_t, 8> kDrumNotes{{36, 38, 42, 46, 39, 43, 56, 70}};
inline constexpr std::array<const char*, 8> kDrumParameterIds{{
    "kick.level", "snare.level", "closed_hat.level", "open_hat.level",
    "clap.level", "tom.level", "cowbell.level", "maraca.level"}};
inline constexpr std::array<const char*, 8> kDrumLabels{{
    "Kick", "Snare", "Closed Hat", "Open Hat", "Clap", "Tom", "Cowbell", "Maraca"}};

class DrumsSynthEngine final : public PagedSynthEngine {
public:
  DrumsSynthEngine();

protected:
  std::string FormatParameterValue(const SynthParameterInfo& parameter) const override;
  bool ShouldSyncParameterOnPresetLoad(const SynthParameterInfo&) const override { return true; }
};

}  // namespace synth
