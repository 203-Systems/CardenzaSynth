#include "Applications/Synth/Runtime/SynthStorage.h"

#include <string>
#include <vector>

#include "OS/Storage/Migration.h"

namespace synth::storage {

const os::storage::AppFolder& AppFolder()
{
  static const os::storage::AppFolder folder(kAppId);
  return folder;
}

std::string ProjectDirectory()
{
  return AppFolder().Projects();
}

std::string SampleDirectory()
{
  return os::storage::SharedSamplesDirectory();
}

std::string ReleaseProjectDirectory()
{
  return os::storage::JoinPath(os::storage::LegacyRoot(), "Projects");
}

std::string ReleaseSampleDirectory()
{
  return os::storage::JoinPath(os::storage::LegacyRoot(), "Samples");
}

std::string ResolveSamplePath(const std::string& fileName)
{
  const std::string current = os::storage::JoinPath(SampleDirectory(), fileName);
  if (os::storage::Exists(current)) return current;
  const std::string release = os::storage::JoinPath(ReleaseSampleDirectory(), fileName);
  return os::storage::Exists(release) ? release : current;
}

std::string PresetDirectory(std::string_view group)
{
  return AppFolder().Presets(group);
}

std::string SessionDirectory()
{
  return AppFolder().Session();
}

bool EnsureLayout()
{
  // One preset folder per engine (SynthDescriptor::displayName).
  static const std::vector<std::string> engines{"DX7", "Juno", "Sampler", "Drums"};
  return os::storage::EnsureAppLayout(AppFolder(), engines);
}

}  // namespace synth::storage
