#pragma once

#include "Applications/Synth/UI/SynthUiModel.h"
#include "OS/UI/Canvas.h"

namespace synth::ui_render {

bool IsDx7ParameterSnapshot(const UiScreenSnapshot& snapshot);
void DrawDx7ParameterContent(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, float alpha);

}  // namespace synth::ui_render