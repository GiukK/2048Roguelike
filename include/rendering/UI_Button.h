#pragma once

#include <SFML/Graphics.hpp>
#include <functional>
#include <string>

class RenderSystem;

class UI_Button {
public:
    enum class State { Idle, Hovered, Pressed };

    UI_Button(RenderSystem& renderer,
              const std::string& textureId,
              sf::Vector2f position,
              std::function<void()> onClick);

    UI_Button(UI_Button&& other) noexcept = default;
    UI_Button& operator=(UI_Button&& other) noexcept = default;

    void update(float dt);
    State getState() const;
    sf::Sprite& getSprite();

    bool disabled = false;

private:
    RenderSystem* renderer;
    sf::Sprite sprite;
    std::function<void()> onClick;
    State currentState = State::Idle;
    bool enlarged = false;
};
