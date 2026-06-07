#pragma once

#include "states/GameState.h"
#include "rendering/UI_Button.h"

#include <SFML/Graphics.hpp>
#include <vector>

class StateManager;
class RenderSystem;

class MenuState : public GameState {
public:
    MenuState(StateManager& stateManager, RenderSystem& renderer);

    void enter() override;
    void exit() override;
    void handleInput(sf::Event& event) override;
    void update(float deltaTime) override;
    void render(RenderSystem& renderer) override;

private:
    void initVisuals();

    StateManager& stateManager;
    RenderSystem& renderer;

    sf::Sprite background;
    std::vector<UI_Button> buttons;
};
