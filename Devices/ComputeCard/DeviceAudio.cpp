#include "Devices/DeviceAudio.h"

#include "Devices/ComputeCard/main/hal/computecard/audio/audio.h"

namespace device {

SynthAudioPipelineBinding ConnectSynthAudioPipeline(std::size_t engineIndex) {
  SynthAudioPipelineBinding binding;
  binding.engineIndex = engineIndex;
  binding.backendName = "computecard-hal";

  const esp_err_t status = ComputeCardHAL::Audio::Init();
  binding.connected = status == ESP_OK;
  binding.hasNativeOutput = ComputeCardHAL::Audio::GetAudioChannel() != nullptr;
  return binding;
}

}  // namespace device