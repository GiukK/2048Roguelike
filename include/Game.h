#pragma once

#include <SFML/Graphics.hpp>
#include "core/StateManager.h"

class Game {
public:
    Game();  // Constructor to initialize game
    void run();  // Main game loop

private:
    void processEvents();
    void update(float deltaTime);
    void render();

    sf::RenderWindow window;
    StateManager stateManager;
    sf::Clock clock;
};