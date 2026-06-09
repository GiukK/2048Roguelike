#include "ui/UIInput.h"

#include <SFML/Graphics/Rect.hpp>

namespace ui {

namespace {
bool contains(const UINode& n, sf::Vector2f p) {
    return sf::FloatRect({n.computedX, n.computedY}, {n.computedW, n.computedH}).contains(p);
}
} // namespace

const UINode* hitTest(const UINode& root, sf::Vector2f point) {
    if (!contains(root, point)) return nullptr;
    // Children are drawn in order, so the LAST one is on top: test back-to-front.
    for (auto it = root.children.rbegin(); it != root.children.rend(); ++it) {
        if (const UINode* hit = hitTest(*it, point)) return hit;
    }
    return &root;  // contains the point and no child captured it
}

UINode* hitTest(UINode& root, sf::Vector2f point) {
    // Reuse the const version; the input tree is the caller's own, so the
    // const_cast back to a mutable node it owns is safe.
    return const_cast<UINode*>(hitTest(static_cast<const UINode&>(root), point));
}

bool dispatchClick(UINode& node, sf::Vector2f point) {
    if (!contains(node, point)) return false;
    // Topmost child first.
    for (auto it = node.children.rbegin(); it != node.children.rend(); ++it) {
        if (dispatchClick(*it, point)) return true;
    }
    if (node.onClick) {
        node.onClick();
        return true;
    }
    return false;
}

} // namespace ui
