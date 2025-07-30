#pragma once

#include "states/GameState.h"
#include "states/StateManager.h"
#include "core/GameRun.h"
#include "rendering/RenderSystem.h"
#include "rendering/UI_Button.h"


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
    void buyItem(const std::string& name);

private:

    GameRun* currentRun;

    StateManager& stateManager;
    RenderSystem& renderer;

    sf::Sprite shopSprite;

    std::vector<UI_Button> itemsForSale;

};
