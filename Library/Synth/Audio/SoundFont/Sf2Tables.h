#pragma once

// The SoundFont's units as integers. Nothing here uses floating point or a library's pow, so the
// host tests and the board get the same numbers (tools/audio/gen_sf2_tables.py writes the tables).

#include <cstdint>

namespace synth::audio::sf2 {

// 2^(cents / 1200) with `fractionBits` bits after the point; `centsQ8` is in 1/256 cent. Held at
// 2^63 - 1 when it does not fit.
std::uint64_t Exp2(std::int32_t centsQ8, unsigned fractionBits);

// A time in timecents as control steps, Q8 (a control step is kControlFrames frames).
std::uint32_t ControlStepsQ8(std::int32_t timecents);

// What a voice adds to its position per output frame, Q16.16: 2^(cents / 1200) times the sample's
// rate over the output's (Q24). At least 1, at most 64 frames a frame.
std::uint32_t PositionStep(std::int32_t centsQ8, std::uint32_t rateRatioQ24);
inline constexpr std::uint32_t kMaxPositionStep = 64u << 16;

// An LFO's phase step per control step for a frequency in absolute cents (a turn is 2^32).
std::uint32_t LfoStep(std::int32_t cents);

// Gain (Q15, 32768 is unity) of an attenuation in 1/4096 cB. Silent from 1440 cB on.
std::uint32_t GainQ15(std::int32_t attenuationQ12);
// The attenuation (1/4096 cB) whose gain is `gainQ15`, for a note let go while it still rises.
std::int32_t AttenuationOfGain(std::uint32_t gainQ15);
// The default velocity curve, 400 log10(127 / velocity) cB, in 1/4096 cB.
std::int32_t VelocityAttenuationQ12(unsigned velocity);

// The lowpass's coefficients, Q30, for a cutoff in 1/256 absolute cents and a resonance in cB.
struct FilterCoefficients {
  std::int32_t a1 = 0;
  std::int32_t a2 = 0;
  std::int32_t a3 = 0;
};
FilterCoefficients FilterFor(std::int32_t cutoffCentsQ8, int resonanceCb);
inline constexpr int kMaxResonanceCb = 240;  // what the filter's fixed-point state has headroom for

}  // namespace synth::audio::sf2
