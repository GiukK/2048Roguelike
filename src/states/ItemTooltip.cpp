#include "states/ItemTooltip.h"
#include "core/ItemRegistry.h"

#include <string>
#include <utility>

ui::UINode buildItemTooltip(const ItemDef& item, int cost) {
    using namespace ui;

    UINode card{UIType::Box};
    card.direction = UIDir::Column;
    card.padding = 14.f;
    card.gap = 8.f;
    card.style.cornerRadius = 14.f;
    card.style.fill = sf::Color(28, 28, 40, 240);
    card.style.border = sf::Color(210, 210, 225);
    card.style.borderThickness = 3.f;

    // Header: item icon + title, side by side and vertically centred.
    UINode icon{UIType::Image};
    icon.image = item.textureId;
    icon.imageSize = {52.f, 52.f};

    UINode title{UIType::Text};
    title.text = item.name.empty() ? item.id : item.name;
    title.style.charSize = 28;
    title.style.textColor = sf::Color(255, 220, 120);

    UINode header{UIType::Box};
    header.direction = UIDir::Row;
    header.gap = 10.f;
    header.align = UIAlign::Center;
    header.children.push_back(std::move(icon));
    header.children.push_back(std::move(title));
    card.children.push_back(std::move(header));

    if (!item.description.empty()) {
        UINode desc{UIType::Text};
        desc.text = item.description;
        desc.style.charSize = 18;
        desc.maxW = 300.f;  // wrap the description
        card.children.push_back(std::move(desc));
    }

    if (cost >= 0) {
        UINode badge{UIType::Box};
        badge.padding = 6.f;
        badge.style.fill = sf::Color(60, 50, 90);
        badge.style.cornerRadius = 6.f;

        UINode badgeText{UIType::Text};
        badgeText.text = std::to_string(cost) + " coins";
        badgeText.style.charSize = 16;
        badge.children.push_back(std::move(badgeText));

        card.children.push_back(std::move(badge));
    }

    return card;
}
