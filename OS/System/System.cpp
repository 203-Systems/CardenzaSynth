#include "OS/System/System.h"

#include <memory>

#include "Applications/BuiltInApplications.h"
#include "Devices/Device.h"

namespace os::system {

namespace {

std::unique_ptr<app::Application>& ActiveApplicationStorage() {
  static std::unique_ptr<app::Application> application;
  return application;
}

Device::Keyboard::MatrixKeyState& PreviousKeyStateStorage() {
  static Device::Keyboard::MatrixKeyState state{};
  return state;
}

bool& StartedStorage() {
  static bool started = false;
  return started;
}

void DispatchMatrixKeyEvents(app::Application& application) {
  auto& previousState = PreviousKeyStateStorage();
  const Device::Keyboard::MatrixKeyState currentState = Device::Keyboard::SnapshotState();

  for (std::size_t index = 0; index < currentState.size(); ++index)
  {
    if (currentState[index] == previousState[index])
    {
      continue;
    }

    const std::size_t row = index / static_cast<std::size_t>(Device::Keyboard::kColumns);
    const std::size_t column = index % static_cast<std::size_t>(Device::Keyboard::kColumns);
    application.HandleMatrixKeyEvent(row, column, currentState[index]);
  }

  previousState = currentState;
}

}  // namespace

bool Begin(const BootOptions& options) {
  if (StartedStorage())
  {
    return ActiveApplicationStorage() != nullptr;
  }

  if (!Device::Init())
  {
    return false;
  }

  app::RegisterBuiltInApplications();
  ActiveApplicationStorage() = app::CreateDefaultApplication();
  if (!ActiveApplicationStorage())
  {
    return false;
  }

  ActiveApplicationStorage()->Setup(options.applicationArgs);
  Device::Start();
  PreviousKeyStateStorage() = Device::Keyboard::SnapshotState();
  StartedStorage() = true;

  Device::Display::Clear();
  ActiveApplicationStorage()->RenderFrame();
  Device::Display::Present();
  return true;
}

void RunFrame() {
  app::Application* application = ActiveApplication();
  if (application == nullptr)
  {
    return;
  }

  DispatchMatrixKeyEvents(*application);
  application->Tick();
  application->RenderFrame();
  Device::Display::Present();
}

app::Application* ActiveApplication() {
  return ActiveApplicationStorage().get();
}

}  // namespace os::system