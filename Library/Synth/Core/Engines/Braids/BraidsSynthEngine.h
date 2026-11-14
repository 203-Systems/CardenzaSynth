#pragma once

#include "Library/Synth/Core/PagedSynthEngine.h"

namespace synth {

// Mutable Instruments' Braids macro oscillator as a four-voice instrument: a MODEL and its two macro
// controls TIMBRE and COLOR, an amplitude envelope, and an AD envelope and an LFO on TIMBRE, COLOR and
// the pitch (the DSP: Library/Synth/Audio/Braids, Library/third_party/braids).
class BraidsSynthEngine final : public PagedSynthEngine {
public:
  BraidsSynthEngine();

protected:
  std::string FormatParameterValue(const SynthParameterInfo& parameter) const override;
  int FactoryParameterValue(const std::string& presetId, const SynthParameterInfo& parameter) const override;
  // A preset is its parameters and nothing else: the runtime goes back to the defaults and takes them all.
  bool ShouldSyncParameterOnPresetLoad(const SynthParameterInfo&) const override { return true; }
  std::size_t FactoryPresetCount() const override;
  std::string_view FactoryPresetId(std::size_t index) const override;
  std::string FactoryPresetName(std::size_t index) const override;
};

}  // namespace synth
