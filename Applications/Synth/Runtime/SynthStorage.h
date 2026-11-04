#pragma once

#include <string_view>

#include "Library/Synth/Core/SynthStorage.h"
#include "OS/Storage/Storage.h"

namespace synth::storage {

// The id the groovebox (and the previous UI) registers; as the primary app its folder is the product
// root, CardenzaSynth/ on the card.
inline constexpr std::string_view kAppId = "Cardenza/Synth";

const os::storage::AppFolder& AppFolder();

}  // namespace synth::storage
