#pragma once

#include "core/GameState.h"
#include "core/StateManager.h"
#include "core/GameRun.h"

#include "SFML/Audio.hpp"

class PlayState : public GameState {
public:
    PlayState(StateManager& stateManager, sf::RenderWindow& window);

    void enter() override;
    void exit() override;

    void handleInput(sf::Event& event) override;
    void update(float deltaTime) override;
    void render(sf::RenderWindow& window) override;

private:

    std::unique_ptr<GameRun> currentRun;

    StateManager& stateManager;
    sf::RenderWindow& window;

};
