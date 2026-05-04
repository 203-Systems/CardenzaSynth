#pragma once

#include <cstddef>
#include <string>

namespace device {

struct SynthAudioPipelineBinding {
  std::size_t engineIndex = 0;
  bool connected = false;
  bool hasNativeOutput = false;
  std::string backendName = "none";
};

SynthAudioPipelineBinding ConnectSynthAudioPipeline(std::size_t engineIndex);

}  // namespace device