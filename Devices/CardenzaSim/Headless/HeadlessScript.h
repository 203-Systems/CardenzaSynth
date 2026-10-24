#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace CardenzaSim::Headless {

// One cell of the 4x14 key matrix, or the separate HOME button.
struct KeyTarget {
  bool home = false;
  std::size_t row = 0;
  std::size_t column = 0;
};

enum class CommandType {
  Wait,
  Tap,
  Down,
  Up,
  Screenshot,
  Quit,
};

struct ScriptCommand {
  CommandType type = CommandType::Wait;
  int lineNumber = 0;
  int frames = 0;    // wait
  KeyTarget key{};   // tap, down, up
  std::string name;  // screenshot
};

// Accepts "row,col" (0-based), "home", or a physical legend label of the Cardputer-style
// matrix (case-insensitive), plus the spellings the Win32 desktop host shows ("esc", "Aa", "ok")
// and the shifted legends ("!", "{", "?", ...).
std::optional<KeyTarget> ResolveKeyTarget(std::string_view token);

// Parses the .script/.simdebug format shared with CardenzaSimDesktop. Key targets are resolved
// up front, so an unknown target fails before the system boots. Errors read "<path>:<line>: ...".
bool LoadScript(const std::filesystem::path& path, std::vector<ScriptCommand>& commands, std::string& errorMessage);

}  // namespace CardenzaSim::Headless
