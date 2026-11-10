#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

// The drum machine's built-in sounds and kits, from AMY's drum samples (tools/audio/drum_sounds.py
// generates the table at configure time; the sounds' order is part of the project format: a pad
// stores its sound's index). A sound is either one of AMY's ROM samples (its tiny TR-808 set,
// played by its preset number) or frames kept in flash, played in place (pcm_load_external).
namespace synth::drums {

inline constexpr std::size_t kPadCount = 8;
inline constexpr std::uint16_t kEmbedded = 0xFFFF;  // romPreset of a sound kept in flash
inline constexpr std::uint32_t kSoundRate = 22050;  // the embedded frames' rate
inline constexpr int kMaxTune = 24;                 // a pad's pitch, semitones either way

struct DrumSound {
  const char* name;         // what the screen shows, up to 14 characters
  const char* label;        // two letters for the pad
  std::uint16_t romPreset;  // AMY's ROM preset, or kEmbedded
  std::uint32_t offset;     // in the embedded frames
  std::uint32_t length;     // frames
  std::uint8_t root;        // the MIDI note it sounds at untransposed
  float gain;               // to the loudness of its kind
};

struct KitPad {
  std::uint16_t sound;
  std::int8_t tune;
};

struct DrumKit {
  const char* id;  // the drum engine's preset id
  const char* name;
  std::array<KitPad, kPadCount> pads;
};

std::size_t SoundCount();
// Out of range gives the first sound (the 808 kick).
const DrumSound& Sound(std::size_t index);
// The frames of an embedded sound, null for a ROM one.
const std::int16_t* SoundFrames(const DrumSound& sound);
std::size_t KitCount();
const DrumKit& Kit(std::size_t index);

}  // namespace synth::drums
