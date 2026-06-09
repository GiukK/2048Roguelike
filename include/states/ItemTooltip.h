#pragma once

#include "ui/UINode.h"
#include "ui/Theme.h"

struct ItemDef;

// Builds a tooltip card (a ui::UINode tree) from item data — the game-side glue
// between ItemDef and the generic UI framework. `cost` < 0 omits the price badge
// (inventory tooltips); `cost` >= 0 shows it (shop tooltips). Styling comes from
// the theme, so the look is centrally tweakable.
ui::UINode buildItemTooltip(const ItemDef& item, int cost,
                            const ui::Theme& theme = ui::defaultTheme());
