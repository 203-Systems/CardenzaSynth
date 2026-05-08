#pragma once

#include "Library/Synth/SynthUiModel.h"
#include "OS/UI/Canvas.h"

namespace synth::ui_render {

bool IsJunoParameterSnapshot(const UiScreenSnapshot& snapshot);
void DrawJunoParameterContent(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, float alpha);

}  // namespace synth::ui_render