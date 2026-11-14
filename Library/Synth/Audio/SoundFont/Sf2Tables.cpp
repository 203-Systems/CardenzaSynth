#include "Library/Synth/Audio/SoundFont/Sf2Tables.h"

#include <algorithm>
#include <limits>

namespace synth::audio::sf2 {

namespace {

#include "Library/Synth/Audio/SoundFont/Sf2Tables.inc"

constexpr std::int32_t kOctaveQ8 = 1200 << 8;
constexpr std::int32_t kSilentQ12 = 1440 << 12;

}  // namespace

std::uint64_t Exp2(std::int32_t centsQ8, unsigned fractionBits)
{
  std::int32_t octave = centsQ8 / kOctaveQ8;
  std::int32_t rest = centsQ8 % kOctaveQ8;
  if (rest < 0)
  {
    rest += kOctaveQ8;
    --octave;
  }
  const std::uint32_t index = static_cast<std::uint32_t>(rest) >> 8;
  const std::uint32_t fraction = static_cast<std::uint32_t>(rest) & 255u;
  const std::uint64_t mantissa = kExp2Q30[index] + ((static_cast<std::uint64_t>(kExp2Q30[index + 1] - kExp2Q30[index]) * fraction) >> 8);
  const int shift = static_cast<int>(fractionBits) - 30 + octave;
  if (shift >= 0)
  {
    if (shift > 31) return static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    return mantissa << shift;
  }
  if (shift <= -63) return 0;
  return (mantissa + (std::uint64_t{1} << (-shift - 1))) >> -shift;
}

std::uint32_t ControlStepsQ8(std::int32_t timecents)
{
  timecents = (std::clamp)(timecents, std::int32_t{-32768}, std::int32_t{12000});
  const std::uint64_t seconds = Exp2(timecents * 256, 24);
  const std::uint64_t steps = (seconds * kControlStepsPerSecondQ8) >> 24;
  return static_cast<std::uint32_t>((std::min)(steps, std::uint64_t{0x7FFFFFFF}));
}

std::uint32_t PositionStep(std::int32_t centsQ8, std::uint32_t rateRatioQ24)
{
  const std::uint64_t ratio = (std::min)(Exp2(centsQ8, 20), std::uint64_t{1} << 27);
  const std::uint64_t step = (ratio * rateRatioQ24) >> 28;
  return static_cast<std::uint32_t>((std::clamp)(step, std::uint64_t{1}, std::uint64_t{kMaxPositionStep}));
}

std::uint32_t LfoStep(std::int32_t cents)
{
  cents = (std::clamp)(cents, std::int32_t{-16000}, std::int32_t{4500});
  return static_cast<std::uint32_t>((Exp2(cents * 256, 24) * kLfoStepAtZeroCents) >> 24);
}

std::uint32_t GainQ15(std::int32_t attenuationQ12)
{
  if (attenuationQ12 <= 0) return 32768;
  if (attenuationQ12 >= kSilentQ12) return 0;
  const std::uint32_t index = static_cast<std::uint32_t>(attenuationQ12) >> 12;
  const std::uint32_t fraction = static_cast<std::uint32_t>(attenuationQ12) & 4095u;
  const std::uint32_t upper = kGainQ15[index];
  return upper - (((upper - kGainQ15[index + 1]) * fraction) >> 12);
}

std::int32_t AttenuationOfGain(std::uint32_t gainQ15)
{
  if (gainQ15 >= 32768) return 0;
  if (gainQ15 == 0) return kSilentQ12;
  // The table falls: the last entry that is still at least this gain.
  std::uint32_t low = 0;
  std::uint32_t high = 1440;
  while (low < high)
  {
    const std::uint32_t middle = (low + high + 1) / 2;
    if (kGainQ15[middle] >= gainQ15) low = middle;
    else high = middle - 1;
  }
  const std::uint32_t span = kGainQ15[low] - kGainQ15[low + 1];
  const std::uint32_t fraction = span != 0 ? ((kGainQ15[low] - gainQ15) << 12) / span : 0;
  return static_cast<std::int32_t>((low << 12) + (std::min)(fraction, std::uint32_t{4095}));
}

std::int32_t VelocityAttenuationQ12(unsigned velocity) { return static_cast<std::int32_t>(kVelocityCbQ4[(std::min)(velocity, 127u)]) << 8; }

FilterCoefficients FilterFor(std::int32_t cutoffCentsQ8, int resonanceCb)
{
  constexpr std::int32_t kLowestQ8 = 1500 << 8;
  constexpr std::int32_t kHighestQ8 = 13500 << 8;
  constexpr std::uint32_t kStepQ8 = 25 << 8;
  const std::uint32_t cutoff = static_cast<std::uint32_t>((std::clamp)(cutoffCentsQ8, kLowestQ8, kHighestQ8) - kLowestQ8);
  const std::uint32_t index = cutoff / kStepQ8;
  const std::uint32_t fraction = cutoff % kStepQ8;
  const std::uint64_t g = kFilterGQ24[index] + (static_cast<std::uint64_t>(kFilterGQ24[index + 1] - kFilterGQ24[index]) * fraction) / kStepQ8;

  const std::uint32_t resonance = static_cast<std::uint32_t>((std::clamp)(resonanceCb, 0, kMaxResonanceCb));
  const std::uint32_t upper = kResonanceKQ24[resonance / 10];
  const std::uint64_t k = upper - ((upper - kResonanceKQ24[resonance / 10 + 1]) * (resonance % 10)) / 10;

  // a1 = 1 / (1 + g (g + k)), a2 = g a1, a3 = g a2.
  const std::uint64_t denominator = (std::uint64_t{1} << 24) + ((g * (g + k)) >> 24);
  FilterCoefficients coefficients;
  const std::uint64_t a1 = (std::uint64_t{1} << 54) / denominator;
  const std::uint64_t a2 = (g * a1) >> 24;
  const std::uint64_t a3 = (g * a2) >> 24;
  coefficients.a1 = static_cast<std::int32_t>(a1);
  coefficients.a2 = static_cast<std::int32_t>(a2);
  coefficients.a3 = static_cast<std::int32_t>(a3);
  return coefficients;
}

}  // namespace synth::audio::sf2
