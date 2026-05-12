#pragma once

#include "Library/Synth/Core/PagedSynthEngine.h"

namespace synth {

class JunoSynthEngine final : public PagedSynthEngine {
public:
  JunoSynthEngine();

protected:
  std::string FormatParameterValue(const SynthParameterInfo& parameter) const override;
  bool ShouldSyncParameterToRuntime(const SynthParameterInfo& parameter) const override;
  bool ShouldSyncParameterOnPresetLoad(const SynthParameterInfo& parameter) const override;
};

}  // namespace synth
