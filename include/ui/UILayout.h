#pragma once

#include <SFML/System/Vector2.hpp>
#include "ui/UINode.h"

class RenderSystem;

namespace ui {

// Two-pass layout.
// 1) measureNode (bottom-up): computes each node's intrinsic size into computedW/H
//    (Text nodes also wrap their text). Needs the renderer for text measurement.
// 2) layoutNode (top-down): places the node at (x, y) using its measured size and
//    positions its children along `direction`, aligned on the cross axis.
// Call measureNode(root) first, then layoutNode(root, x, y), then draw.
sf::Vector2f measureNode(UINode& node, RenderSystem& renderer);
void         layoutNode(UINode& node, float x, float y);

} // namespace ui
