#include "Devices/CardenzaSim/Headless/HeadlessScript.h"

#include <array>
#include <cctype>
#include <charconv>
#include <fstream>
#include <system_error>

#include "Devices/Device.h"

namespace CardenzaSim::Headless {
namespace {

constexpr std::size_t kRows = static_cast<std::size_t>(Device::Keyboard::kRows);
constexpr std::size_t kColumns = static_cast<std::size_t>(Device::Keyboard::kColumns);

// Physical legends, lower case, indexed [row][column].
constexpr std::array<std::array<std::string_view, 14>, 4> kLegends = {{
    {"`", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", "del"},
    {"tab", "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "[", "]", "\\"},
    {"fn", "shift", "a", "s", "d", "f", "g", "h", "j", "k", "l", ";", "'", "enter"},
    {"ctrl", "opt", "alt", "z", "x", "c", "v", "b", "n", "m", ",", ".", "/", "space"},
}};
static_assert(kLegends.size() == kRows && kLegends[0].size() == kColumns, "legend table must match the key matrix");

struct KeyAlias {
  std::string_view label;
  std::size_t row;
  std::size_t column;
};

// Desktop-host spellings and the shifted legends printed on the same keys.
constexpr std::array<KeyAlias, 22> kAliases = {{
    {"esc", 0, 0}, {"!", 0, 1}, {"@", 0, 2}, {"#", 0, 3}, {"$", 0, 4}, {"%", 0, 5},
    {"^", 0, 6},   {"&", 0, 7}, {"*", 0, 8}, {"(", 0, 9}, {")", 0, 10}, {"_", 0, 11},
    {"+", 0, 12},  {"{", 1, 11}, {"}", 1, 12}, {"|", 1, 13},
    {"aa", 2, 1},  {":", 2, 11}, {"ok", 2, 13},
    {"<", 3, 10},  {">", 3, 11}, {"?", 3, 12},
}};

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

std::string Lowercase(std::string_view text)
{
  std::string value;
  value.reserve(text.size());
  for (char ch : text)
  {
    value.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
  }
  return value;
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
  const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
  return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

// Whitespace-separated tokens; double quotes group a token that contains spaces.
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
    errorMessage = "unterminated quote";
    return false;
  }
  if (!current.empty())
  {
    tokens.push_back(current);
  }
  return true;
}

std::string LineError(const std::filesystem::path& path, int lineNumber, const std::string& message)
{
  return path.string() + ":" + std::to_string(lineNumber) + ": " + message;
}

}  // namespace

std::optional<KeyTarget> ResolveKeyTarget(std::string_view token)
{
  const std::string trimmed = Trim(token);
  if (trimmed.empty())
  {
    return std::nullopt;
  }

  // "row,col". A lone "," (or anything that is not two integers) falls through to the labels.
  const auto comma = trimmed.find(',');
  if (comma != std::string::npos && comma > 0 && comma + 1 < trimmed.size())
  {
    int row = 0;
    int column = 0;
    const std::string_view view(trimmed);
    if (ParseNonNegativeInt(view.substr(0, comma), row) && ParseNonNegativeInt(view.substr(comma + 1), column))
    {
      if (static_cast<std::size_t>(row) >= kRows || static_cast<std::size_t>(column) >= kColumns)
      {
        return std::nullopt;
      }
      return KeyTarget{false, static_cast<std::size_t>(row), static_cast<std::size_t>(column)};
    }
  }

  const std::string label = Lowercase(trimmed);
  if (label == "home")
  {
    return KeyTarget{true, 0, 0};
  }
  for (std::size_t row = 0; row < kRows; ++row)
  {
    for (std::size_t column = 0; column < kColumns; ++column)
    {
      if (kLegends[row][column] == label)
      {
        return KeyTarget{false, row, column};
      }
    }
  }
  for (const KeyAlias& alias : kAliases)
  {
    if (alias.label == label)
    {
      return KeyTarget{false, alias.row, alias.column};
    }
  }
  return std::nullopt;
}

bool LoadScript(const std::filesystem::path& path, std::vector<ScriptCommand>& commands, std::string& errorMessage)
{
  commands.clear();
  std::ifstream script(path);
  if (!script.is_open())
  {
    errorMessage = "failed to open script: " + path.string();
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
      errorMessage = LineError(path, lineNumber, tokenizeError);
      return false;
    }
    if (tokens.empty())
    {
      continue;
    }

    ScriptCommand command;
    command.lineNumber = lineNumber;
    const std::string name = Lowercase(tokens[0]);
    std::size_t arity = 1;
    if (name == "wait")
    {
      command.type = CommandType::Wait;
    }
    else if (name == "tap")
    {
      command.type = CommandType::Tap;
    }
    else if (name == "down")
    {
      command.type = CommandType::Down;
    }
    else if (name == "up")
    {
      command.type = CommandType::Up;
    }
    else if (name == "screenshot")
    {
      command.type = CommandType::Screenshot;
    }
    else if (name == "app")
    {
      command.type = CommandType::App;
    }
    else if (name == "quit")
    {
      command.type = CommandType::Quit;
      arity = 0;
    }
    else
    {
      errorMessage = LineError(path, lineNumber, "unknown command '" + tokens[0] + "'");
      return false;
    }

    // A '#' token after the command's own arguments starts a trailing comment.
    for (std::size_t index = arity + 1; index < tokens.size(); ++index)
    {
      if (tokens[index][0] == '#')
      {
        tokens.resize(index);
        break;
      }
    }
    if (tokens.size() != arity + 1)
    {
      errorMessage = LineError(path, lineNumber,
                               arity == 0 ? tokens[0] + " does not accept arguments"
                                          : tokens[0] + " expects exactly one argument");
      return false;
    }

    switch (command.type)
    {
      case CommandType::Wait:
        if (!ParseNonNegativeInt(tokens[1], command.frames))
        {
          errorMessage = LineError(path, lineNumber, "wait expects a non-negative frame count");
          return false;
        }
        break;

      case CommandType::Tap:
      case CommandType::Down:
      case CommandType::Up: {
        const auto key = ResolveKeyTarget(tokens[1]);
        if (!key.has_value())
        {
          errorMessage = LineError(path, lineNumber, "unknown key target '" + tokens[1] + "'");
          return false;
        }
        command.key = *key;
        break;
      }

      case CommandType::Screenshot:
      case CommandType::App:
        command.name = tokens[1];
        break;

      case CommandType::Quit:
        break;
    }

    commands.push_back(std::move(command));
  }

  return true;
}

}  // namespace CardenzaSim::Headless
