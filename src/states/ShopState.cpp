#include "states/ShopState.h"
#include "states/StateManager.h"
#include "core/GameRun.h"
#include "core/ItemRegistry.h"
#include "rendering/RenderSystem.h"

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

    // Picks items weighted by rarity. Duplicates are intentional:
    // a common item can fill multiple slots, rare items appear less often.
    auto picks = gameRun->pickShopItems(SHOP_SLOTS);

    for (const auto* def : picks) {
        if (def) {
            shopItemIds.push_back(def->id);
        }
    }

    rebuildShopButtons();
}

void ShopState::rebuildShopButtons() {
    shopButtons.clear();

    auto ws = renderer.getWindowSize();
    float centerX = static_cast<float>(ws.x) / 2.f;
    float centerY = static_cast<float>(ws.y) / 2.f - 50.f;
    float spacing = 250.f;
    float startX = centerX - (static_cast<float>(shopItemIds.size()) - 1.f) / 2.f * spacing;

    for (size_t i = 0; i < shopItemIds.size(); ++i) {
        const auto& def = gameRun->getItemRegistry().get(shopItemIds[i]);
        float x = startX + static_cast<float>(i) * spacing;
        size_t idx = i;

        shopButtons.emplace_back(renderer, def.textureId,
            sf::Vector2f{x, centerY},
            [this, idx]() { pendingBuyIndex = static_cast<int>(idx); });
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

    shopItemIds.erase(shopItemIds.begin() + static_cast<ptrdiff_t>(index));
    rebuildShopButtons();
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
