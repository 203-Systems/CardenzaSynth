#include "Library/Synth/ISynthEngine.h"

#include "Library/Synth/Dx7SynthEngine.h"
#include "Library/Synth/JunoSynthEngine.h"
#include "Library/Synth/SamplerSynthEngine.h"

namespace synth {

std::unique_ptr<ISynthEngine> CreateSynthEngine(SynthEngineKind kind)
{
  switch (kind)
  {
    case SynthEngineKind::Dx7:
      return std::make_unique<Dx7SynthEngine>();
    case SynthEngineKind::Juno:
      return std::make_unique<JunoSynthEngine>();
    case SynthEngineKind::Sampler:
      return std::make_unique<SamplerSynthEngine>();
    case SynthEngineKind::None:
    default:
      return nullptr;
  }
}

std::string ToString(SynthEngineKind kind)
{
  switch (kind)
  {
    case SynthEngineKind::Dx7:
      return "DX7";
    case SynthEngineKind::Juno:
      return "Juno";
    case SynthEngineKind::Sampler:
      return "Sampler";
    case SynthEngineKind::None:
    default:
      return "Empty";
  }
}

}  // namespace synth