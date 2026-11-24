#pragma once

#include <cstddef>
#include <cstdint>

// Device-wide settings of this firmware: kept in Settings/system.cfg (sys.log, sys.theme), applied at
// boot and shared by every app. Apps change them only through these functions; projects never carry
// them.
//
// The output volume, the key lights' master level and the screen's backlight are the board's, not ours
// (since 2026-11-23, the user: "从 SETUP 里去掉"): CardenzaHAL's settings, one value in every firmware,
// changed in the HOME menu and over USB and kept by the HAL; the device facade reads and sets them
// (Device::Audio::SetOutputVolume, Device::LED::SetGlobalBrightness). The sys.volume, sys.led and
// sys.screen an older firmware wrote in system.cfg are left there unread.
namespace os::storage {

// The board's default volume: about -10 dB on its curve (Device::Audio::SetOutputVolume).
inline constexpr std::uint8_t kDefaultOutputVolume = 70;
// The log on the card (OS/System/Log.h) is on unless someone turns it off.
inline constexpr bool kDefaultLogEnabled = true;

// The screen's colors. DARK is light on black (the shipped screen), LIGHT the same colors as ink on
// white paper, RETRO a backlit green LCD. Stored by name (`dark`, `light`, `retro`); anything else
// reads as the default.
enum class Theme : std::uint8_t {
  Dark,
  Light,
  Retro,
};
inline constexpr std::size_t kThemeCount = 3;
inline constexpr Theme kDefaultTheme = Theme::Dark;

// Reads the system settings again (boot, a card that came back).
void ApplyDeviceSettings();

// The board's output volume, 0..100 by ear (Device::Audio::SetOutputVolume): the groovebox's volume keys
// and its MIX master change the same value as the HOME menu.
void SetOutputVolume(std::uint8_t percent);
std::uint8_t OutputVolume();
// The console log written to the card (Apps/CardenzaSynth/Logs).
void SetLogEnabled(bool enabled);
bool LogEnabled();
// The screen theme. The app that draws applies it (only it knows its colors).
void SetScreenTheme(Theme theme);
Theme ScreenTheme();

}  // namespace os::storage
