#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Library/Synth/Core/Engines/SoundFont/SoundFontSynthEngine.h"

namespace synth {

// The SoundFont files on the card, for SoundFont tracks: <name>.sf2 in
// synth::storage::SoundFontDirectory() (CardenzaSynth/SoundFonts). A project names its tracks' files
// by file name.
inline constexpr const char* kSoundFontExtension = ".sf2";
// A file name a project may hold: up to 64 printable characters ending in .sf2, no path.
bool IsSoundFontFileName(const std::string& fileName);
// File names, sorted. Empty without a card.
std::vector<std::string> ListSoundFonts();
// Opens one and reads its list of patches (no samples: a patch is read when the engine asks for it,
// through the file again). Null with what to tell the user in `message` when it is not there, is no
// SoundFont this engine plays or does not fit in memory; a read that failed is reported to the card
// (os::storage::ReportCardIoError).
std::shared_ptr<soundfont::Font> OpenSoundFont(const std::string& fileName, std::string& message);
// The name to show for a file: without the extension, in capitals.
std::string SoundFontLabel(const std::string& fileName);

// Tests: patches the engines ask to read in the background (soundfont::Font::LoadLater) wait for
// RunDeferredSoundFontLoads instead of being read in the call, as on the simulator.
void DeferSoundFontLoadsForTests(bool defer);
void RunDeferredSoundFontLoads();

}  // namespace synth
