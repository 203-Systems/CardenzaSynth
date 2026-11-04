#pragma once

#include <string>
#include <string_view>

// Where the synth keeps its files. The application that hosts the synth defines these functions:
// Cardenza/Synth maps them onto its folders of the storage protocol
// (Applications/Synth/Runtime/SynthStorage.cpp, docs/app-storage-protocol.md). Tests and tools
// define their own.
namespace synth::storage {

std::string ProjectDirectory();
// Samples are shared between apps (CardenzaSynth/Samples); projects name them by file name.
std::string SampleDirectory();
// User preset banks, one folder per engine (`group` is the engine's display name).
std::string PresetDirectory(std::string_view group);
// The working state: the autosave, and sample recordings not saved yet.
std::string SessionDirectory();
// Creates every folder above, the preset folder of each engine included. False without a
// writable card.
bool EnsureLayout();

}  // namespace synth::storage
