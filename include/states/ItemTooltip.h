#pragma once

#include "ui/UINode.h"

struct ItemDef;

// Builds a tooltip card (a ui::UINode tree) from item data — the game-side glue
// between ItemDef and the generic UI framework. `cost` < 0 omits the price badge
// (inventory tooltips); `cost` >= 0 shows it (shop tooltips).
ui::UINode buildItemTooltip(const ItemDef& item, int cost);
