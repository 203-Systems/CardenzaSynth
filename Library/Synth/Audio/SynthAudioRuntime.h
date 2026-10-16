#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

#include "Library/Synth/Core/SynthTypes.h"

namespace synth::audio {

inline constexpr std::uint32_t kSampleRate = 44100;

// A published sample is immutable; replacing it only transfers shared ownership
// at the audio boundary, without allocating or copying PCM in the render loop.
struct SampleBuffer {
  std::unique_ptr<std::int16_t[]> frames;
  std::size_t frameCount = 0;
};
using SharedSampleBuffer = std::shared_ptr<const SampleBuffer>;

void EnsureEngine(std::size_t engineIndex, SynthEngineKind kind);
void SetEnginePreset(std::size_t engineIndex, std::string_view presetId);
void SetEngineParameter(std::size_t engineIndex, std::string_view parameterId, int value);
void SetSamplerFrames(std::size_t engineIndex, SharedSampleBuffer frames, std::uint32_t sampleRate);
void NoteOn(std::size_t engineIndex, std::uint8_t note, std::uint8_t velocity);
void NoteOff(std::size_t engineIndex, std::uint8_t note);
void AllNotesOff(std::size_t engineIndex);
void WarmUp();
std::size_t RenderFrames(std::int16_t* destination, std::size_t frameCount);

}  // namespace synth::audio
