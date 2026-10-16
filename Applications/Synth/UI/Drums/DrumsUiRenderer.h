#pragma once

#include <array>
#include <string>

#include "Library/Synth/Core/SynthTypes.h"
#include "OS/UI/Canvas.h"

namespace synth::ui_render {

// Eight pads on the shared performance-column grid; returns the names of pads being hit.
std::string DrawDrumsPads(ui::Canvas& canvas,
                          int x,
                          int y,
                          int height,
                          const std::array<bool, kKeyboardMatrixRows * kKeyboardMatrixColumns>& activeKeys,
                          float alpha);

}  // namespace synth::ui_render
