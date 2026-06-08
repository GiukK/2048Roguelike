#include "states/ShopState.h"
#include "states/StateManager.h"
#include "core/GameRun.h"
#include "core/ItemRegistry.h"
#include "rendering/RenderSystem.h"
#include "Debug.h"

#include <algorithm>

ShopState::ShopState(StateManager& stateManager, RenderSystem& renderer, GameRun* gameRun)
    : stateManager(stateManager),
      renderer(renderer),
      gameRun(gameRun),
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

    rebuildShopButtons();
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

void ShopState::handleInput(sf::Event& event) {
    auto* key = event.getIf<sf::Event::KeyReleased>();
    if (key && key->scancode == sf::Keyboard::Scancode::Escape) {
        exit();
    }
}

void ShopState::update(float deltaTime) {
    gameRun->update(deltaTime);

    for (auto& anim : decorAnimations) {
        anim.update(deltaTime);
    }

    for (auto& btn : shopButtons) {
        btn.update(deltaTime);
    }

    if (pendingBuyIndex >= 0) {
        buyItem(static_cast<size_t>(pendingBuyIndex));
        pendingBuyIndex = -1;
    }
}

void ShopState::render(RenderSystem& renderer) {
    gameRun->render(renderer);
    renderer.draw(shopSprite);

    // Draw shop items and their prices
    for (size_t i = 0; i < shopButtons.size(); ++i) {
        renderer.draw(shopButtons[i].getSprite());

        // Price tag: centered below the item sprite
        const auto& def = gameRun->getItemRegistry().get(shopItemIds[i]);
        int cost = gameRun->getEffectiveCost(def);
        sf::Vector2f itemPos = shopButtons[i].getSprite().getPosition();
        float priceY = itemPos.y + shopButtons[i].getSprite().getGlobalBounds().size.y / 2.f + 20.f;

        renderer.drawNumber(static_cast<unsigned int>(cost), {itemPos.x, priceY}, 6.f);
    }

    for (auto& anim : decorAnimations) {
        renderer.draw(anim.getSprite());
    }
}
