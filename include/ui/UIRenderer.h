#pragma once

#include "ui/UINode.h"

class RenderSystem;

namespace ui {

// Draws a laid-out node tree. Generic: it only knows UINodes and the RenderSystem
// primitives (drawPixelRoundedRect / drawText) — no game concepts. Call after
// measureNode + layoutNode have filled the computed rects.
void drawNode(const UINode& node, RenderSystem& renderer);

} // namespace ui
