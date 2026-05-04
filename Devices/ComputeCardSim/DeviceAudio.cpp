#include "Devices/DeviceAudio.h"

namespace device {

SynthAudioPipelineBinding ConnectSynthAudioPipeline(std::size_t engineIndex) {
  SynthAudioPipelineBinding binding;
  binding.engineIndex = engineIndex;
  binding.backendName = "simulator";
  return binding;
}

}  // namespace device