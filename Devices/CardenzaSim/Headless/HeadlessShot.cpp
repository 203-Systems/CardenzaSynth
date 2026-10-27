// CardenzaSimShot: headless screenshot runner for the CardenzaSim device.
//
// Boots the production runtime exactly like the device (os::system::Begin, then
// os::system::RunFrame on a 16 ms cadence), replays a CardenzaSimDesktop-compatible key
// script through the public simulator input hooks, and writes framebuffer PNGs plus the
// keyboard LED state. It knows nothing about the application or its UI.

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>

#include "Devices/CardenzaSim/Headless/HeadlessScript.h"
#include "Devices/CardenzaSim/Headless/PngWriter.h"
#include "Devices/CardenzaSim/SimulatorInput.h"
#include "Devices/Device.h"
#include "OS/System/System.h"

namespace {

namespace fs = std::filesystem;
using namespace CardenzaSim::Headless;
using Clock = std::chrono::steady_clock;

// Same period as the device runtime task and the desktop host's frame timer.
constexpr auto kFramePeriod = std::chrono::milliseconds(16);
constexpr std::size_t kRows = static_cast<std::size_t>(Device::Keyboard::kRows);
constexpr std::size_t kColumns = static_cast<std::size_t>(Device::Keyboard::kColumns);

constexpr const char* kUsage =
    "usage: CardenzaSimShot --script <file> --out <dir> [--scale N] [--root <dir>]\n"
    "  --script <file>  key script (.script/.simdebug format: wait/tap/down/up/screenshot/quit)\n"
    "  --out <dir>      directory for <name>.png and <name>.leds.txt (created if missing)\n"
    "  --scale N        nearest-neighbour upscale factor 1..4 (default 1)\n"
    "  --root <dir>     chdir here before boot; the card is <dir>/sdcard (docs/app-storage-protocol.md)\n"
    "                   (default: the caller's working directory)\n";

struct Options {
  fs::path scriptPath;
  fs::path outputDirectory;
  fs::path rootDirectory;
  int scale = 1;
};

bool ParseArguments(int argc, char** argv, Options& options, std::string& errorMessage)
{
  for (int index = 1; index < argc; ++index)
  {
    std::string argument = argv[index];
    std::string value;
    bool hasValue = false;
    if (const auto equals = argument.find('='); argument.rfind("--", 0) == 0 && equals != std::string::npos)
    {
      value = argument.substr(equals + 1);
      argument.resize(equals);
      hasValue = true;
    }

    if (argument == "-h" || argument == "--help")
    {
      errorMessage.clear();
      return false;
    }
    if (argument != "--script" && argument != "--out" && argument != "--scale" && argument != "--root")
    {
      errorMessage = "unknown argument '" + argument + "'";
      return false;
    }
    if (!hasValue)
    {
      if (index + 1 >= argc)
      {
        errorMessage = argument + " expects a value";
        return false;
      }
      value = argv[++index];
    }

    if (argument == "--script")
    {
      options.scriptPath = value;
    }
    else if (argument == "--out")
    {
      options.outputDirectory = value;
    }
    else if (argument == "--root")
    {
      options.rootDirectory = value;
    }
    else
    {
      if (value.size() != 1 || value[0] < '1' || value[0] > '4')
      {
        errorMessage = "--scale expects 1, 2, 3 or 4";
        return false;
      }
      options.scale = value[0] - '0';
    }
  }

  if (options.scriptPath.empty() || options.outputDirectory.empty())
  {
    errorMessage = "--script and --out are required";
    return false;
  }
  return true;
}

std::string Lowercase(std::string text)
{
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return text;
}

// "name" -> <out>/name.png; "name.bmp" (desktop scripts) -> <out>/name.png; absolute paths kept.
fs::path ScreenshotPath(const fs::path& outputDirectory, const std::string& name)
{
  fs::path path(name);
  if (path.is_relative())
  {
    path = outputDirectory / path;
  }
  const std::string extension = Lowercase(path.extension().string());
  if (extension == ".bmp")
  {
    path.replace_extension(".png");
  }
  else if (extension != ".png")
  {
    path += ".png";
  }
  return path;
}

// RGB565 -> RGB888 with the same integer conversion as the desktop host's BMP writer.
std::vector<std::uint8_t> CaptureRgb(int scale, std::uint32_t& width, std::uint32_t& height)
{
  const auto frame = Device::Display::CurrentFrameBuffer();
  width = static_cast<std::uint32_t>(frame.width * scale);
  height = static_cast<std::uint32_t>(frame.height * scale);
  std::vector<std::uint8_t> rgb(static_cast<std::size_t>(width) * height * 3u);
  const std::size_t scaledRowBytes = static_cast<std::size_t>(width) * 3u;

  for (int y = 0; y < frame.height; ++y)
  {
    std::uint8_t* firstRow = rgb.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(scale) * scaledRowBytes;
    for (int x = 0; x < frame.width; ++x)
    {
      const std::uint16_t pixel = frame.pixels[(y * frame.width) + x];
      const std::uint8_t red = static_cast<std::uint8_t>(((pixel >> 11) & 0x1F) * 255 / 31);
      const std::uint8_t green = static_cast<std::uint8_t>(((pixel >> 5) & 0x3F) * 255 / 63);
      const std::uint8_t blue = static_cast<std::uint8_t>((pixel & 0x1F) * 255 / 31);
      for (int dx = 0; dx < scale; ++dx)
      {
        std::uint8_t* out = firstRow + (static_cast<std::size_t>(x) * static_cast<std::size_t>(scale) + static_cast<std::size_t>(dx)) * 3u;
        out[0] = red;
        out[1] = green;
        out[2] = blue;
      }
    }
    for (int dy = 1; dy < scale; ++dy)
    {
      std::copy(firstRow, firstRow + scaledRowBytes, firstRow + static_cast<std::size_t>(dy) * scaledRowBytes);
    }
  }
  return rgb;
}

// Four lines of fourteen levels (0-15), then the master brightness and enabled flag.
bool WriteLedState(const fs::path& path)
{
  std::ofstream stream(path, std::ios::trunc);
  if (!stream.is_open())
  {
    return false;
  }
  for (std::size_t row = 0; row < kRows; ++row)
  {
    for (std::size_t column = 0; column < kColumns; ++column)
    {
      stream << (column ? " " : "") << static_cast<unsigned>(Device::LED::Brightness(row, column));
    }
    stream << '\n';
  }
  stream << "brightness " << static_cast<unsigned>(Device::LED::GlobalBrightness())
         << " enabled " << (Device::LED::Enabled() ? 1 : 0) << '\n';
  return stream.good();
}

class ScriptRunner {
public:
  ScriptRunner(const std::vector<ScriptCommand>& commands, const Options& options, fs::path scriptPath)
      : commands_(commands), options_(options), scriptPath_(std::move(scriptPath)) {}

