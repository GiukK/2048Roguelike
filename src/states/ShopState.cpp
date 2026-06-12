#include "states/ShopState.h"
#include "states/StateManager.h"
#include "states/ItemTooltip.h"
#include "core/GameRun.h"
#include "core/ItemRegistry.h"
#include "core/CardRegistry.h"
#include "rendering/RenderSystem.h"
#include "ui/UI.h"
#include "Debug.h"

#include <algorithm>

ShopState::ShopState(StateManager& stateManager, RenderSystem& renderer, GameRun* gameRun,
                     RenderBehind renderBehind)
    : stateManager(stateManager),
      renderer(renderer),
      gameRun(gameRun),
      renderBehind(std::move(renderBehind)),
      shopSprite(renderer.getTextureManager().get("shop"))
{
    initVisuals();
    enter();
}

void ShopState::initVisuals() {
    for (int i = 1; i <= 14; ++i) {
        decorAnimations.emplace_back(renderer, "coin_animation",
            sf::Vector2i{32, 32}, 8, sf::Vector2f{120.f * i, 900.f});
    }

    renderer.resizeSprite("shop", shopSprite);
    shopSprite.setOrigin(shopSprite.getLocalBounds().getCenter());
    auto ws = renderer.getWindowSize();
    shopSprite.setPosition({static_cast<float>(ws.x) / 2.f,
                            static_cast<float>(ws.y) / 2.f});
}

void ShopState::enter() {
    generateShop();
}

void ShopState::exit() {
    shopItemIds.clear();
    shopButtons.clear();
    shopCardIds.clear();
    cardButtons.clear();
    gameRun->shopOpen = false;
    stateManager.requestPop();
}

void ShopState::generateShop() {
    shopItemIds.clear();
    shopButtons.clear();

    if (debug::Enabled) {
        // Debug shop: stock every registered item so they can all be tested at
        // once. Count comes from the registry, never hardcoded.
        for (const auto* def : gameRun->getItemRegistry().getAll()) {
            shopItemIds.push_back(def->id);
        }
    } else {
        // Picks items weighted by rarity. Duplicates are intentional:
        // a common item can fill multiple slots, rare items appear less often.
        auto picks = gameRun->pickShopItems(SHOP_SLOTS);
        for (const auto* def : picks) {
            if (def) {
                shopItemIds.push_back(def->id);
            }
        }
    }

    // Card stock. Debug: the whole catalogue, always — duplicates included, so
    // stacked copies (two "Two for Two" both firing) can be tested. Normal play
    // policy for now: only cards the player doesn't own yet (the model allows
    // stacking; offering duplicates is a shop-design decision for later).
    shopCardIds.clear();
    for (const auto* def : gameRun->getCardRegistry().getAll()) {
        if (debug::Enabled || !gameRun->ownsCard(def->id)) {
            shopCardIds.push_back(def->id);
        }
    }

    rebuildShopButtons();
    rebuildCardButtons();
}

void ShopState::rebuildShopButtons() {
    shopButtons.clear();
    if (shopItemIds.empty()) return;

    auto ws = renderer.getWindowSize();
    float centerX = static_cast<float>(ws.x) / 2.f;
    float centerY = static_cast<float>(ws.y) / 2.f - 50.f;

    const size_t count = shopItemIds.size();

    // Spacing shrinks once items would overflow a usable band of the screen, so
    // the row always fits — this keeps the debug shop (which lists every item)
    // readable for any item count. With the normal 3 slots it stays at 250.
    const float band = static_cast<float>(ws.x) * 0.9f;
    float spacing = std::min(250.f, band / static_cast<float>(count));
    float startX = centerX - (static_cast<float>(count) - 1.f) / 2.f * spacing;

    // Icons that would overflow their slot are scaled down (keeping a gap), so a
    // crowded shop never overlaps. Normal-sized icons are left untouched.
    const float maxIconWidth = spacing * 0.8f;

    for (size_t i = 0; i < count; ++i) {
        const auto& def = gameRun->getItemRegistry().get(shopItemIds[i]);
        float x = startX + static_cast<float>(i) * spacing;
        size_t idx = i;

        shopButtons.emplace_back(renderer, def.textureId,
            sf::Vector2f{x, centerY},
            [this, idx]() { pendingBuyIndex = static_cast<int>(idx); });

        sf::Sprite& sprite = shopButtons.back().getSprite();
        float width = sprite.getGlobalBounds().size.x;
        if (width > maxIconWidth) {
            sprite.setScale(sprite.getScale() * (maxIconWidth / width));
        }
    }
}

