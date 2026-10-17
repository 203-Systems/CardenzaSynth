#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <vector>

#include "DesktopSimulator.h"

namespace CardenzaSim::DesktopHost {

class DesktopDebugSession {
public:
  explicit DesktopDebugSession(DebugScriptOptions options);

  bool Load(std::string& errorMessage);
  bool Step(DesktopSimulator& simulator, std::string& errorMessage);
  bool IsComplete() const;
  bool ExitOnComplete() const;

private:
  enum class CommandType {
    Wait,
    Tap,
    Down,
    Up,
    Screenshot,
    Quit,
  };

  struct Command {
    CommandType type = CommandType::Wait;
    std::string argument;
    int integerValue = 0;
    int lineNumber = 0;
  };

  void MarkComplete(DesktopSimulator& simulator);
  void ReleaseHeldKeys(DesktopSimulator& simulator);
  std::filesystem::path ResolveScreenshotPath(const DesktopSimulator& simulator, std::string_view path) const;
  static std::size_t HeldKeyIndex(MatrixPosition position);

  DebugScriptOptions options_{};
  std::filesystem::path scriptPath_{};
  std::vector<Command> commands_{};
  std::array<bool, kKeyboardRows * kKeyboardColumns> heldKeys_{};
  std::size_t nextCommandIndex_ = 0;
  int waitFramesRemaining_ = 0;
  bool complete_ = false;
};

}  // namespace CardenzaSim::DesktopHost