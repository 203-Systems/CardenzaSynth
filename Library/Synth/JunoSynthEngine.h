#pragma once

#include "Library/Synth/PagedSynthEngine.h"

namespace synth {

class JunoSynthEngine final : public PagedSynthEngine {
public:
  JunoSynthEngine();

protected:
  std::string FormatParameterValue(const SynthParameterInfo& parameter) const override;
};

}  // namespace synth