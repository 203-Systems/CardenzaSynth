#pragma once

#include <array>

#include "Applications/Synth/UI/SynthUiModel.h"
#include "Library/Synth/Core/ISynthEngine.h"
#include "Library/Synth/Core/SynthTypes.h"
#include "OS/UI/Canvas.h"

namespace synth {

void RenderSynthUiFrame(ui::Canvas& canvas,
                        const SynthUiModel& model,
                        const ISynthEngine* engine,
                        const std::array<bool, kKeyboardMatrixRows * kKeyboardMatrixColumns>& activeKeys);

}  // namespace synth