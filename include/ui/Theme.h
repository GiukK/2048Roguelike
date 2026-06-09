#pragma once

#include <SFML/Graphics/Color.hpp>

// The single source of truth for the UI's look. UI generators read theme tokens
// instead of hardcoded colors/sizes/spacing, so the whole UI re-skins from one
// place — the seam for the future art direction (and, later, loading from
// assets/configs/theme.json). Keep it generic: palette + typography + spacing,
// not widget-specific layout.
namespace ui {

struct Theme {
    // Palette
    sf::Color panelFill      = sf::Color(28, 28, 40, 240);
    sf::Color panelBorder    = sf::Color(210, 210, 225);
    sf::Color accent         = sf::Color(255, 220, 120);  // titles / highlights
    sf::Color textPrimary    = sf::Color::White;
    sf::Color textMuted      = sf::Color(200, 200, 210);
    sf::Color badgeFill      = sf::Color(60, 50, 90);

    // Typography (character sizes)
    unsigned int titleSize = 28;
    unsigned int bodySize  = 18;
    unsigned int smallSize = 16;  // badges

    // Spacing / metrics (screen px)
    float padding         = 14.f;
    float gap             = 8.f;
    float radius          = 14.f;
    float borderThickness = 3.f;
    float badgePadding    = 6.f;
    float badgeRadius     = 6.f;
    float iconSize        = 52.f;  // standard square icon
};

// The active default theme (a stable static). Swap its values — or, later, load
// from config — to re-skin everything.
const Theme& defaultTheme();

} // namespace ui