void ShopState::rebuildCardButtons() {
    cardButtons.clear();
    if (shopCardIds.empty()) return;

    // Card row: centred below the item row, mirroring its layout.
    auto ws = renderer.getWindowSize();
    float centerX = static_cast<float>(ws.x) / 2.f;
    float cardY = static_cast<float>(ws.y) / 2.f + 280.f;

    const size_t count = shopCardIds.size();
    const float band = static_cast<float>(ws.x) * 0.9f;
    float spacing = std::min(250.f, band / static_cast<float>(count));
    float startX = centerX - (static_cast<float>(count) - 1.f) / 2.f * spacing;
    const float maxIconWidth = spacing * 0.8f;

    for (size_t i = 0; i < count; ++i) {
        const auto& def = gameRun->getCardRegistry().get(shopCardIds[i]);
        float x = startX + static_cast<float>(i) * spacing;
        size_t idx = i;

        cardButtons.emplace_back(renderer, def.textureId,
            sf::Vector2f{x, cardY},
            [this, idx]() { pendingBuyCardIndex = static_cast<int>(idx); });

        sf::Sprite& sprite = cardButtons.back().getSprite();
        float width = sprite.getGlobalBounds().size.x;
        if (width > maxIconWidth) {
            sprite.setScale(sprite.getScale() * (maxIconWidth / width));
        }
    }
}

void ShopState::buyItem(size_t index) {
    if (index >= shopItemIds.size()) return;

    const auto& def = gameRun->getItemRegistry().get(shopItemIds[index]);
    int cost = gameRun->getEffectiveCost(def);

    if (gameRun->getCoins() < cost) return;
    if (gameRun->isInventoryFull()) return;

    gameRun->addCoins(-cost);
    gameRun->addItem(shopItemIds[index]);

    // Debug shop stays fully stocked so every item can be grabbed repeatedly;
    // normally a purchased item is removed from the shop.
    if (!debug::Enabled) {
        shopItemIds.erase(shopItemIds.begin() + static_cast<ptrdiff_t>(index));
        rebuildShopButtons();
    }
}

void ShopState::buyCard(size_t index) {
    if (index >= shopCardIds.size()) return;

    const auto& def = gameRun->getCardRegistry().get(shopCardIds[index]);
    int cost = gameRun->getEffectiveCost(def);

    if (gameRun->getCoins() < cost) return;
    // Cards mount at run scope, not in the inventory — no inventory-full check.
    if (!gameRun->acquireCard(shopCardIds[index])) return;

    gameRun->addCoins(-cost);

    // Debug shop stays fully stocked (infinite copies, for synergy testing);
    // normally a bought card leaves this shop's stock.
    if (!debug::Enabled) {
        shopCardIds.erase(shopCardIds.begin() + static_cast<ptrdiff_t>(index));
        rebuildCardButtons();
    }
}

void ShopState::handleInput(sf::Event& event) {
    auto* key = event.getIf<sf::Event::KeyReleased>();
    if (key && key->scancode == sf::Keyboard::Scancode::Escape) {
        exit();
    }
}

void ShopState::update(float deltaTime) {
    // The world below is frozen while the shop is open (modal) — it's only drawn,
    // never updated, so the inventory/board can't be interacted with mid-shop.
    for (auto& anim : decorAnimations) {
        anim.update(deltaTime);
    }

    for (auto& btn : shopButtons) {
        btn.update(deltaTime);
    }
    for (auto& btn : cardButtons) {
        btn.update(deltaTime);
    }

    if (pendingBuyIndex >= 0) {
        buyItem(static_cast<size_t>(pendingBuyIndex));
        pendingBuyIndex = -1;
    }
    if (pendingBuyCardIndex >= 0) {
        buyCard(static_cast<size_t>(pendingBuyCardIndex));
        pendingBuyCardIndex = -1;
    }
}

