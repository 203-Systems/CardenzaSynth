#include "Applications/Synth/Runtime/SessionStore.h"

#include <cstdlib>
#include <string>

#include "Applications/Synth/Runtime/SynthStorage.h"
#include "OS/Storage/Storage.h"

namespace synth {

namespace {

// At this many starts begun with the autosave and not finished, the next start skips it once.
constexpr int kMaxRestoreTries = 3;
constexpr std::size_t kMaxInfoBytes = 512;

std::string ProjectPath() { return os::storage::JoinPath(storage::SessionDirectory(), "autosave.ccproj"); }
std::string InfoPath() { return os::storage::JoinPath(storage::SessionDirectory(), "autosave.txt"); }
std::string TriesPath() { return os::storage::JoinPath(storage::SessionDirectory(), "restoring.txt"); }

// `key=value` lines; what does not look like one is skipped.
template <typename Visit>
void ForEachLine(const std::string& text, Visit visit)
{
  std::size_t start = 0;
  while (start < text.size())
  {
    std::size_t end = text.find('\n', start);
    if (end == std::string::npos) end = text.size();
    std::string line = text.substr(start, end - start);
    start = end + 1;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const std::size_t equals = line.find('=');
    if (equals != std::string::npos) visit(line.substr(0, equals), line.substr(equals + 1));
  }
}

}  // namespace

bool SessionStore::Save(const SessionInfo& info, const ProjectData& project)
{
  // The project first, then the text that names it.
  if (SaveProjectFileAt(ProjectPath(), project) != ProjectResult::Ok) return false;
  const std::string text = "generation=" + std::to_string(info.generation) + "\nname=" + info.name + "\ndirty=" + (info.dirty ? "1" : "0") + "\n";
  return os::storage::WriteFileAtomic(InfoPath(), text);
}

bool SessionStore::ReadInfo(SessionInfo& info)
{
  std::string text;
  if (!os::storage::ReadFileWithBackup(InfoPath(), text, kMaxInfoBytes)) return false;
  SessionInfo read;
  ForEachLine(text, [&](const std::string& key, const std::string& value) {
    if (key == "generation") read.generation = static_cast<std::uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
    else if (key == "name") read.name = value;
    else if (key == "dirty") read.dirty = value == "1";
  });
  if (read.generation == 0) return false;
  info = read;
  return true;
}

bool SessionStore::Load(SessionInfo& info, ProjectData& project)
{
  return ReadInfo(info) && LoadProjectFileAt(ProjectPath(), project) == ProjectResult::Ok;
}

bool SessionStore::BeginRestore()
{
  std::string text;
  int tries = 0;
  if (os::storage::ReadFileWithBackup(TriesPath(), text, kMaxInfoBytes))
    ForEachLine(text, [&](const std::string& key, const std::string& value) {
      if (key == "starts") tries = std::atoi(value.c_str());
    });
  if (tries >= kMaxRestoreTries)
  {
    FinishRestore();
    return false;
  }
  // Not written, it cannot count: the start goes on as if it could.
  (void)os::storage::WriteFileAtomic(TriesPath(), "starts=" + std::to_string(tries + 1) + "\n");
  return true;
}

void SessionStore::FinishRestore()
{
  (void)os::storage::RemoveFile(TriesPath());
  (void)os::storage::RemoveFile(TriesPath() + ".bak");
}

}  // namespace synth
