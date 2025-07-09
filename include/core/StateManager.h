#pragma once

#include <iostream>
#include <memory>
#include <stack>
#include <SFML/Graphics.hpp>

#include <core/GameState.h>

class StateManager {
public:
    void pushState(std::unique_ptr<GameState> state);
    void popState();
    //void changeState(std::unique_ptr<GameState> state);

    void handleInput(sf::Event& event);
    void update(float deltaTime);
    void render(sf::RenderWindow& window);

    bool isEmpty() const { return states.empty(); }

private:
    std::stack<std::unique_ptr<GameState>> states;
};
