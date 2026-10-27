#include "OS/System/System.h"

#include <chrono>
#include <memory>
#include <utility>

#include "Applications/BuiltInApplications.h"
#include "Devices/Device.h"
#include "OS/Storage/DeviceSettings.h"
#include "OS/Storage/Settings.h"
#include "OS/Storage/Storage.h"

namespace os::system {

namespace {

constexpr const char* kBootAppKey = "boot.app";
constexpr const char* kProtocolKey = "protocol";
// Keep failed mounts/writes out of the per-frame path. Unwritten settings remain in RAM only.
constexpr std::uint32_t kCardRetryMs = 5000;

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
  std::uint32_t cardTriedAt = 0;
  std::uint32_t settingsTriedAt = 0;
  bool cardReady = false;
  bool settingsWriteFailed = false;
};

SystemState& State() {
  static SystemState state;
  return state;
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

bool PrepareCard(bool force = false) {
  auto& state = State();
  if (!os::storage::CardMounted()) state.cardReady = false;
  if (state.cardReady) return true;
  const std::uint32_t now = Now();
  if (!force && now - state.cardTriedAt < kCardRetryMs) return false;
  state.cardTriedAt = now;
  if (!os::storage::CardPresent()) return false;
  // Read only the current layout and keep unsaved edits when a card becomes available.
  // Earlier layouts are never imported, moved or removed automatically.
  if (!os::storage::EnsureSystemLayout()) return false;
  // One unreadable store must not block healthy stores. Refresh keeps the previous RAM snapshot
  // on failure, and each store independently refuses unsafe reads/writes in Flush().
  (void)os::storage::RefreshAllSettingsFromCard();
  (void)os::storage::SystemSettings().SetInt(kProtocolKey, os::storage::kProtocolVersion);
  state.cardReady = true;
  os::storage::ApplyDeviceSettings();
  return true;
}

bool FlushSettings(bool force = false) {
  auto& state = State();
  const bool prepared = PrepareCard(force);
  state.settingsTriedAt = Now();
  state.settingsWriteFailed = !prepared || !os::storage::FlushAllSettings();
  if (state.settingsWriteFailed) state.cardReady = false;
  return !state.settingsWriteFailed;
}

void PerformSwitch(const std::string& id) {
  auto& state = State();
  if (id == state.activeId && state.application) return;
  const std::string from = state.activeId;
  if (state.application)
  {
    (void)PrepareCard(true);
    state.application->Suspend();
    (void)FlushSettings();
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
  (void)FlushSettings();
}

void WriteBehindSettings() {
  auto& state = State();
  const std::uint32_t now = Now();
  (void)PrepareCard();
  const std::uint32_t revision = os::storage::SettingsRevision();
  if (revision != state.settingsRevision)
  {
    state.settingsRevision = revision;
    state.settingsChangedAt = now;
  }
  if (os::storage::AnySettingsDirty())
  {
    if (now - state.settingsChangedAt < kSettingsWriteDelayMs) return;
    if (state.settingsWriteFailed && now - state.settingsTriedAt < kCardRetryMs) return;
    (void)FlushSettings();
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

  // Only the current SD layout supplies settings. No legacy import or internal fallback.
  auto& settings = os::storage::SystemSettings();
  (void)settings.SetInt(kProtocolKey, os::storage::kProtocolVersion);
  state.cardReady = os::storage::EnsureSystemLayout();
  if (state.cardReady) (void)os::storage::RefreshAllSettingsFromCard();
  state.cardTriedAt = Now();
  os::storage::ApplyDeviceSettings();

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
  (void)FlushSettings();
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
  (void)PrepareCard(true);
  if (state.application)
  {
    state.application->Suspend();
    (void)FlushSettings();
    state.application.reset();
    os::storage::ReleaseAppSettings(state.activeId);
    state.activeId.clear();
  }
  (void)FlushSettings();
}

void UseClock(std::uint32_t (*clock)()) {
  State().clock = clock;
}

}  // namespace os::system
