#include "OS/Storage/DeviceSettings.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <string_view>

#include "Devices/Device.h"
#include "OS/Storage/Settings.h"

namespace os::storage {

namespace {

constexpr const char* kLogKey = "sys.log";
constexpr const char* kThemeKey = "sys.theme";
constexpr std::array<const char*, kThemeCount> kThemeNames{{"dark", "light", "retro"}};

// What the app kept itself until 2026-11-23, now the board's (Device::Settings). sys.volume counted only
// at its revision 3 (a curve over 60 dB, 2026-11-06, the board's curve now); an older one was reset to the
// default anyway.
constexpr const char* kOldVolumeKey = "sys.volume";
constexpr const char* kOldVolumeRevisionKey = "sys.volume.rev";
constexpr int kOldVolumeRevision = 3;
constexpr const char* kOldLedKey = "sys.led";
constexpr const char* kOldScreenKey = "sys.screen";

struct Cache {
  bool loaded = false;
  bool log = kDefaultLogEnabled;
  Theme theme = kDefaultTheme;
};

// Tolerant like every setting: the name in any case, anything unknown is the default.
Theme ReadTheme(const Settings& system)
{
  const std::string text = system.GetString(kThemeKey);
  for (std::size_t index = 0; index < kThemeNames.size(); ++index)
  {
    const std::string_view name = kThemeNames[index];
    if (text.size() == name.size() &&
        std::equal(text.begin(), text.end(), name.begin(), [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == b; }))
      return static_cast<Theme>(index);
  }
  return kDefaultTheme;
}

Cache& Values()
{
  static Cache cache;
  if (!cache.loaded)
  {
    const Settings& system = SystemSettings();
    cache.log = system.GetBool(kLogKey, kDefaultLogEnabled);
    cache.theme = ReadTheme(system);
    cache.loaded = true;
  }
  return cache;
}

// The old values to the board, once: each only where the board has none of its own (the device decides),
// then out of system.cfg, so a board reset later never brings them back.
void CarryOverOldSettings()
{
  Settings& system = SystemSettings();
  if (!system.Has(kOldVolumeKey) && !system.Has(kOldLedKey) && !system.Has(kOldScreenKey) && !system.Has(kOldVolumeRevisionKey)) return;
  Device::Settings::OldValues old;
  if (system.Has(kOldVolumeKey) && system.GetInt(kOldVolumeRevisionKey, 0, 0, 1000) >= kOldVolumeRevision)
    old.volume = system.GetInt(kOldVolumeKey, -1, 0, 100);
  if (system.Has(kOldLedKey)) old.keyLights = system.GetInt(kOldLedKey, -1, 0, 15);
  if (system.Has(kOldScreenKey)) old.backlight = system.GetInt(kOldScreenKey, -1, 0, 255);
  Device::Settings::CarryOver(old);
  for (const char* key : {kOldVolumeKey, kOldVolumeRevisionKey, kOldLedKey, kOldScreenKey}) (void)system.Remove(key);
}

}  // namespace

void ApplyDeviceSettings()
{
  Values().loaded = false;
  (void)Values();
  CarryOverOldSettings();
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

void SetScreenTheme(Theme theme)
{
  if (static_cast<std::size_t>(theme) >= kThemeCount) theme = kDefaultTheme;
  Values().theme = theme;
  SystemSettings().SetString(kThemeKey, kThemeNames[static_cast<std::size_t>(theme)]);
}

Theme ScreenTheme()
{
  return Values().theme;
}

}  // namespace os::storage
