#pragma once

#include <string_view>

#include "Library/Synth/Core/SynthStorage.h"
#include "OS/Storage/Storage.h"

namespace synth::storage {

// The id the groovebox (and the previous UI) registers; its SD folder is /CardenzaSynth.
inline constexpr std::string_view kAppId = "Cardenza/Synth";

const os::storage::AppFolder& AppFolder();

}  // namespace synth::storage
