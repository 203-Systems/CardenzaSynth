#include "OS/Storage/DeviceSettings.h"

#include <algorithm>

#include "Devices/Device.h"
#include "OS/Storage/Settings.h"

namespace os::storage {

namespace {

constexpr const char* kVolumeKey = "sys.volume";
// The output got louder (limiter, 2026-11), so a volume saved before means something else: it starts
// again from the default, once. Raise this only when that happens again.
constexpr const char* kVolumeRevisionKey = "sys.volume.rev";
constexpr int kVolumeRevision = 2;
constexpr const char* kLedKey = "sys.led";
constexpr const char* kScreenKey = "sys.screen";
constexpr const char* kLogKey = "sys.log";
constexpr const char* kStandbyKey = "sys.standby";
constexpr const char* kStandbyLightsKey = "sys.standby.led";

struct Cache {
  bool loaded = false;
  std::uint8_t volume = kDefaultOutputVolume;
  std::uint8_t led = kDefaultKeyLedBrightness;
  std::uint8_t screen = kDefaultScreenBacklight;
  bool log = kDefaultLogEnabled;
  std::uint8_t standby = kDefaultStandbyMinutes;
  std::uint8_t standbyLights = kDefaultStandbyLights;
  std::uint8_t dim = 100;  // percent of the backlight shown (standby), never saved
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
    cache.log = system.GetBool(kLogKey, kDefaultLogEnabled);
    cache.standby = static_cast<std::uint8_t>(system.GetInt(kStandbyKey, kDefaultStandbyMinutes, 0, kMaxStandbyMinutes));
    cache.standbyLights = static_cast<std::uint8_t>(system.GetInt(kStandbyLightsKey, kDefaultStandbyLights, 0, kMaxStandbyLights));
    cache.loaded = true;
  }
  return cache;
}

// The backlight as set, or dimmed for standby (a little light stays on: never fully dark).
void ApplyBacklight(const Cache& cache)
{
  const int level = cache.dim >= 100 ? cache.screen : std::max(3, cache.screen * cache.dim / 100);
  Device::Display::SetBacklight(static_cast<std::uint8_t>(level));
}

void ApplyVolume(std::uint8_t percent)
{
  // The device takes half a dB per percent, so 1..100 ends up about -20..0 dB; 0 is mute.
  const int device = percent == 0 ? 0 : 100 - (100 - percent) * 42 / 100;
  Device::Audio::SetOutputVolume(static_cast<std::uint8_t>(device));
}

}  // namespace

void ApplyDeviceSettings()
{
  const std::uint8_t dim = Values().dim;
  Values().loaded = false;
  Cache& cache = Values();
  cache.dim = dim;
  ApplyVolume(cache.volume);
  Device::LED::SetGlobalBrightness(cache.led);
  ApplyBacklight(cache);
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
  ApplyBacklight(cache);
  SystemSettings().SetInt(kScreenKey, cache.screen);
}

std::uint8_t ScreenBacklight()
{
  return Values().screen;
}

void SetScreenDim(std::uint8_t percent)
{
  Cache& cache = Values();
  const std::uint8_t dim = std::min<std::uint8_t>(percent, 100);
  if (dim == cache.dim) return;
  cache.dim = dim;
  ApplyBacklight(cache);
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

void SetStandbyMinutes(std::uint8_t minutes)
{
  Cache& cache = Values();
  cache.standby = std::min(minutes, kMaxStandbyMinutes);
  SystemSettings().SetInt(kStandbyKey, cache.standby);
}

std::uint8_t StandbyMinutes()
{
  return Values().standby;
}

void SetStandbyLights(std::uint8_t style)
{
  Cache& cache = Values();
  cache.standbyLights = std::min(style, kMaxStandbyLights);
  SystemSettings().SetInt(kStandbyLightsKey, cache.standbyLights);
}

std::uint8_t StandbyLights()
{
  return Values().standbyLights;
}

}  // namespace os::storage
