#pragma once

#include "states/GameState.h"
#include "states/StateManager.h"
#include "core/GameRun.h"
#include "rendering/RenderSystem.h"


#include "SFML/Audio.hpp"

class PlayState : public GameState {
public:
    PlayState(StateManager& stateManager, RenderSystem& renderer);

    void enter() override;
    void exit() override;

    void handleInput(sf::Event& event) override;
    void update(float deltaTime) override;
    void render(RenderSystem& renderer) override;

    StateManager& stateManager;
    RenderSystem& renderer;

private:

    std::unique_ptr<GameRun> currentRun;


};
