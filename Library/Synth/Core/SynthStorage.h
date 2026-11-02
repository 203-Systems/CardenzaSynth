#pragma once

#include <string>
#include <string_view>

// Where the synth keeps its files. The application that hosts the synth defines these functions:
// Cardenza/Synth maps them onto its folders of the app storage protocol
// (Applications/Synth/Runtime/SynthStorage.cpp, docs/app-storage-protocol.md). Tests and tools
// define their own.
namespace synth::storage {

std::string ProjectDirectory();
// Samples are shared between apps (Shared/Samples); projects name them by file name.
std::string SampleDirectory();
// Where the 0.1.0 release kept projects and samples: read, never written (docs/app-storage-protocol.md).
std::string ReleaseProjectDirectory();
std::string ReleaseSampleDirectory();
// A sample's file: in SampleDirectory() if it is there, else the release's folder if it is there, else
// where it would go in SampleDirectory().
std::string ResolveSamplePath(const std::string& fileName);
// User preset banks, one folder per engine (`group` is the engine's display name).
std::string PresetDirectory(std::string_view group);
// The working state handed over when the app is switched away and back.
std::string SessionDirectory();
// Creates every folder above, the preset folder of each engine included. False without a
// writable card.
bool EnsureLayout();

}  // namespace synth::storage
