#include "states/ItemTooltip.h"
#include "core/ItemRegistry.h"
#include "core/CardRegistry.h"

#include <utility>

ui::UINode buildInfoTooltip(const std::string& textureId, const std::string& name,
                            const std::string& description, int cost,
                            const ui::Theme& theme) {
    using namespace ui;

    // Wrap width for the description (tooltip layout, not a global theme token).
    constexpr float kDescWidth = 300.f;

    UINode card{UIType::Box};
    card.direction = UIDir::Column;
    card.padding = theme.padding;
    card.gap = theme.gap;
    card.style.cornerRadius = theme.radius;
    card.style.fill = theme.panelFill;
    card.style.border = theme.panelBorder;
    card.style.borderThickness = theme.borderThickness;

    // Header: icon + title, side by side and vertically centred.
    UINode icon{UIType::Image};
    icon.image = textureId;
    icon.imageSize = {theme.iconSize, theme.iconSize};

    UINode title{UIType::Text};
    title.text = name;
    title.style.charSize = theme.titleSize;
    title.style.textColor = theme.accent;

    UINode header{UIType::Box};
    header.direction = UIDir::Row;
    header.gap = theme.gap;
    header.align = UIAlign::Center;
    header.children.push_back(std::move(icon));
    header.children.push_back(std::move(title));
    card.children.push_back(std::move(header));

    if (!description.empty()) {
        UINode desc{UIType::Text};
        desc.text = description;
        desc.style.charSize = theme.bodySize;
        desc.style.textColor = theme.textPrimary;
        desc.maxW = kDescWidth;  // wrap the description
        card.children.push_back(std::move(desc));
    }

    if (cost >= 0) {
        UINode badge{UIType::Box};
        badge.padding = theme.badgePadding;
        badge.style.fill = theme.badgeFill;
        badge.style.cornerRadius = theme.badgeRadius;

        UINode badgeText{UIType::Text};
        badgeText.text = std::to_string(cost) + " coins";
        badgeText.style.charSize = theme.smallSize;
        badgeText.style.textColor = theme.textPrimary;
        badge.children.push_back(std::move(badgeText));

        card.children.push_back(std::move(badge));
    }

    return card;
}

ui::UINode buildItemTooltip(const ItemDef& item, int cost, const ui::Theme& theme) {
    return buildInfoTooltip(item.textureId,
                            item.name.empty() ? item.id : item.name,
                            item.description, cost, theme);
}

ui::UINode buildCardTooltip(const CardDef& card, int cost, const ui::Theme& theme) {
    return buildInfoTooltip(card.textureId,
                            card.name.empty() ? card.id : card.name,
                            card.description, cost, theme);
}
