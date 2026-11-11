#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

#include "Devices/Cardenza/CardenzaHAL/keyboard/led_light.h"
#include "Devices/Device.h"

// Key LED light. Device::LED levels 0-15 are perceived brightness. CardenzaHAL drives each key at one
// of 16 linear duties and corrects only its global level (gamma 2); its keyboard/led_light.h maps a
// perceived level to the duty (gamma 2.6, every lit level stays lit) and gives the light a key shows.
// The device layer, the simulators, the Learn manual and the tests all use that one model.
namespace Device::LED {

inline constexpr float kGamma = CardenzaHAL::Keyboard::Light::kPerceivedGamma;

// Perceived brightness 0-15 -> CardenzaHAL per-key level 0-15 (linear duty).
constexpr std::uint8_t KeyLevel(std::uint8_t perceived) {
  return CardenzaHAL::Keyboard::Light::PerceivedToLevel(perceived);
}

// Fraction of the time a key at this HAL level is lit.
constexpr float KeyDuty(std::uint8_t keyLevel) {
  return CardenzaHAL::Keyboard::Light::LevelDuty(keyLevel);
}

// The HAL's global level: its gamma-2 curve.
constexpr float GlobalDuty(std::uint8_t master) {
  return CardenzaHAL::Keyboard::Light::GlobalDuty(master);
}

// Light of a key at a perceived level and master level, in thousandths of full.
constexpr std::uint16_t LightPermille(std::uint8_t perceived, std::uint8_t master) {
  return CardenzaHAL::Keyboard::Light::LightPermille(perceived, master);
}

}  // namespace Device::LED
