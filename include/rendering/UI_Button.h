#pragma once

#include <SFML/Graphics.hpp>
#include <functional>

class RenderSystem;

class UI_Button {
public:
    enum class State {
        Idle,
        Hovered,
        Pressed
    };

    UI_Button(  RenderSystem& renderer,
                const std::string& idle_id,
    //            std::string& hover_id,
    //            std::string& pressed_id,
                std::function<void()> onClick);
                

    void update(float dt);

    State getState() const;
    sf::Sprite& getSprite();

    bool disabled{ false };

private:

    RenderSystem& renderer;

    sf::Sprite sprite;

    std::function<void()> onClick;

    State currentState = State::Idle;


};