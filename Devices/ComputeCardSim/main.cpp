#include <iostream>

#include "Applications/BuiltInApplications.h"
#include "Devices/Device.h"
#include "OS/System/System.h"

int main() {
  if (!os::system::Begin())
  {
    std::cerr << "Failed to boot ComputeCard runtime\n";
    return 1;
  }

  auto* application = os::system::ActiveApplication();
  if (!application)
  {
    std::cerr << "Failed to create default application\n";
    return 1;
  }

  os::system::RunFrame();

  std::cout << Device::Name() << " [" << Device::BackendName() << "] "
            << Device::Display::kWidth << "x" << Device::Display::kHeight << "\n";
  return 0;
}
