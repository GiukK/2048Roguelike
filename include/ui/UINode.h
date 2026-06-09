#pragma once

#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>
#include <string>
#include <vector>
#include <functional>

// Data-driven UI framework (generic, zero game coupling).
//
// A UINode is plain data: build a tree, run the two-pass layout (measureNode then
// layoutNode), then drawNode it. Nodes are value types and cheap to rebuild from
// game state (the retained-by-rebuild model), like PlayUI already does.
namespace ui {

enum class UIType  { Box, Text, Image };
enum class UIDir   { Row, Column };  // a Box stacks its children along this axis
enum class UIAlign { Start, Center, End };  // cross-axis alignment of children

// Visual style. Boxes use fill/border/cornerRadius; Text uses textColor/charSize.
struct UIStyle {
    sf::Color    fill            = sf::Color::Transparent;
    sf::Color    border          = sf::Color::Transparent;
    float        borderThickness = 0.f;
    float        cornerRadius    = 0.f;
    sf::Color    textColor       = sf::Color::White;
    unsigned int charSize        = 22;
};

struct UINode {
    UIType type = UIType::Box;

    std::vector<UINode> children;  // Box only
    std::string         text;      // Text only

    // Image only: texture id (TextureManager) drawn scaled to imageSize.
    std::string  image;
    sf::Vector2f imageSize{0.f, 0.f};

    UIStyle style;

    // Box layout.
    UIDir   direction = UIDir::Column;
    float   padding   = 0.f;   // inner spacing on all sides
    float   gap       = 0.f;   // space between children along `direction`
    UIAlign align     = UIAlign::Start;

    // Sizing constraints. maxW = 0 means unconstrained; on a Text node maxW > 0
    // enables word-wrapping to that width.
    float minW = 0.f;
    float minH = 0.f;
    float maxW = 0.f;

    // Interactivity: if set, this node consumes a click over it (see UIInput).
    std::function<void()> onClick;

    // --- Filled by the layout pass (do not set by hand) ---
    float computedX = 0.f;
    float computedY = 0.f;
    float computedW = 0.f;
    float computedH = 0.f;
    std::vector<std::string> wrappedLines;  // Text: result of measure-time wrapping
};

} // namespace ui