  // Mirrors CardenzaSimDesktop's debug session: every 16 ms tick runs one frame and then the
  // next command. down/up/tap apply the change and run a frame immediately, so a tap is
  // down + frame + up + frame inside one tick; "wait N" idles for N further ticks.
  bool Run(std::string& errorMessage)
  {
    const auto started = Clock::now();
    nextTick_ = started + kFramePeriod;
    std::size_t nextCommand = 0;
    int waitRemaining = 0;
    bool ok = true;

    while (true)
    {
      PaceTick();
      RunFrame();

      if (waitRemaining > 0)
      {
        --waitRemaining;
        continue;
      }
      if (nextCommand >= commands_.size())
      {
        break;
      }

      const ScriptCommand& command = commands_[nextCommand++];
      if (command.type == CommandType::Quit)
      {
        break;
      }
      if (command.type == CommandType::Wait)
      {
        waitRemaining = command.frames;
        continue;
      }
      if (!Execute(command, errorMessage))
      {
        ok = false;
        break;
      }
    }

    ReleaseHeldKeys();
    const double seconds = std::chrono::duration<double>(Clock::now() - started).count();
    std::cout << "CardenzaSimShot: " << screenshots_ << " screenshot(s), " << ticks_ << " ticks, " << frames_
              << " frames in " << seconds << " s";
    if (lateTicks_ > 0)
    {
      std::cout << " (" << lateTicks_ << " tick(s) ran more than one frame late; hold timings may drift)";
    }
    std::cout << '\n';
    return ok;
  }

private:
  void PaceTick()
  {
    const auto now = Clock::now();
    if (now < nextTick_)
    {
      std::this_thread::sleep_until(nextTick_);
    }
    else if (now - nextTick_ > kFramePeriod)
    {
      // Too far behind: resume the cadence from now rather than bursting frames to catch up.
      ++lateTicks_;
      nextTick_ = now;
    }
    nextTick_ += kFramePeriod;
    ++ticks_;
  }

  void RunFrame()
  {
    os::system::RunFrame();
    ++frames_;
  }

