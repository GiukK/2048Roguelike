#pragma once

#include "states/GameState.h"
#include "states/StateManager.h"
#include "core/GameRun.h"
#include "rendering/RenderSystem.h"
#include "core/utils/saleItem.h"

#include "SFML/Audio.hpp"

class ShopState : public GameState {
public:
    ShopState(StateManager& stateManager, RenderSystem& renderer, GameRun* gamerun);

    void fixVisualAssets();

    void enter() override;
    void exit() override;

    void handleInput(sf::Event& event) override;
    void update(float deltaTime) override;
    void render(RenderSystem& renderer) override;

    void handleClick(sf::Vector2f worldPos);

    void generateShop();
    void buyItem(saleItem& item);

private:

    GameRun* currentRun;

    StateManager& stateManager;
    RenderSystem& renderer;

    sf::Texture shopTexture;
    sf::Sprite shopSprite;

    std::vector<saleItem> itemsForSale;

};
