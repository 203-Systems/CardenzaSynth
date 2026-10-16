#pragma once

#include "Library/Synth/Core/PagedSynthEngine.h"

namespace synth {

class Dx7SynthEngine final : public PagedSynthEngine {
public:
  Dx7SynthEngine();

protected:
  std::string FormatParameterValue(const SynthParameterInfo& parameter) const override;
  int FactoryParameterValue(const std::string& presetId, const SynthParameterInfo& parameter) const override;
  std::size_t FactoryPresetCount() const override;
  std::string_view FactoryPresetId(std::size_t index) const override;
  std::string FactoryPresetName(std::size_t index) const override;
};

}  // namespace synth
