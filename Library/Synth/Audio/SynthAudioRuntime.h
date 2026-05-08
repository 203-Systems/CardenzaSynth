#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "Library/Synth/Core/SynthTypes.h"

namespace synth::audio {

inline constexpr std::uint32_t kSampleRate = 44100;

void EnsureEngine(std::size_t engineIndex, SynthEngineKind kind);
void SetEnginePreset(std::size_t engineIndex, std::string_view presetId);
void SetEngineParameter(std::size_t engineIndex, std::string_view parameterId, int value);
void NoteOn(std::size_t engineIndex, std::uint8_t note, std::uint8_t velocity);
void NoteOff(std::size_t engineIndex, std::uint8_t note);
void AllNotesOff(std::size_t engineIndex);
std::size_t RenderFrames(std::int16_t* destination, std::size_t frameCount);

}  // namespace synth::audio