#pragma once

#include "states/GameState.h"
#include "core/GameRun.h"
#include "rendering/UI_Button.h"
#include "rendering/Animation.h"

#include <SFML/Graphics.hpp>
#include <memory>
#include <vector>

class StateManager;
class RenderSystem;

class PlayState : public GameState {
public:
    PlayState(StateManager& stateManager, RenderSystem& renderer);

    void enter() override;
    void exit() override;
    void handleInput(sf::Event& event) override;
    void update(float deltaTime) override;
    void render(RenderSystem& renderer) override;

    void addAnimation(std::unique_ptr<Animation> anim);
    bool hasActiveAnimations() const { return !animations.empty(); }

    StateManager& stateManager;

private:
    RenderSystem& renderer;
    std::unique_ptr<GameRun> currentRun;
    std::vector<UI_Button> buttons;
    std::vector<std::unique_ptr<Animation>> animations;
};
