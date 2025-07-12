#pragma once

#include "core/GameState.h"
#include "core/StateManager.h"
#include "core/GameRun.h"

#include "SFML/Audio.hpp"

class ShopState : public GameState {
public:
    ShopState(StateManager& stateManager, sf::RenderWindow& window, GameRun* gamerun);

    void enter() override;
    void exit() override;

    void handleInput(sf::Event& event) override;
    void update(float deltaTime) override;
    void render(sf::RenderWindow& window) override;

private:

    GameRun* currentRun;

    StateManager& stateManager;
    sf::RenderWindow& window;

    sf::Texture shopTexture;
    sf::Sprite shopSprite;

};
