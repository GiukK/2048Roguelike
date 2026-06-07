#pragma once

#include <SFML/Graphics.hpp>

class RenderSystem;

class GameState {
public:
    virtual ~GameState() = default;

    virtual void enter() = 0;
    virtual void exit() = 0;

    virtual void handleInput(sf::Event& event) = 0;
    virtual void update(float deltaTime) = 0;
    virtual void render(RenderSystem& renderer) = 0;
};
