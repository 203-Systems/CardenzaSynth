#pragma once

#include <cstdint>

#include "Applications/Synth/UI/Groove/GrooveController.h"
#include "Applications/Synth/UI/Groove/Render/Animator.h"
#include "Applications/Synth/UI/Groove/Render/Gfx.h"

namespace groove {

// Draws one frame. The animator must have been updated for this frame.
void RenderFrame(Gfx& g, const Controller& controller, Animator& animator);

namespace render {

// Scene content for an explicit page, so transitions can draw the outgoing page too (SceneRender.cpp).
void SceneContent(Gfx& g, const Controller& controller, Animator& animator, Scene scene, std::uint8_t page);
// The FN panel page (held FN or SETUP), drawn like an INSTR page (SceneRender.cpp).
// The FN menu of the current screen on the GRID cells (held FN and the open menu); `t` drops the tiles in.
void MenuContent(Gfx& g, const Controller& controller, Animator& animator, float t);
// Modal content replaces the content area (ModalRender.cpp).
void ModalContent(Gfx& g, const Controller& controller, Animator& animator, const Modal& modal);
const char* ModalHint(const Controller& controller, const Modal& modal);

}  // namespace render

}  // namespace groove
