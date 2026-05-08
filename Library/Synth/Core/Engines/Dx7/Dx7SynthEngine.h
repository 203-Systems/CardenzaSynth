#pragma once

#include "Library/Synth/Core/PagedSynthEngine.h"

namespace synth {

class Dx7SynthEngine final : public PagedSynthEngine {
public:
  Dx7SynthEngine();

protected:
  std::string FormatParameterValue(const SynthParameterInfo& parameter) const override;
};

}  // namespace synth