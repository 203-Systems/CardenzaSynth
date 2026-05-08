#define NOMINMAX
#include <windows.h>

#include <string>

#include "Devices/ComputeCardSim/DesktopHost/DesktopHostLayout.h"
#include "Devices/ComputeCardSim/DesktopHost/DesktopSimulator.h"

namespace {

int ParseInitialScale(const std::string& commandLine)
{
  const std::string token = "--scale=";
  const auto position = commandLine.find(token);
  if (position == std::string::npos)
  {
    return ComputeCardSim::DesktopHost::kDefaultScale;
  }

  const auto start = position + token.size();
  const auto end = commandLine.find(' ', start);
  const std::string value = commandLine.substr(start, end == std::string::npos ? std::string::npos : end - start);
  if (value == "1" || value == "2" || value == "4")
  {
    return std::stoi(value);
  }
  return ComputeCardSim::DesktopHost::kDefaultScale;
}

}  // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR commandLine, int)
{
  ComputeCardSim::DesktopHost::DesktopSimulator simulator(ParseInitialScale(commandLine ? std::string(commandLine) : std::string{}));
  if (!simulator.Initialize())
  {
    return 1;
  }

  if (!simulator.Create(instance))
  {
    MessageBoxA(nullptr, "Failed to create desktop simulator window.", "ComputeCardSimDesktop", MB_ICONERROR | MB_OK);
    return 1;
  }

  return simulator.Run();
}
