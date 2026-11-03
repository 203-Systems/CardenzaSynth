#include "OS/System/System.h"

#include <chrono>
#include <cstdio>
#include <memory>
#include <utility>

#include "Applications/BuiltInApplications.h"
#include "Devices/Device.h"
#include "OS/Storage/DeviceSettings.h"
#include "OS/Storage/Migration.h"
#include "OS/Storage/Settings.h"
#include "OS/Storage/Storage.h"
#include "OS/System/Log.h"

namespace os::system {

namespace {

constexpr const char* kBootAppKey = "boot.app";
constexpr const char* kProtocolKey = "protocol";
// While the newest settings wait in internal flash, the card is tried this often.
constexpr std::uint32_t kCardSyncRetryMs = 10000;

struct SystemState {
  std::unique_ptr<app::Application> application;
  std::string activeId;
  std::string pendingId;
  std::vector<std::string> bootArgs;
  Device::Keyboard::MatrixKeyState previousKeys{};
  // Keys held when the app changed: the new app sees neither their press nor their release.
  Device::Keyboard::MatrixKeyState ignoredKeys{};
  bool previousHome = false;
  // The current HOME press belongs to the app that ran before a switch.
  bool ignoreHome = false;
  bool started = false;
  std::uint32_t (*clock)() = nullptr;
  std::uint32_t settingsRevision = 0;
  std::uint32_t settingsChangedAt = 0;
  std::uint32_t cardSyncTriedAt = 0;
};

SystemState& State() {
  static SystemState state;
  return state;
}

// Every physical key change also goes to the USB console as one line, "@K<row>,<column>,<0|1>"
// ("@H<0|1>" for HOME): the Learn manual reads them over Web Serial and plays them on its live
// device (tools/learn/learn.js). Raw changes, so the mirror sees every press and its release.
void ReportKey(std::size_t row, std::size_t column, bool pressed) {
#if defined(ESP_PLATFORM)
  std::printf("@K%u,%u,%u\n", static_cast<unsigned>(row), static_cast<unsigned>(column), pressed ? 1u : 0u);
#else
  (void)row;
  (void)column;
  (void)pressed;
#endif
}

void ReportHome(bool pressed) {
#if defined(ESP_PLATFORM)
  std::printf("@H%u\n", pressed ? 1u : 0u);
#else
  (void)pressed;
#endif
}

std::uint32_t Now() {
  const auto& state = State();
  if (state.clock != nullptr) return state.clock();
  using namespace std::chrono;
  return static_cast<std::uint32_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

const app::ApplicationInfo* FindApplication(const std::string& id) {
  const auto& registry = app::Registry();
  const auto entry = registry.find(id);
  return entry == registry.end() ? nullptr : &entry->second;
}

void UpdateApplicationLeds(app::Application& application) {
  Device::LED::BeginFrame();
  Device::LED::Clear();
  application.UpdateLeds();
  Device::LED::Present();
}

void DispatchMatrixKeyEvents(app::Application& application) {
  auto& state = State();
  const Device::Keyboard::MatrixKeyState& currentState = Device::Keyboard::State();

  for (std::size_t index = 0; index < currentState.size(); ++index)
  {
    if (currentState[index] == state.previousKeys[index])
    {
      continue;
    }
    ReportKey(index / static_cast<std::size_t>(Device::Keyboard::kColumns), index % static_cast<std::size_t>(Device::Keyboard::kColumns),
              currentState[index]);
    if (state.ignoredKeys[index])
    {
      state.ignoredKeys.reset(index);
      continue;
    }

    const std::size_t row = index / static_cast<std::size_t>(Device::Keyboard::kColumns);
    const std::size_t column = index % static_cast<std::size_t>(Device::Keyboard::kColumns);
    application.HandleMatrixKeyEvent(row, column, currentState[index]);
  }

  state.previousKeys = currentState;
}

void DispatchHomeButtonEvents(app::Application& application) {
  auto& state = State();
  const bool pressed = Device::Buttons::HomePressed();
  if (pressed == state.previousHome)
  {
    return;
  }
  ReportHome(pressed);
  state.previousHome = pressed;
  if (state.ignoreHome)
  {
    if (!pressed) state.ignoreHome = false;
    return;
  }
  application.HandleHomeButtonEvent(pressed);
}

void RememberApplication(const std::string& id) {
  const app::ApplicationInfo* info = FindApplication(id);
  if (info != nullptr && !info->isSystem) (void)os::storage::SystemSettings().SetString(kBootAppKey, id);
}

bool StartApplication(const std::string& id, const std::vector<std::string>& args) {
  auto application = app::CreateApplication(id);
  if (!application) return false;
  auto& state = State();
  state.application = std::move(application);
  state.activeId = id;
  state.application->Setup(args);
  RememberApplication(id);
  return true;
}

void PerformSwitch(const std::string& id) {
  auto& state = State();
  if (id == state.activeId && state.application) return;
  const std::string from = state.activeId;
  if (state.application)
  {
    state.application->Suspend();
    (void)os::storage::FlushAllSettings();
    (void)os::log::Flush();
    // Destroy before create: the next app gets the whole heap.
    state.application.reset();
    os::storage::ReleaseAppSettings(from);
  }
  state.activeId.clear();

  Device::LED::BeginFrame();
  Device::LED::Clear();
  Device::LED::Present();
  Device::Display::Clear();

  bool started = StartApplication(id, {});
  if (!started && !from.empty()) started = StartApplication(from, {});
  // Neither the new nor the old app came up: fall back to the default app.
  if (!started) (void)StartApplication(app::DefaultApplicationId(), {});

  state.ignoredKeys = Device::Keyboard::State();
  state.previousKeys = state.ignoredKeys;
  state.ignoreHome = Device::Buttons::HomePressed();
  state.previousHome = state.ignoreHome;
  (void)os::storage::FlushAllSettings();
}

void WriteBehindSettings() {
  auto& state = State();
  const std::uint32_t now = Now();
  const std::uint32_t revision = os::storage::SettingsRevision();
  if (revision != state.settingsRevision)
  {
    state.settingsRevision = revision;
    state.settingsChangedAt = now;
  }
  if (os::storage::AnySettingsDirty())
  {
    if (now - state.settingsChangedAt < kSettingsWriteDelayMs) return;
    // On failure try again after another delay instead of every frame.
    if (!os::storage::FlushAllSettings()) state.settingsChangedAt = now;
    return;
  }
  if (os::storage::AnySettingsPendingCardSync() && now - state.cardSyncTriedAt >= kCardSyncRetryMs)
  {
    state.cardSyncTriedAt = now;
    (void)os::storage::FlushAllSettings();
  }
}

}  // namespace

bool Begin(const BootOptions& options) {
  auto& state = State();
  if (state.started)
  {
    return state.application != nullptr;
  }

  if (!Device::Init())
  {
    return false;
  }

  app::RegisterBuiltInApplications();

  // The card: protocol version, the pre-protocol layout moved into place, the system folders.
  auto& settings = os::storage::SystemSettings();
  (void)settings.SetInt(kProtocolKey, os::storage::kProtocolVersion);
  (void)os::storage::MigrateLegacyLayout(os::storage::AppFolder(app::DefaultApplicationId()));
  (void)os::storage::EnsureSystemLayout();
  os::storage::ApplyDeviceSettings();
  // The end of the previous run (a crash too) goes to the card before anything can go wrong again.
  (void)os::log::Flush();

  const std::string bootId = BootApplicationId();
  if (!StartApplication(bootId, options.applicationArgs) &&
      (bootId == app::DefaultApplicationId() || !StartApplication(app::DefaultApplicationId(), options.applicationArgs)))
  {
    return false;
  }

  Device::Start();
  state.previousKeys = Device::Keyboard::State();
  state.previousHome = Device::Buttons::HomePressed();
  state.started = true;
  (void)os::storage::FlushAllSettings();
  state.settingsRevision = os::storage::SettingsRevision();

  UpdateApplicationLeds(*state.application);
  Device::Display::Clear();
  state.application->RenderFrame();
  Device::Display::Present();
  return true;
}

void RunFrame() {
  auto& state = State();
  if (!state.pendingId.empty())
  {
    const std::string id = std::move(state.pendingId);
    state.pendingId.clear();
    PerformSwitch(id);
  }

  app::Application* application = ActiveApplication();
  if (application == nullptr)
  {
    return;
  }

  DispatchMatrixKeyEvents(*application);
  DispatchHomeButtonEvents(*application);
  application->Tick();
  UpdateApplicationLeds(*application);
  application->RenderFrame();
  Device::Display::Present();
  WriteBehindSettings();
  os::log::Service(Now());
}

app::Application* ActiveApplication() {
  return State().application.get();
}

const std::string& ActiveApplicationId() {
  return State().activeId;
}

bool SwitchApplication(const std::string& id) {
  const app::ApplicationInfo* info = FindApplication(id);
  if (info == nullptr || info->factory == nullptr) return false;
  State().pendingId = id;
  return true;
}

std::string BootApplicationId() {
  const std::string remembered = os::storage::SystemSettings().GetString(kBootAppKey);
  const app::ApplicationInfo* info = remembered.empty() ? nullptr : FindApplication(remembered);
  return info != nullptr && !info->isSystem && info->factory != nullptr ? remembered : app::DefaultApplicationId();
}

void Shutdown() {
  auto& state = State();
  if (state.application)
  {
    state.application->Suspend();
    (void)os::storage::FlushAllSettings();
    state.application.reset();
    os::storage::ReleaseAppSettings(state.activeId);
    state.activeId.clear();
  }
  (void)os::storage::FlushAllSettings();
  (void)os::log::Flush();
}

void UseClock(std::uint32_t (*clock)()) {
  State().clock = clock;
}

}  // namespace os::system
