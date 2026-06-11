#pragma once

#include <string>

#include "ui/UINode.h"
#include "ui/Theme.h"

struct ItemDef;
struct CardDef;

// Tooltip builders — the game-side glue between content defs and the generic UI
// framework. `cost` < 0 omits the price badge (owned things); `cost` >= 0 shows
// it (shop). Styling comes from the theme, so the look is centrally tweakable.

// The shared layout: icon + title header, wrapped description, optional price.
// Item and card tooltips are both this card; new content types reuse it too.
ui::UINode buildInfoTooltip(const std::string& textureId, const std::string& name,
                            const std::string& description, int cost,
                            const ui::Theme& theme = ui::defaultTheme());

ui::UINode buildItemTooltip(const ItemDef& item, int cost,
                            const ui::Theme& theme = ui::defaultTheme());
ui::UINode buildCardTooltip(const CardDef& card, int cost,
                            const ui::Theme& theme = ui::defaultTheme());
