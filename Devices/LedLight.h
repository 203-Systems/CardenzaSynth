#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

#include "Devices/Device.h"

// Key LED light. Device::LED levels 0-15 are perceived brightness. CardenzaHAL (Devices/Cardenza/
// CardenzaHAL, keyboard/internal/led_brightness.h; a read-only dependency) drives each key at one of 16
// linear duties (level / 15 of the time) and corrects only its global level (a separate hardware PWM
// on the LED enable pin, (global / 15)^2). The eye is far from linear, so the device layer maps the
// perceived level to the HAL's per-key duty with gamma 2.6 (KeyLevel); every lit level stays lit.
// The simulators, the Learn manual and the tests use the same model. Check it when the HAL changes.
namespace Device::LED {

inline constexpr float kGamma = 2.6f;
// round(15 * (perceived / 15) ^ 2.6), at least 1 for a lit key.
inline constexpr std::array<std::uint8_t, kBrightnessMax + 1> kKeyLevelForPerceived{
    {0, 1, 1, 1, 1, 1, 1, 2, 3, 4, 5, 7, 8, 10, 13, 15}};

// Perceived brightness 0-15 -> CardenzaHAL per-key level 0-15 (linear duty).
constexpr std::uint8_t KeyLevel(std::uint8_t perceived) {
  return kKeyLevelForPerceived[std::min(perceived, kBrightnessMax)];
}

// Fraction of the time a key at this HAL level is lit.
constexpr float KeyDuty(std::uint8_t keyLevel) {
  return static_cast<float>(std::min(keyLevel, kBrightnessMax)) / kBrightnessMax;
}

// The HAL's global level: fixed gamma 2.
constexpr float GlobalDuty(std::uint8_t master) {
  const float global = static_cast<float>(std::min(master, kBrightnessMax)) / kBrightnessMax;
  return global * global;
}

// Light of a key at a perceived level and master level, in thousandths of full.
constexpr std::uint16_t LightPermille(std::uint8_t perceived, std::uint8_t master) {
  return static_cast<std::uint16_t>(KeyDuty(KeyLevel(perceived)) * GlobalDuty(master) * 1000.0f + 0.5f);
}

}  // namespace Device::LED
