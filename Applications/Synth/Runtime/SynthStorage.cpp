#include "Applications/Synth/Runtime/SynthStorage.h"

#include <string>
#include <vector>

namespace synth::storage {

void NotifyWriteFailure()
{
  os::storage::NotifyWriteFailure();
}

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
