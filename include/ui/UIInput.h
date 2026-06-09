#pragma once

#include <SFML/System/Vector2.hpp>
#include "ui/UINode.h"

namespace ui {

// The single input pass. Hit-test the tree front-to-back (last child = topmost),
// so the deepest node under `point` wins. This is what — at Step 4 — replaces
// UI_Button's self-polling and the manual isPointOverUI: the caller asks the UI
// first, and only if nothing here captures the input does it reach the world.

// Topmost node whose computed rect contains `point`, or nullptr. (const + mutable
// overloads so callers can both inspect and invoke onClick.)
const UINode* hitTest(const UINode& root, sf::Vector2f point);
UINode*       hitTest(UINode& root, sf::Vector2f point);

// Invokes the onClick of the topmost node-with-onClick under `point`. Returns
// true if a handler fired (i.e. the click was consumed).
bool dispatchClick(UINode& root, sf::Vector2f point);

} // namespace ui