  void SetKey(const KeyTarget& key, bool pressed)
  {
    if (key.home)
    {
      CardenzaSim::Input::SetHomeButton(pressed);
      homeHeld_ = pressed;
    }
    else
    {
      CardenzaSim::Input::SetMatrixKey(key.row, key.column, pressed);
      held_[key.row * kColumns + key.column] = pressed;
    }
    RunFrame();
  }

  bool Execute(const ScriptCommand& command, std::string& errorMessage)
  {
    switch (command.type)
    {
      case CommandType::Tap:
        SetKey(command.key, true);
        SetKey(command.key, false);
        return true;
      case CommandType::Down:
        SetKey(command.key, true);
        return true;
      case CommandType::Up:
        SetKey(command.key, false);
        return true;
      case CommandType::Screenshot:
        return Screenshot(command, errorMessage);
      case CommandType::Wait:
      case CommandType::Quit:
        return true;
    }
    return true;
  }

  bool Screenshot(const ScriptCommand& command, std::string& errorMessage)
  {
    const fs::path pngPath = ScreenshotPath(options_.outputDirectory, command.name);
    fs::path ledPath = pngPath;
    ledPath.replace_extension(".leds.txt");

    std::error_code error;
    if (pngPath.has_parent_path())
    {
      fs::create_directories(pngPath.parent_path(), error);
    }

    std::uint32_t width = 0;
    std::uint32_t height = 0;
    const std::vector<std::uint8_t> rgb = CaptureRgb(options_.scale, width, height);
    if (error || !WritePngRgb8(pngPath, rgb.data(), width, height) || !WriteLedState(ledPath))
    {
      errorMessage = scriptPath_.string() + ":" + std::to_string(command.lineNumber) +
                     ": failed to write screenshot '" + pngPath.string() + "'";
      return false;
    }
    ++screenshots_;
    std::cout << pngPath.string() << '\n';
    return true;
  }

  void ReleaseHeldKeys()
  {
    for (std::size_t index = 0; index < held_.size(); ++index)
    {
      if (held_[index])
      {
        SetKey(KeyTarget{false, index / kColumns, index % kColumns}, false);
      }
    }
    if (homeHeld_)
    {
      SetKey(KeyTarget{true, 0, 0}, false);
    }
  }

  const std::vector<ScriptCommand>& commands_;
  const Options& options_;
  fs::path scriptPath_;
  std::array<bool, kRows * kColumns> held_{};
  bool homeHeld_ = false;
  Clock::time_point nextTick_{};
  std::uint64_t ticks_ = 0;
  std::uint64_t frames_ = 0;
  std::uint64_t lateTicks_ = 0;
  int screenshots_ = 0;
};

}  // namespace

int main(int argc, char** argv)
{
  Options options;
  std::string errorMessage;
  if (!ParseArguments(argc, argv, options, errorMessage))
  {
    if (errorMessage.empty())
    {
      std::cout << kUsage;
      return 0;
    }
    std::cerr << "CardenzaSimShot: " << errorMessage << '\n' << kUsage;
    return 2;
  }

  // Resolve caller-relative paths before --root changes the working directory.
  std::error_code error;
  const fs::path scriptPath = fs::absolute(options.scriptPath, error);
  options.outputDirectory = fs::absolute(options.outputDirectory, error);

  std::vector<ScriptCommand> commands;
  if (!LoadScript(scriptPath, commands, errorMessage))
  {
    std::cerr << "CardenzaSimShot: " << errorMessage << '\n';
    return 1;
  }

  if (!options.rootDirectory.empty())
  {
    fs::create_directories(options.rootDirectory, error);
    fs::current_path(options.rootDirectory, error);
    if (error)
    {
      std::cerr << "CardenzaSimShot: cannot use --root '" << options.rootDirectory.string() << "': " << error.message() << '\n';
      return 1;
    }
  }
  fs::create_directories(options.outputDirectory, error);
  if (error)
  {
    std::cerr << "CardenzaSimShot: cannot create '" << options.outputDirectory.string() << "': " << error.message() << '\n';
    return 1;
  }

  // The device's storage root is fixed on first use, so it follows the directory chosen above.
  if (!os::system::Begin())
  {
    std::cerr << "CardenzaSimShot: failed to boot Cardenza runtime\n";
    return 1;
  }
  std::cout << "CardenzaSimShot: storage root " << Device::Storage::RootPath() << '\n';

  ScriptRunner runner(commands, options, scriptPath);
  if (!runner.Run(errorMessage))
  {
    std::cerr << "CardenzaSimShot: " << errorMessage << '\n';
    return 1;
  }
  return 0;
}
