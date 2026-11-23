#include "Library/Synth/Core/ISynthEngine.h"

#include "Library/Synth/Core/Engines/Braids/BraidsSynthEngine.h"
#include "Library/Synth/Core/Engines/Dx7/Dx7SynthEngine.h"
#include "Library/Synth/Core/Engines/Juno/JunoSynthEngine.h"
#include "Library/Synth/Core/Engines/Sampler/SamplerSynthEngine.h"
#include "Library/Synth/Core/Engines/Chop/ChopSynthEngine.h"
#include "Library/Synth/Core/Engines/Drums/DrumsSynthEngine.h"
#include "Library/Synth/Core/Engines/Wave/WaveSynthEngine.h"
#include "Library/Synth/Core/Engines/Plaits/PlaitsSynthEngine.h"
#include "Library/Synth/Core/Engines/Vocal/VocalSynthEngine.h"
#include "Library/Synth/Core/Engines/SoundFont/SoundFontSynthEngine.h"

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
    case SynthEngineKind::Wave:
      return std::make_unique<WaveSynthEngine>();
    case SynthEngineKind::Braids:
      return std::make_unique<BraidsSynthEngine>();
    case SynthEngineKind::Plaits:
      return std::make_unique<PlaitsSynthEngine>();
    case SynthEngineKind::Vocal:
      return std::make_unique<VocalSynthEngine>();
    case SynthEngineKind::Chop:
      return std::make_unique<ChopSynthEngine>();
    case SynthEngineKind::SoundFont:
      return std::make_unique<SoundFontSynthEngine>();
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
    case SynthEngineKind::Wave:
      return "Wave";
    case SynthEngineKind::Braids:
      return "Braids";
    case SynthEngineKind::Plaits:
      return "Plaits";
    case SynthEngineKind::Vocal:
      return "Vocal";
    case SynthEngineKind::Chop:
      return "Chop";
    case SynthEngineKind::SoundFont:
      return "SoundFont";
    case SynthEngineKind::None:
    default:
      return "Empty";
  }
}

}  // namespace synth
