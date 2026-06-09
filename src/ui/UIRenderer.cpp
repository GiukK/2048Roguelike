#include "ui/UIRenderer.h"
#include "rendering/RenderSystem.h"

namespace ui {

void drawNode(const UINode& node, RenderSystem& renderer) {
    if (node.type == UIType::Box) {
        const bool hasFill   = node.style.fill.a > 0;
        const bool hasBorder = node.style.border.a > 0 && node.style.borderThickness > 0.f;
        if (hasFill || hasBorder) {
            renderer.drawPixelRoundedRect(
                {{node.computedX, node.computedY}, {node.computedW, node.computedH}},
                node.style.cornerRadius, node.style.fill,
                node.style.border, node.style.borderThickness);
        }
        for (const auto& child : node.children) {
            drawNode(child, renderer);
        }
    } else { // Text — one drawText per wrapped line.
        const float lineH = renderer.measureText("Ag", node.style.charSize).y;
        for (std::size_t i = 0; i < node.wrappedLines.size(); ++i) {
            renderer.drawText(node.wrappedLines[i],
                              {node.computedX, node.computedY + static_cast<float>(i) * lineH},
                              node.style.charSize, node.style.textColor);
        }
    }
}

} // namespace ui
