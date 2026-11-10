#pragma once

#include <array>
#include <string>
#include "Library/Synth/Core/Engines/Drums/DrumSounds.h"
#include "Library/Synth/Core/PagedSynthEngine.h"

namespace synth {

// The eight pads (lanes), as the GM notes the sequencer stores: their order is the pads', left to
// right. What each one sounds is its own (padN.sound, padN.tune, its level), a kit (a preset) sets all.
inline constexpr std::array<std::uint8_t, 8> kDrumNotes{{36, 38, 42, 46, 39, 43, 56, 70}};
// Each pad's level: the ids are older than pads with sounds of their own, and stay.
inline constexpr std::array<const char*, 8> kDrumParameterIds{{
    "kick.level", "snare.level", "closed_hat.level", "open_hat.level",
    "clap.level", "tom.level", "cowbell.level", "maraca.level"}};
inline constexpr std::array<const char*, 8> kDrumLabels{{
    "Kick", "Snare", "Closed Hat", "Open Hat", "Clap", "Tom", "Cowbell", "Maraca"}};
inline constexpr std::array<const char*, 8> kDrumSoundIds{{
    "pad1.sound", "pad2.sound", "pad3.sound", "pad4.sound", "pad5.sound", "pad6.sound", "pad7.sound", "pad8.sound"}};
inline constexpr std::array<const char*, 8> kDrumTuneIds{{
    "pad1.tune", "pad2.tune", "pad3.tune", "pad4.tune", "pad5.tune", "pad6.tune", "pad7.tune", "pad8.tune"}};

// The pad (0..7) a padN.sound / padN.tune id names, -1 for any other id.
int DrumPadOfParameter(const std::string& id);

class DrumsSynthEngine final : public PagedSynthEngine {
public:
  DrumsSynthEngine();

protected:
  std::string FormatParameterValue(const SynthParameterInfo& parameter) const override;
  bool ShouldSyncParameterOnPresetLoad(const SynthParameterInfo&) const override { return true; }
};

}  // namespace synth
