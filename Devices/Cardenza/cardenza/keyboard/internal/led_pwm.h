#pragma once

#include "../types.h"

namespace CardenzaHAL::Keyboard::detail {

// Four fast phases keep the dimmest steady light above an 800 Hz carrier.
inline constexpr uint8_t kLedPwmPhaseCount = 4;
// Keep the original four-row-cycle key sampling cadence independently of PWM.
inline constexpr uint8_t kKeyScanNotifyPhaseInterval = 4;
static_assert(kLedPwmPhaseCount % kKeyScanNotifyPhaseInterval == 0);

constexpr uint8_t LedDuty(uint8_t level, uint8_t globalBrightness)
{
    const unsigned local = level > LED_BRIGHTNESS_MAX ? LED_BRIGHTNESS_MAX : level;
    const unsigned global = globalBrightness > LED_BRIGHTNESS_MAX ? LED_BRIGHTNESS_MAX : globalBrightness;
    constexpr unsigned denominator = LED_BRIGHTNESS_MAX * LED_BRIGHTNESS_MAX;
    const unsigned numerator = local * global * kLedPwmPhaseCount;
    const unsigned rounded = (numerator + denominator / 2) / denominator;
    // A nonzero locator remains visible at reduced master brightness.
    return static_cast<uint8_t>(numerator != 0 && rounded == 0 ? 1 : rounded);
}

constexpr uint8_t LedPhaseRank(uint8_t phase)
{
    return static_cast<uint8_t>(((phase & 1u) << 1) | ((phase & 2u) >> 1));
}

constexpr bool LedPhaseOn(uint8_t level, uint8_t globalBrightness, uint8_t phase)
{
    return LedPhaseRank(phase) < LedDuty(level, globalBrightness);
}

constexpr bool NotifyKeysAtPhase(uint8_t phase)
{
    return phase % kKeyScanNotifyPhaseInterval == 0;
}

}  // namespace CardenzaHAL::Keyboard::detail
