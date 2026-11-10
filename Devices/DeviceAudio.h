#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace device {

inline constexpr std::uint32_t kDefaultSynthAudioSampleRate = 44100;

// Renders `frameCount` mono frames into `destination`; `computer`, when not null (a computer records from
// the device over USB), receives what it is to get: the same sound or a mix of its own.
using SynthAudioRenderCallback = std::size_t (*)(std::int16_t* destination, std::size_t frameCount, std::int16_t* computer);

struct SynthAudioRenderProvider {
  std::uint32_t sampleRate = kDefaultSynthAudioSampleRate;
  SynthAudioRenderCallback renderFrames = nullptr;
};

struct SynthAudioPipelineBinding {
  std::size_t engineIndex = 0;
  bool connected = false;
  bool hasNativeOutput = false;
  std::string backendName = "none";
};

void SetSynthAudioRenderProvider(SynthAudioRenderProvider provider);
SynthAudioPipelineBinding ConnectSynthAudioPipeline(std::size_t engineIndex);

}  // namespace device