#include "Applications/Synth/Runtime/SynthStorage.h"

#include <string>
#include <vector>

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

std::string PresetDirectory(std::string_view group)
{
  return AppFolder().Presets(group);
}

std::string VoiceDirectory()
{
  return AppFolder().Folder("Voices");
}

std::string SoundFontDirectory()
{
  return AppFolder().Folder("SoundFonts");
}

std::string SessionDirectory()
{
  return AppFolder().Session();
}

bool EnsureLayout()
{
  // One preset folder per engine (SynthDescriptor::displayName).
  static const std::vector<std::string> engines{"DX7", "Juno", "Sampler", "Drums", "Plaits", "Vocal", "Wave", "SoundFont"};
  // Voices/ and SoundFonts/ are where the user puts voice packs and .sf2 files: there from the start,
  // so they are found.
  return os::storage::EnsureAppLayout(AppFolder(), engines) && os::storage::CreateDirectories(VoiceDirectory()) &&
         os::storage::CreateDirectories(SoundFontDirectory());
}

}  // namespace synth::storage