void ShopState::render(RenderSystem& renderer) {
    // Draw the play screen behind (PlayState handles the board/UI view switches),
    // then the shop overlay and its widgets purely in the UI view.
    if (renderBehind) renderBehind(renderer);

    renderer.useUIView();
    renderer.draw(shopSprite);

    // Draw shop items and their prices. Bounded by BOTH lists (they are rebuilt
    // together, but a desync must degrade to a missing price, not an OOB read).
    for (size_t i = 0; i < shopButtons.size() && i < shopItemIds.size(); ++i) {
        renderer.draw(shopButtons[i].getSprite());

        // Price tag: centered below the item sprite
        const auto& def = gameRun->getItemRegistry().get(shopItemIds[i]);
        int cost = gameRun->getEffectiveCost(def);
        sf::Vector2f itemPos = shopButtons[i].getSprite().getPosition();
        float priceY = itemPos.y + shopButtons[i].getSprite().getGlobalBounds().size.y / 2.f + 20.f;

        renderer.drawNumber(static_cast<unsigned int>(cost), {itemPos.x, priceY}, 6.f);
    }

    // Card stock below the items, same presentation: sprite + price tag.
    for (size_t i = 0; i < cardButtons.size() && i < shopCardIds.size(); ++i) {
        renderer.draw(cardButtons[i].getSprite());

        const auto& def = gameRun->getCardRegistry().get(shopCardIds[i]);
        int cost = gameRun->getEffectiveCost(def);
        sf::Vector2f cardPos = cardButtons[i].getSprite().getPosition();
        float priceY = cardPos.y + cardButtons[i].getSprite().getGlobalBounds().size.y / 2.f + 20.f;

        renderer.drawNumber(static_cast<unsigned int>(cost), {cardPos.x, priceY}, 6.f);
    }

    for (auto& anim : decorAnimations) {
        renderer.draw(anim.getSprite());
    }

    renderHoveredTooltip(renderer);
}

void ShopState::renderHoveredTooltip(RenderSystem& renderer) {
    const sf::Vector2f mouse =
        renderer.mapPixelToUI(sf::Mouse::getPosition(renderer.getWindow()));

    for (std::size_t i = 0; i < shopButtons.size() && i < shopItemIds.size(); ++i) {
        if (!shopButtons[i].contains(mouse)) continue;

        const ItemDef& def = gameRun->getItemRegistry().get(shopItemIds[i]);
        ui::UINode tip = buildItemTooltip(def, gameRun->getEffectiveCost(def));
        ui::measureNode(tip, renderer);

        // Shop items sit in a row, so place the tooltip above the item (below if
        // there's no room), centred horizontally and clamped to the screen.
        const sf::FloatRect b = shopButtons[i].getSprite().getGlobalBounds();
        const auto ws = renderer.getWindowSize();
        float tx = std::clamp(b.position.x + b.size.x / 2.f - tip.computedW / 2.f,
                              10.f, static_cast<float>(ws.x) - tip.computedW - 10.f);
        float ty = b.position.y - tip.computedH - 16.f;
        if (ty < 10.f) ty = b.position.y + b.size.y + 16.f;

        ui::layoutNode(tip, tx, ty);
        ui::drawNode(tip, renderer);
        return;
    }

    for (std::size_t i = 0; i < cardButtons.size() && i < shopCardIds.size(); ++i) {
        if (!cardButtons[i].contains(mouse)) continue;

        const CardDef& def = gameRun->getCardRegistry().get(shopCardIds[i]);
        ui::UINode tip = buildCardTooltip(def, gameRun->getEffectiveCost(def));
        ui::measureNode(tip, renderer);

        // The card row sits low on the screen, so the tooltip goes above it.
        const sf::FloatRect b = cardButtons[i].getSprite().getGlobalBounds();
        const auto ws = renderer.getWindowSize();
        float tx = std::clamp(b.position.x + b.size.x / 2.f - tip.computedW / 2.f,
                              10.f, static_cast<float>(ws.x) - tip.computedW - 10.f);
        float ty = b.position.y - tip.computedH - 16.f;
        if (ty < 10.f) ty = b.position.y + b.size.y + 16.f;

        ui::layoutNode(tip, tx, ty);
        ui::drawNode(tip, renderer);
        return;
    }
}
