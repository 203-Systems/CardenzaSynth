#include "OS/Storage/DeviceSettings.h"

#include <algorithm>

#include "Devices/Device.h"
#include "OS/Storage/Settings.h"

namespace os::storage {

namespace {

constexpr const char* kVolumeKey = "sys.volume";
// A volume saved under an earlier meaning starts again from the default, once: the output got louder
// (limiter, 2026-11, revision 2), then the volume went from a 20 dB scale to a curve over 60 dB
// (2026-11-06, revision 3). Raise this only when that happens again.
constexpr const char* kVolumeRevisionKey = "sys.volume.rev";
constexpr int kVolumeRevision = 3;
constexpr const char* kLedKey = "sys.led";
constexpr const char* kScreenKey = "sys.screen";
constexpr const char* kLogKey = "sys.log";
// The log used to default to on, so a device that never touched the switch may have nothing stored
// and a device that did may have "on" stored; this revision settles both on off, once.
constexpr const char* kLogRevisionKey = "sys.log.rev";
constexpr int kLogRevision = 1;
constexpr const char* kThemeKey = "sys.theme";
constexpr int kUiThemeCount = 3;

struct Cache {
  bool loaded = false;
  std::uint8_t volume = kDefaultOutputVolume;
  std::uint8_t led = kDefaultKeyLedBrightness;
  std::uint8_t screen = kDefaultScreenBacklight;
  bool log = kDefaultLogEnabled;
  std::uint8_t theme = kDefaultUiTheme;
};

Cache& Values()
{
  static Cache cache;
  if (!cache.loaded)
  {
    Settings& system = SystemSettings();
    if (system.GetInt(kVolumeRevisionKey, 0, 0, 1000) < kVolumeRevision)
    {
      system.SetInt(kVolumeKey, kDefaultOutputVolume);
      system.SetInt(kVolumeRevisionKey, kVolumeRevision);
    }
    cache.volume = static_cast<std::uint8_t>(system.GetInt(kVolumeKey, kDefaultOutputVolume, 0, 100));
    cache.led = static_cast<std::uint8_t>(system.GetInt(kLedKey, kDefaultKeyLedBrightness, 0, Device::LED::kBrightnessMax));
    cache.screen = static_cast<std::uint8_t>(system.GetInt(kScreenKey, kDefaultScreenBacklight, kMinScreenBacklight, 255));
    if (system.GetInt(kLogRevisionKey, 0, 0, 1000) < kLogRevision)
    {
      system.SetBool(kLogKey, kDefaultLogEnabled);
      system.SetInt(kLogRevisionKey, kLogRevision);
    }
    cache.log = system.GetBool(kLogKey, kDefaultLogEnabled);
    cache.theme = static_cast<std::uint8_t>(system.GetInt(kThemeKey, kDefaultUiTheme, 0, kUiThemeCount - 1));
    cache.loaded = true;
  }
  return cache;
}

void ApplyVolume(std::uint8_t percent)
{
  // The device puts the curve on it (Device::Audio::SetOutputVolume); 0 is mute.
  Device::Audio::SetOutputVolume(percent);
}

}  // namespace

void ApplyDeviceSettings()
{
  Values().loaded = false;
  const Cache& cache = Values();
  ApplyVolume(cache.volume);
  Device::LED::SetGlobalBrightness(cache.led);
  Device::Display::SetBacklight(cache.screen);
}

void SetOutputVolume(std::uint8_t percent)
{
  Cache& cache = Values();
  cache.volume = std::min<std::uint8_t>(percent, 100);
  ApplyVolume(cache.volume);
  SystemSettings().SetInt(kVolumeKey, cache.volume);
}

std::uint8_t OutputVolume()
{
  return Values().volume;
}

void SetKeyLedBrightness(std::uint8_t level)
{
  Cache& cache = Values();
  cache.led = std::min<std::uint8_t>(level, Device::LED::kBrightnessMax);
  Device::LED::SetGlobalBrightness(cache.led);
  SystemSettings().SetInt(kLedKey, cache.led);
}

std::uint8_t KeyLedBrightness()
{
  return Values().led;
}

void SetScreenBacklight(std::uint8_t level)
{
  Cache& cache = Values();
  cache.screen = std::max<std::uint8_t>(level, kMinScreenBacklight);
  Device::Display::SetBacklight(cache.screen);
  SystemSettings().SetInt(kScreenKey, cache.screen);
}

std::uint8_t ScreenBacklight()
{
  return Values().screen;
}

void SetLogEnabled(bool enabled)
{
  Values().log = enabled;
  SystemSettings().SetBool(kLogKey, enabled);
}

bool LogEnabled()
{
  return Values().log;
}

void SetUiTheme(std::uint8_t theme)
{
  Cache& cache = Values();
  cache.theme = std::min<std::uint8_t>(theme, kUiThemeCount - 1);
  SystemSettings().SetInt(kThemeKey, cache.theme);
}

std::uint8_t UiTheme()
{
  return Values().theme;
}

}  // namespace os::storage
