#include "DesktopSimulatorDebug.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "Devices/Device.h"

namespace ComputeCardSim::DesktopHost {
namespace {

std::string Trim(std::string_view text)
{
  std::size_t start = 0;
  while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])))
  {
    ++start;
  }

  std::size_t end = text.size();
  while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])))
  {
    --end;
  }

  return std::string(text.substr(start, end - start));
}

std::string Uppercase(std::string_view text)
{
  std::string value;
  value.reserve(text.size());
  for (char ch : text)
  {
    value.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
  }
  return value;
}

bool Tokenize(std::string_view text, std::vector<std::string>& tokens, std::string& errorMessage)
{
  tokens.clear();
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

  if (inQuotes)
  {
    errorMessage = "unterminated quote in debug script";
    return false;
  }

  if (!current.empty())
  {
    tokens.push_back(current);
  }

  return true;
}

bool ParseNonNegativeInt(std::string_view text, int& value)
{
  if (text.empty())
  {
    return false;
  }

  for (char ch : text)
  {
    if (!std::isdigit(static_cast<unsigned char>(ch)))
    {
      return false;
    }
  }

  value = std::stoi(std::string(text));
  return true;
}

void WriteLe16(std::ofstream& stream, std::uint16_t value)
{
  const std::array<char, 2> bytes = {
      static_cast<char>(value & 0xFF),
      static_cast<char>((value >> 8) & 0xFF),
  };
  stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

void WriteLe32(std::ofstream& stream, std::uint32_t value)
{
  const std::array<char, 4> bytes = {
      static_cast<char>(value & 0xFF),
      static_cast<char>((value >> 8) & 0xFF),
      static_cast<char>((value >> 16) & 0xFF),
      static_cast<char>((value >> 24) & 0xFF),
  };
  stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

std::string LineError(const std::filesystem::path& path, int lineNumber, const std::string& message)
{
  return path.string() + ":" + std::to_string(lineNumber) + ": " + message;
}

}  // namespace

DesktopDebugSession::DesktopDebugSession(DebugScriptOptions options)
    : options_(std::move(options)) {}

bool DesktopDebugSession::Load(std::string& errorMessage)
{
  commands_.clear();
  nextCommandIndex_ = 0;
  waitFramesRemaining_ = 0;
  complete_ = false;
  heldKeys_.fill(false);

  scriptPath_ = std::filesystem::path(options_.scriptPath);
  std::ifstream script(scriptPath_);
  if (!script.is_open())
  {
    errorMessage = "Failed to open debug script: " + scriptPath_.string();
    return false;
  }

  std::string line;
  int lineNumber = 0;
  std::vector<std::string> tokens;
  while (std::getline(script, line))
  {
    ++lineNumber;
    const std::string trimmed = Trim(line);
    if (trimmed.empty() || trimmed[0] == '#')
    {
      continue;
    }

    std::string tokenizeError;
    if (!Tokenize(trimmed, tokens, tokenizeError))
    {
      errorMessage = LineError(scriptPath_, lineNumber, tokenizeError);
      return false;
    }

    if (tokens.empty())
    {
      continue;
    }

    Command command;
    command.lineNumber = lineNumber;
    const std::string commandName = Uppercase(tokens[0]);
    if (commandName == "WAIT")
    {
      if (tokens.size() != 2 || !ParseNonNegativeInt(tokens[1], command.integerValue))
      {
        errorMessage = LineError(scriptPath_, lineNumber, "wait expects a non-negative frame count");
        return false;
      }
      command.type = CommandType::Wait;
    }
    else if (commandName == "TAP" || commandName == "DOWN" || commandName == "UP")
    {
      if (tokens.size() != 2)
      {
        errorMessage = LineError(scriptPath_, lineNumber, tokens[0] + " expects one key target");
        return false;
      }

      command.type = commandName == "TAP" ? CommandType::Tap : (commandName == "DOWN" ? CommandType::Down : CommandType::Up);
      command.argument = tokens[1];
    }
    else if (commandName == "SCREENSHOT")
    {
      if (tokens.size() != 2)
      {
        errorMessage = LineError(scriptPath_, lineNumber, "screenshot expects one output path");
        return false;
      }

      command.type = CommandType::Screenshot;
      command.argument = tokens[1];
    }
    else if (commandName == "QUIT")
    {
      if (tokens.size() != 1)
      {
        errorMessage = LineError(scriptPath_, lineNumber, "quit does not accept arguments");
        return false;
      }

      command.type = CommandType::Quit;
    }
    else
    {
      errorMessage = LineError(scriptPath_, lineNumber, "unknown command '" + tokens[0] + "'");
      return false;
    }

    commands_.push_back(std::move(command));
  }

  return true;
}

bool DesktopDebugSession::Step(DesktopSimulator& simulator, std::string& errorMessage)
{
  if (complete_)
  {
    return true;
  }

  if (waitFramesRemaining_ > 0)
  {
    --waitFramesRemaining_;
    return true;
  }

  if (nextCommandIndex_ >= commands_.size())
  {
    MarkComplete(simulator);
    return true;
  }

  const Command& command = commands_[nextCommandIndex_++];
  switch (command.type)
  {
    case CommandType::Wait:
      waitFramesRemaining_ = command.integerValue;
      break;

    case CommandType::Tap: {
      const auto position = simulator.ResolveDebugKey(command.argument);
      if (!position.has_value())
      {
        errorMessage = LineError(scriptPath_, command.lineNumber, "unknown key target '" + command.argument + "'");
        return false;
      }

      simulator.TapVirtualKey(*position);
      break;
    }

    case CommandType::Down: {
      const auto position = simulator.ResolveDebugKey(command.argument);
      if (!position.has_value())
      {
        errorMessage = LineError(scriptPath_, command.lineNumber, "unknown key target '" + command.argument + "'");
        return false;
      }

      simulator.SetVirtualKey(*position, true);
      heldKeys_[HeldKeyIndex(*position)] = true;
      break;
    }

    case CommandType::Up: {
      const auto position = simulator.ResolveDebugKey(command.argument);
      if (!position.has_value())
      {
        errorMessage = LineError(scriptPath_, command.lineNumber, "unknown key target '" + command.argument + "'");
        return false;
      }

      simulator.SetVirtualKey(*position, false);
      heldKeys_[HeldKeyIndex(*position)] = false;
      break;
    }

    case CommandType::Screenshot: {
      const std::filesystem::path outputPath = ResolveScreenshotPath(simulator, command.argument);
      if (!simulator.SaveFrameBufferBmp(outputPath))
      {
        errorMessage = LineError(scriptPath_, command.lineNumber, "failed to save screenshot to '" + outputPath.string() + "'");
        return false;
      }
      break;
    }

    case CommandType::Quit:
      MarkComplete(simulator);
      break;
  }

  if (!complete_ && nextCommandIndex_ >= commands_.size())
  {
    MarkComplete(simulator);
  }

  return true;
}

bool DesktopDebugSession::IsComplete() const
{
  return complete_;
}

bool DesktopDebugSession::ExitOnComplete() const
{
  return options_.exitOnComplete;
}

void DesktopDebugSession::MarkComplete(DesktopSimulator& simulator)
{
  ReleaseHeldKeys(simulator);
  complete_ = true;
}

void DesktopDebugSession::ReleaseHeldKeys(DesktopSimulator& simulator)
{
  for (std::size_t row = 0; row < kKeyboardRows; ++row)
  {
    for (std::size_t column = 0; column < kKeyboardColumns; ++column)
    {
      const MatrixPosition position{row, column};
      const std::size_t index = HeldKeyIndex(position);
      if (!heldKeys_[index])
      {
        continue;
      }

      simulator.SetVirtualKey(position, false);
      heldKeys_[index] = false;
    }
  }
}

std::filesystem::path DesktopDebugSession::ResolveScreenshotPath(const DesktopSimulator& simulator, std::string_view path) const
{
  std::filesystem::path outputPath = simulator.ResolveDebugOutputPath(path);
  if (!outputPath.has_extension())
  {
    outputPath.replace_extension(".bmp");
  }
  return outputPath;
}

std::size_t DesktopDebugSession::HeldKeyIndex(MatrixPosition position)
{
  return position.row * kKeyboardColumns + position.column;
}

std::optional<MatrixPosition> DesktopSimulator::ResolveDebugKey(std::string_view token) const
{
  const std::string trimmed = Trim(token);
  const auto comma = trimmed.find(',');
  if (comma != std::string::npos)
  {
    int row = 0;
    int column = 0;
    if (!ParseNonNegativeInt(std::string_view(trimmed).substr(0, comma), row)
        || !ParseNonNegativeInt(std::string_view(trimmed).substr(comma + 1), column))
    {
      return std::nullopt;
    }

    if (row < 0 || row >= kKeyboardRows || column < 0 || column >= kKeyboardColumns)
    {
      return std::nullopt;
    }

    return MatrixPosition{static_cast<std::size_t>(row), static_cast<std::size_t>(column)};
  }

  const std::string upperToken = Uppercase(trimmed);
  for (const KeyVisual& key : keyboard_)
  {
    if (Uppercase(key.primaryLabel) == upperToken || Uppercase(key.secondaryLabel) == upperToken || Uppercase(key.note) == upperToken)
    {
      return MatrixPosition{static_cast<std::size_t>(key.row), static_cast<std::size_t>(key.column)};
    }
  }

  return std::nullopt;
}

std::filesystem::path DesktopSimulator::ResolveDebugOutputPath(std::string_view path) const
{
  std::filesystem::path outputPath(path);
  if (outputPath.is_absolute())
  {
    return outputPath;
  }

  if (!debugOptions_.outputDirectory.empty())
  {
    return std::filesystem::path(debugOptions_.outputDirectory) / outputPath;
  }

  if (!debugOptions_.scriptPath.empty())
  {
    return std::filesystem::path(debugOptions_.scriptPath).parent_path() / outputPath;
  }

  return std::filesystem::current_path() / outputPath;
}

bool DesktopSimulator::SaveFrameBufferBmp(const std::filesystem::path& outputPath) const
{
  const auto frameBuffer = Device::Display::CurrentFrameBuffer();
  if (frameBuffer.pixels == nullptr || frameBuffer.width <= 0 || frameBuffer.height <= 0)
  {
    return false;
  }

  std::error_code errorCode;
  if (!outputPath.parent_path().empty())
  {
    std::filesystem::create_directories(outputPath.parent_path(), errorCode);
    if (errorCode)
    {
      return false;
    }
  }

  const int rowStride = ((frameBuffer.width * 3) + 3) & ~3;
  const std::uint32_t pixelBytes = static_cast<std::uint32_t>(rowStride * frameBuffer.height);
  const std::uint32_t fileSize = 14u + 40u + pixelBytes;

  std::ofstream stream(outputPath, std::ios::binary);
  if (!stream.is_open())
  {
    return false;
  }

  stream.put('B');
  stream.put('M');
  WriteLe32(stream, fileSize);
  WriteLe16(stream, 0);
  WriteLe16(stream, 0);
  WriteLe32(stream, 54);

  WriteLe32(stream, 40);
  WriteLe32(stream, static_cast<std::uint32_t>(frameBuffer.width));
  WriteLe32(stream, static_cast<std::uint32_t>(frameBuffer.height));
  WriteLe16(stream, 1);
  WriteLe16(stream, 24);
  WriteLe32(stream, 0);
  WriteLe32(stream, pixelBytes);
  WriteLe32(stream, 0);
  WriteLe32(stream, 0);
  WriteLe32(stream, 0);
  WriteLe32(stream, 0);

  std::vector<std::uint8_t> row(static_cast<std::size_t>(rowStride), 0);
  for (int y = frameBuffer.height - 1; y >= 0; --y)
  {
    std::fill(row.begin(), row.end(), 0);
    for (int x = 0; x < frameBuffer.width; ++x)
    {
      const std::uint16_t pixel = frameBuffer.pixels[(y * frameBuffer.width) + x];
      const std::uint8_t red = static_cast<std::uint8_t>(((pixel >> 11) & 0x1F) * 255 / 31);
      const std::uint8_t green = static_cast<std::uint8_t>(((pixel >> 5) & 0x3F) * 255 / 63);
      const std::uint8_t blue = static_cast<std::uint8_t>((pixel & 0x1F) * 255 / 31);

      row[(x * 3) + 0] = blue;
      row[(x * 3) + 1] = green;
      row[(x * 3) + 2] = red;
    }

    stream.write(reinterpret_cast<const char*>(row.data()), static_cast<std::streamsize>(row.size()));
  }

  return stream.good();
}

}  // namespace ComputeCardSim::DesktopHost