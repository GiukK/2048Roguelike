#pragma once

#include <iostream>
#include <memory>
#include <stack>
#include <SFML/Graphics.hpp>

#include "rendering/RenderSystem.h"
#include <states/GameState.h>



class StateManager {
public:

    void pushState(std::unique_ptr<GameState> state);
    void popState();
    //void changeState(std::unique_ptr<GameState> state);

    void handleInput(sf::Event& event);
    void update(float deltaTime);
    void render(RenderSystem& renderer);

    bool isEmpty() const { return states.empty(); }

private:
    std::stack<std::unique_ptr<GameState>> states;
};
