#pragma once

#include "states/GameState.h"
#include "states/StateManager.h"
#include "rendering/RenderSystem.h"


#include "SFML/Audio.hpp"

class MenuState : public GameState {
public:
    MenuState(StateManager& stateManager, RenderSystem& renderer);

    void fixVisualAssets();

    void enter() override;
    void exit() override;

    void handleInput(sf::Event& event) override;
    void update(float deltaTime) override;
    void render(RenderSystem& renderer) override;

private:

    void updateButton(sf::Sprite& button);

    StateManager& stateManager;
    RenderSystem& renderer;

    //UI
    sf::Sprite background;
    sf::Sprite startButton;
    sf::Sprite optionsButton;

};
