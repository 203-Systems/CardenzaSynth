#pragma once

#include <string>
#include <vector>

#include "Library/Synth/Core/Engines/Vocal/VocalPack.h"

namespace synth {

// The voice packs on the card, for vocal tracks: <name>.vocal in synth::storage::VoiceDirectory()
// (CardenzaSynth/Voices). A project names its tracks' packs by file name; the built-in voice has no
// file and its name is empty.
inline constexpr const char* kVoicePackExtension = ".vocal";
// A file name a project may hold: up to 64 printable characters ending in .vocal, no path.
bool IsVoicePackFileName(const std::string& fileName);
// File names, sorted. Empty without a card.
std::vector<std::string> ListVoicePacks();
// Reads and checks one. Null when it is not there, is no valid pack or does not fit in memory; a read
// that failed is reported to the card (os::storage::ReportCardIoError).
vocal::PackRef LoadVoicePack(const std::string& fileName);
// The name to show for a file: without the extension, in capitals.
std::string VoicePackLabel(const std::string& fileName);

}  // namespace synth
