#pragma once
#include "Applications/Synth/UI/Model/SynthUiTypes.h"
#include "OS/UI/Canvas.h"

namespace synth::ui_render {
bool DrawSequencerHomeContent(ui::Canvas&, const ui::Rect&, const UiScreenSnapshot&, float);
// Draws the TRACKS and PATTERN pages; other SEQ pages use the shared templates.
bool DrawSequencerContextContent(ui::Canvas&, const ui::Rect&, const UiScreenSnapshot&, float);
}
