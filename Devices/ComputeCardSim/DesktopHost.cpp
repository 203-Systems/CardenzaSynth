#define NOMINMAX
#include <windows.h>

#include <cctype>
#include <string>
#include <vector>

#include "Devices/ComputeCardSim/DesktopHost/DesktopHostLayout.h"
#include "Devices/ComputeCardSim/DesktopHost/DesktopSimulator.h"

namespace {

std::vector<std::string> TokenizeArguments(const std::string& text)
{
  std::vector<std::string> tokens;
  std::string current;
  bool inQuotes = false;

  for (char ch : text)
  {
    if (ch == '"')
    {
      inQuotes = !inQuotes;
      continue;
    }

    if (std::isspace(static_cast<unsigned char>(ch)) && !inQuotes)
    {
      if (!current.empty())
      {
        tokens.push_back(current);
        current.clear();
      }
      continue;
    }

    current.push_back(ch);
  }

  if (!current.empty())
  {
    tokens.push_back(current);
  }

  return tokens;
}

int ParseInitialScale(const std::vector<std::string>& arguments)
{
  for (std::size_t index = 0; index < arguments.size(); ++index)
  {
    if (arguments[index].rfind("--scale=", 0) == 0)
    {
      const std::string value = arguments[index].substr(8);
      if (value == "1" || value == "2" || value == "4")
      {
        return std::stoi(value);
      }
    }
    else if (arguments[index] == "--scale" && index + 1 < arguments.size())
    {
      const std::string& value = arguments[index + 1];
      if (value == "1" || value == "2" || value == "4")
      {
        return std::stoi(value);
      }
    }
  }

  return ComputeCardSim::DesktopHost::kDefaultScale;
}

ComputeCardSim::DesktopHost::DebugScriptOptions ParseDebugOptions(const std::vector<std::string>& arguments)
{
  ComputeCardSim::DesktopHost::DebugScriptOptions options;
  for (std::size_t index = 0; index < arguments.size(); ++index)
  {
    const std::string& argument = arguments[index];
    if (argument.rfind("--debug-script=", 0) == 0)
    {
      options.scriptPath = argument.substr(15);
    }
    else if (argument == "--debug-script" && index + 1 < arguments.size())
    {
      options.scriptPath = arguments[++index];
    }
    else if (argument.rfind("--debug-output=", 0) == 0)
    {
      options.outputDirectory = argument.substr(15);
    }
    else if (argument == "--debug-output" && index + 1 < arguments.size())
    {
      options.outputDirectory = arguments[++index];
    }
    else if (argument == "--debug-exit-on-complete")
    {
      options.exitOnComplete = true;
    }
    else if (argument == "--debug-hide-window")
    {
      options.hideWindow = true;
    }
  }

  return options;
}

}  // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR commandLine, int)
{
  const std::vector<std::string> arguments = TokenizeArguments(commandLine ? std::string(commandLine) : std::string{});
  ComputeCardSim::DesktopHost::DesktopSimulator simulator(ParseInitialScale(arguments), ParseDebugOptions(arguments));
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
