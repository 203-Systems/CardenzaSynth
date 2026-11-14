#pragma once

#include "Library/Synth/Core/PagedSynthEngine.h"

namespace synth {

// Two oscillators over AMY's wavetables and classic shapes, with unison, a filter, two envelopes and
// an LFO (Library/Synth/Core/Engines/Wave/WaveParameters.h; the voice is AMY's, built by the audio
// runtime).
class WaveSynthEngine final : public PagedSynthEngine {
public:
  WaveSynthEngine();

protected:
  std::string FormatParameterValue(const SynthParameterInfo& parameter) const override;
  int FactoryParameterValue(const std::string& presetId, const SynthParameterInfo& parameter) const override;
  std::size_t FactoryPresetCount() const override;
  std::string_view FactoryPresetId(std::size_t index) const override;
  std::string FactoryPresetName(std::size_t index) const override;
};

}  // namespace synth
