#include "ui/UILayout.h"
#include "rendering/RenderSystem.h"

#include <algorithm>

namespace ui {

sf::Vector2f measureNode(UINode& node, RenderSystem& renderer) {
    if (node.type == UIType::Text) {
        // Wrap to maxW if set, otherwise a single line.
        if (node.maxW > 0.f) {
            node.wrappedLines = renderer.wrapText(node.text, node.maxW, node.style.charSize);
        } else {
            node.wrappedLines = { node.text };
        }
        const float lineH = renderer.measureText("Ag", node.style.charSize).y;
        float width = 0.f;
        for (const auto& line : node.wrappedLines) {
            width = std::max(width, renderer.measureText(line, node.style.charSize).x);
        }
        const float height = lineH * static_cast<float>(node.wrappedLines.size());
        node.computedW = std::max(width, node.minW);
        node.computedH = std::max(height, node.minH);
        return { node.computedW, node.computedH };
    }

    // Box: measure children, then sum along the main axis and take the max on the
    // cross axis, plus padding and inter-child gaps.
    const bool row = (node.direction == UIDir::Row);
    float mainSize = 0.f;
    float crossSize = 0.f;
    for (auto& child : node.children) {
        const sf::Vector2f cs = measureNode(child, renderer);
        mainSize  += row ? cs.x : cs.y;
        crossSize  = std::max(crossSize, row ? cs.y : cs.x);
    }
    if (node.children.size() > 1) {
        mainSize += node.gap * static_cast<float>(node.children.size() - 1);
    }
    mainSize  += 2.f * node.padding;
    crossSize += 2.f * node.padding;

    float width  = row ? mainSize : crossSize;
    float height = row ? crossSize : mainSize;
    if (node.maxW > 0.f) width = std::min(width, node.maxW);
    node.computedW = std::max(width, node.minW);
    node.computedH = std::max(height, node.minH);
    return { node.computedW, node.computedH };
}

void layoutNode(UINode& node, float x, float y) {
    node.computedX = x;
    node.computedY = y;
    if (node.type == UIType::Text) return;  // leaf

    const bool row = (node.direction == UIDir::Row);
    // Pen starts inside the padding; advances along the main axis per child.
    float mainCursor = (row ? x : y) + node.padding;
    const float crossStart = (row ? y : x) + node.padding;
    const float crossAvail = (row ? node.computedH : node.computedW) - 2.f * node.padding;

    for (auto& child : node.children) {
        const float childMain  = row ? child.computedW : child.computedH;
        const float childCross = row ? child.computedH : child.computedW;

        float crossOffset = 0.f;
        if (node.align == UIAlign::Center) crossOffset = (crossAvail - childCross) / 2.f;
        else if (node.align == UIAlign::End) crossOffset = crossAvail - childCross;

        const float childX = row ? mainCursor : crossStart + crossOffset;
        const float childY = row ? crossStart + crossOffset : mainCursor;

        layoutNode(child, childX, childY);
        mainCursor += childMain + node.gap;
    }
}

} // namespace ui
