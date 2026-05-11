#pragma once

#include "Library/Synth/Core/PagedSynthEngine.h"

#include <string>

namespace synth {

class SamplerSynthEngine final : public PagedSynthEngine {
public:
  SamplerSynthEngine();

protected:
  std::string FormatParameterValue(const SynthParameterInfo& parameter) const override;
};

}  // namespace synth