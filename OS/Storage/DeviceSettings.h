#pragma once

#include <cstddef>
#include <cstdint>

// Device-wide settings of the app's own: kept in Settings/system.cfg (sys.log, sys.theme), applied at boot and
// shared by every app. Apps change them only through these functions; projects never carry them.
//
// The output volume, the screen's and the key lights' brightness, the microphone's gain and the language are
// the board's since 2026-11-23: CardenzaHAL keeps them in the board's flash, the same in every firmware, and
// they are changed in its system menu (HOME > Settings, Device::Settings). The app no longer keeps them; its
// old sys.volume, sys.led and sys.screen are carried over to the board once (ApplyDeviceSettings).
namespace os::storage {

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

// Reads the system settings (boot, and a card that comes back). The volume, key lights and backlight the app
// kept until 2026-11-23 go to the board's settings, each only where the board has never stored its own
// (Device::Settings::CarryOver), and leave system.cfg.
void ApplyDeviceSettings();

// The console log written to the card (Apps/CardenzaSynth/Logs).
void SetLogEnabled(bool enabled);
bool LogEnabled();
// The screen theme. The app that draws applies it (only it knows its colors).
void SetScreenTheme(Theme theme);
Theme ScreenTheme();

}  // namespace os::storage
