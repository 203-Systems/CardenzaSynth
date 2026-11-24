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
    Settings& system = SystemSettings();
    cache.log = system.GetBool(kLogKey, kDefaultLogEnabled);
    cache.theme = ReadTheme(system);
    cache.loaded = true;
  }
  return cache;
}

}  // namespace

void ApplyDeviceSettings()
{
  Values().loaded = false;
  (void)Values();
}

void SetOutputVolume(std::uint8_t percent)
{
  Device::Audio::SetOutputVolume(std::min<std::uint8_t>(percent, 100));
}

std::uint8_t OutputVolume()
{
  return Device::Audio::OutputVolume();
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
