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

    // Screen-space hit-test (buttons live in the 1:1 UI view, so `point` is a raw
    // pixel). Lets the play state give UI priority over board pan/selection.
    bool contains(sf::Vector2f point) const;

    bool disabled = false;

private:
    RenderSystem* renderer;
    sf::Sprite sprite;
    std::function<void()> onClick;
    State currentState = State::Idle;
    bool enlarged = false;
    // Tracks the previous frame's button state so onClick only fires for a press
    // that STARTED on the button — dragging over it with the button already held
    // (e.g. a board pan) must not trigger it.
    bool wasMouseDown = false;
};
