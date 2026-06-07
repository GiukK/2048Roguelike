#pragma once

#include <SFML/Graphics.hpp>
#include "states/StateManager.h"
#include "rendering/RenderSystem.h"

class Game {
public:
    Game();
    void run();

private:
    void processEvents();
    void update(float deltaTime);
    void render();

    sf::RenderWindow window;
    sf::Clock clock;
    StateManager stateManager;
    RenderSystem renderer;
};
