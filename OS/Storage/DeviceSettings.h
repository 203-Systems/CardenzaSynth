#pragma once

#include <cstdint>

// Device-wide settings: kept in Settings/system.cfg (sys.volume, sys.led, sys.screen, sys.log,
// sys.standby, sys.standby.led), applied
// to the device at boot and shared by every app. Apps change them only through these functions; projects
// never carry them.
namespace os::storage {

inline constexpr std::uint8_t kDefaultOutputVolume = 50;
inline constexpr std::uint8_t kDefaultKeyLedBrightness = 15;
inline constexpr std::uint8_t kDefaultScreenBacklight = 180;
inline constexpr std::uint8_t kMinScreenBacklight = 10;
// The log on the card (OS/System/Log.h) is on unless someone turns it off.
inline constexpr bool kDefaultLogEnabled = true;
// Standby (the screen saver): minutes without a key before it starts (0 = never), and the style of its
// key lights (0 = each in turn; the app names them).
inline constexpr std::uint8_t kDefaultStandbyMinutes = 3;
inline constexpr std::uint8_t kMaxStandbyMinutes = 60;
inline constexpr std::uint8_t kDefaultStandbyLights = 0;
inline constexpr std::uint8_t kMaxStandbyLights = 15;

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
// Dims the screen to `percent` of its backlight setting without changing (or saving) the setting:
// standby. 100 shows it as set again.
void SetScreenDim(std::uint8_t percent);
// The console log written to the card (CardenzaSynth/Logs).
void SetLogEnabled(bool enabled);
bool LogEnabled();
// Standby: minutes without a key before it starts (0 = never), and the style of its key lights.
void SetStandbyMinutes(std::uint8_t minutes);
std::uint8_t StandbyMinutes();
void SetStandbyLights(std::uint8_t style);
std::uint8_t StandbyLights();

}  // namespace os::storage
