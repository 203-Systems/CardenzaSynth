#include "OS/Storage/Migration.h"

#include <array>
#include <cstdio>
#include <cstring>

#include "Devices/Device.h"
#include "OS/Storage/Settings.h"

namespace os::storage {

namespace {

constexpr std::string_view kLegacyFolder = "CardenzaSynth";

std::size_t CountFiles(const std::string& directory)
{
  std::size_t count = 0;
  for (const auto& entry : ListDirectory(directory))
    count += entry.directory ? CountFiles(JoinPath(directory, entry.name)) : 1;
  return count;
}

bool SameContents(const std::string& left, const std::string& right)
{
  const long long size = FileSize(left);
  if (size < 0 || size != FileSize(right)) return false;
  std::FILE* a = std::fopen(left.c_str(), "rb");
  std::FILE* b = a != nullptr ? std::fopen(right.c_str(), "rb") : nullptr;
  bool same = a != nullptr && b != nullptr;
  std::array<char, 256> bufferA{};
  std::array<char, 256> bufferB{};
  while (same)
  {
    const std::size_t readA = std::fread(bufferA.data(), 1, bufferA.size(), a);
    const std::size_t readB = std::fread(bufferB.data(), 1, bufferB.size(), b);
    if (readA != readB || std::memcmp(bufferA.data(), bufferB.data(), readA) != 0) same = false;
    if (readA < bufferA.size()) break;
  }
  if (a != nullptr) std::fclose(a);
  if (b != nullptr) std::fclose(b);
  return same;
}

bool CopyFile(const std::string& source, const std::string& destination)
{
  std::FILE* input = std::fopen(source.c_str(), "rb");
  if (input == nullptr) return false;
  std::FILE* output = std::fopen(destination.c_str(), "wb");
  if (output == nullptr)
  {
    std::fclose(input);
    return false;
  }
  std::array<char, 512> buffer{};
  bool ok = true;
  for (;;)
  {
    const std::size_t read = std::fread(buffer.data(), 1, buffer.size(), input);
    if (read != 0 && std::fwrite(buffer.data(), 1, read, output) != read) ok = false;
    if (!ok || read < buffer.size()) break;
  }
  ok = std::ferror(input) == 0 && ok;
  std::fclose(input);
  ok = std::fclose(output) == 0 && ok;
  return ok;
}

void MoveFile(const std::string& source, const std::string& destination, MigrationResult& result)
{
  if (Exists(destination))
  {
    // Already moved by an interrupted run (copy done, source not yet removed), or a real conflict.
    if (IsFile(destination) && SameContents(source, destination) && std::remove(source.c_str()) == 0) ++result.duplicates;
    else ++result.conflicts;
    return;
  }
  if (std::rename(source.c_str(), destination.c_str()) == 0)
  {
    ++result.moved;
    return;
  }
  // Rename can fail across volumes; copy, verify and only then remove the original.
  if (CopyFile(source, destination) && SameContents(source, destination))
  {
    if (std::remove(source.c_str()) == 0) ++result.moved;
    else ++result.duplicates;  // Both copies are identical; the old one stays.
    return;
  }
  if (IsFile(destination) && !SameContents(source, destination)) std::remove(destination.c_str());
  ++result.failed;
}

void MoveTree(const std::string& source, const std::string& destination, MigrationResult& result)
{
  // An empty target (created by an earlier start) does not block the one-step folder rename.
  if (IsDirectory(destination) && ListDirectory(destination).empty()) (void)RemoveEmptyDirectory(destination);
  if (!Exists(destination))
  {
    const std::size_t files = CountFiles(source);
    if (std::rename(source.c_str(), destination.c_str()) == 0)
    {
      result.moved += files;
      return;
    }
    if (!CreateDirectories(destination))
    {
      result.failed += files;
      return;
    }
  }
  if (!IsDirectory(destination))
  {
    result.conflicts += CountFiles(source);
    return;
  }
  for (const auto& entry : ListDirectory(source))
  {
    const std::string from = JoinPath(source, entry.name);
    const std::string to = JoinPath(destination, entry.name);
    if (entry.directory) MoveTree(from, to, result);
    else MoveFile(from, to, result);
  }
  (void)RemoveEmptyDirectory(source);
}

}  // namespace

std::string LegacyRoot()
{
  return JoinPath(Device::Storage::RootPath(), kLegacyFolder);
}

MigrationResult MigrateLegacyLayout(const AppFolder& app)
{
  MigrationResult result;
  const std::string legacy = LegacyRoot();
  if (!app.Valid() || !CardPresent() || !IsDirectory(legacy)) return result;
  result.found = true;
  // Parents only: the leaf folders are renamed into place when they do not exist yet.
  if (!CreateDirectories(Root()) || !CreateDirectories(SharedDirectory()) || !CreateDirectories(app.Root())) return result;

  const std::array<std::pair<std::string, std::string>, 3> moves{{
      {JoinPath(legacy, "Project"), app.Projects()},
      {JoinPath(legacy, "Preset"), app.Presets()},
      {JoinPath(legacy, "Sample"), SharedSamplesDirectory()},
  }};
  for (const auto& move : moves)
    if (IsDirectory(move.first)) MoveTree(move.first, move.second, result);
  // Only an empty folder goes; anything else the user kept there stays.
  (void)RemoveEmptyDirectory(legacy);

  Settings& system = SystemSettings();
  std::size_t left = 0;
  for (const auto& move : moves) left += IsDirectory(move.first) ? CountFiles(move.first) : 0;
  system.SetString("migrate.legacy", left == 0 ? "done" : "partial");
  system.SetInt("migrate.left", static_cast<int>(left));
  (void)system.Flush();
  return result;
}

}  // namespace os::storage
