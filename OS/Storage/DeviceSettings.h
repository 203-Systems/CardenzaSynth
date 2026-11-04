#pragma once

#include <cstdint>

// Device-wide settings: kept in System/settings.cfg (sys.volume, sys.led, sys.screen, sys.log), applied
// to the device at boot and shared by every app. Apps change them only through these functions; projects
// never carry them.
namespace os::storage {

inline constexpr std::uint8_t kDefaultOutputVolume = 50;
inline constexpr std::uint8_t kDefaultKeyLedBrightness = 15;
inline constexpr std::uint8_t kDefaultScreenBacklight = 180;
inline constexpr std::uint8_t kMinScreenBacklight = 10;
// The log on the card (OS/System/Log.h) is on unless someone turns it off.
inline constexpr bool kDefaultLogEnabled = true;

// Reads the system settings and applies all three to the device (boot).
void ApplyDeviceSettings();

// Output volume 0..100: 0 mutes, 1..100 spans about -20..0 dB of the output, a fifth of a dB per percent.
void SetOutputVolume(std::uint8_t percent);
std::uint8_t OutputVolume();
// Master brightness of the key LEDs, 0..15.
void SetKeyLedBrightness(std::uint8_t level);
std::uint8_t KeyLedBrightness();
// Screen backlight, kMinScreenBacklight..255.
void SetScreenBacklight(std::uint8_t level);
std::uint8_t ScreenBacklight();
// The console log written to the card (Cardenza/Logs).
void SetLogEnabled(bool enabled);
bool LogEnabled();

}  // namespace os::storage
