#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Applications/Application.h"

namespace os::system {

struct BootOptions {
  std::vector<std::string> applicationArgs;
};

// Settings are written this long after the last change (and whenever an app is suspended).
inline constexpr std::uint32_t kSettingsWriteDelayMs = 1000;

// Boots the device, reads the settings on the card, applies the device settings and starts the boot
// app (BootApplicationId).
bool Begin(const BootOptions& options = {});
// One frame: input, the app's tick and its LEDs, then, when `draw`, its picture on the screen. The
// device runs a frame every 10 ms and draws every other one (50 fps): keys keep their 10 ms latency.
void RunFrame(bool draw = true);
app::Application* ActiveApplication();
const std::string& ActiveApplicationId();

// Asks for another app. The switch happens before the next frame, never inside an app's own call:
// the active app is suspended (Suspend, then every settings store is flushed) and destroyed, then
// the new one is created and set up. The last non-system app is remembered (Settings/system.cfg
// boot.app). False for an id that is not registered.
bool SwitchApplication(const std::string& id);
// The remembered app when it is registered and not a system app, else app::DefaultApplicationId().
std::string BootApplicationId();
// Suspends the active app and writes every setting (simulator exit, orderly power-off).
void Shutdown();

// Tests replace the wall clock (milliseconds).
void UseClock(std::uint32_t (*clock)());

}  // namespace os::system
