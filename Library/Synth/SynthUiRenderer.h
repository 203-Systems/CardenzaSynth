#pragma once

#include <array>

#include "Library/Synth/ISynthEngine.h"
#include "Library/Synth/SynthTypes.h"
#include "Library/Synth/SynthUiModel.h"
#include "OS/UI/Canvas.h"

namespace synth {

void RenderSynthUiFrame(ui::Canvas& canvas,
                        const SynthUiModel& model,
                        const ISynthEngine* engine,
                        const std::array<bool, kKeyboardMatrixRows * kKeyboardMatrixColumns>& activeKeys);

}  // namespace synth