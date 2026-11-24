#pragma once

#include <cstdint>

// CardenzaHAL's model of the key light, pure (no ESP-IDF): the device, the simulators and the tests use it.
#include "Devices/Cardenza/CardenzaHAL/keyboard/led_light.h"

// Key LED light. Device::LED levels 0-15 are perceived brightness. CardenzaHAL drives each key at one of
// 16 linear duties (level / 15 of the time) and corrects only its global level (a separate hardware PWM on
// the LED enable pin, gamma 2). The eye is far from linear, so the device layer maps the perceived level to
// the HAL's per-key duty with gamma 2.6 (KeyLevel); every lit level stays lit. The model and its table are
// the HAL's (keyboard/led_light.h); these are the names this firmware has always used for them. The
// simulators, the Learn manual (wiki/src/components/cardenza/keys.ts) and the tests use the same model.
namespace Device::LED {

namespace light = CardenzaHAL::Keyboard::Light;

inline constexpr float kGamma = light::kPerceivedGamma;
inline constexpr const auto& kKeyLevelForPerceived = light::kLevelForPerceived;

// Perceived brightness 0-15 -> CardenzaHAL per-key level 0-15 (linear duty).
constexpr std::uint8_t KeyLevel(std::uint8_t perceived) { return light::PerceivedToLevel(perceived); }
// Fraction of the time a key at this HAL level is lit.
constexpr float KeyDuty(std::uint8_t keyLevel) { return light::LevelDuty(keyLevel); }
// The HAL's global level: fixed gamma 2.
constexpr float GlobalDuty(std::uint8_t master) { return light::GlobalDuty(master); }
// Light of a key at a perceived level and master level, in thousandths of full.
constexpr std::uint16_t LightPermille(std::uint8_t perceived, std::uint8_t master) { return light::LightPermille(perceived, master); }

}  // namespace Device::LED
