#include "Library/Synth/Core/ISynthEngine.h"

#include "Library/Synth/Core/Engines/Dx7/Dx7SynthEngine.h"
#include "Library/Synth/Core/Engines/Juno/JunoSynthEngine.h"
#include "Library/Synth/Core/Engines/Sampler/SamplerSynthEngine.h"
#include "Library/Synth/Core/Engines/Drums/DrumsSynthEngine.h"
#include "Library/Synth/Core/Engines/Empty/EmptySynthEngine.h"

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
    case SynthEngineKind::Drums:
      return std::make_unique<DrumsSynthEngine>();
    case SynthEngineKind::Empty:
      return std::make_unique<EmptySynthEngine>();
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
    case SynthEngineKind::Drums:
      return "Drums";
    case SynthEngineKind::Empty:
      return "Empty";
    case SynthEngineKind::None:
    default:
      return "None";
  }
}

}  // namespace synth
