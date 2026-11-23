#pragma once

// The SoundFont engine's own parameters (its TONE page), shared by the engine and the audio runtime:
// what the track adds to every region of the patch it plays. The patch itself is the engine's preset.

#include <algorithm>
#include <cstdint>
#include <string_view>

#include "Library/Synth/Audio/SoundFont/Sf2Tables.h"
#include "Library/Synth/Audio/SoundFont/Sf2Voice.h"

namespace synth::soundfont {

inline constexpr const char* kCutoffId = "tone.cutoff";      // 0..127: 127 leaves the patch's filter alone, less closes it
inline constexpr const char* kResonanceId = "tone.reso";     // 0..127: added to the patch's
inline constexpr const char* kAttackId = "env.attack";       // -63..63: slower (+) or faster (-) than the patch
inline constexpr const char* kReleaseId = "env.release";     // -63..63
inline constexpr const char* kTuneId = "tune.coarse";        // -24..24 semitones
inline constexpr const char* kFineId = "tune.fine";          // -50..50 cents
inline constexpr const char* kLevelId = "amp.level";         // 0..127
inline constexpr const char* kVelocityId = "amp.velocity";   // 0..127: how much the velocity sets the level, 127 as the patch says
inline constexpr std::size_t kParameterCount = 8;

inline constexpr int kDefaultLevel = 96;

struct ToneValues {
  int cutoff = 127;
  int resonance = 0;
  int attack = 0;
  int release = 0;
  int tune = 0;
  int fine = 0;
  int level = kDefaultLevel;
  int velocity = 127;
};

// False: not one of the parameters above.
inline bool SetToneValue(ToneValues& tone, std::string_view id, int value)
{
  if (id == kCutoffId) tone.cutoff = std::clamp(value, 0, 127);
  else if (id == kResonanceId) tone.resonance = std::clamp(value, 0, 127);
  else if (id == kAttackId) tone.attack = std::clamp(value, -63, 63);
  else if (id == kReleaseId) tone.release = std::clamp(value, -63, 63);
  else if (id == kTuneId) tone.tune = std::clamp(value, -24, 24);
  else if (id == kFineId) tone.fine = std::clamp(value, -50, 50);
  else if (id == kLevelId) tone.level = std::clamp(value, 0, 127);
  else if (id == kVelocityId) tone.velocity = std::clamp(value, 0, 127);
  else return false;
  return true;
}

// In the voice's units. A step of the cutoff is 75 cents (127 steps close eight octaves), of the
// resonance 2 cB, of the envelope times 150 timecents (63 steps are 250 times as long). The level
// goes with the square of the value, like a velocity; VELO scales the velocity's attenuation.
inline audio::sf2::Tone MakeTone(const ToneValues& tone)
{
  audio::sf2::Tone out;
  out.tuneCents = static_cast<std::int16_t>(tone.tune * 100 + tone.fine);
  out.cutoffCents = static_cast<std::int16_t>((tone.cutoff - 127) * 75);
  out.resonanceCb = static_cast<std::int16_t>(tone.resonance * 2);
  out.attackTimecents = static_cast<std::int16_t>(tone.attack * 150);
  out.releaseTimecents = static_cast<std::int16_t>(tone.release * 150);
  const std::uint32_t gain = static_cast<std::uint32_t>(tone.level * tone.level * 32768 / (127 * 127));
  out.attenuationCb = static_cast<std::int16_t>(audio::sf2::AttenuationOfGain(gain) >> 12);
  out.velocityDepth = static_cast<std::uint8_t>(tone.velocity);
  return out;
}

}  // namespace synth::soundfont
