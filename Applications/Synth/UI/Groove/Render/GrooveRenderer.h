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
// Modal content replaces the content area (ModalRender.cpp).
void ModalContent(Gfx& g, const Controller& controller, Animator& animator, const Modal& modal);
const char* ModalHint(const Controller& controller, const Modal& modal);

}  // namespace render

}  // namespace groove
