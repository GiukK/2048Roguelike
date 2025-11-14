#include "rendering/RenderSystem.h"
#include "rendering/UI_Button.h"

UI_Button::UI_Button(   RenderSystem& renderer,
                        const std::string& idle_id,
    //                  std::string& hover_id,
    //                  std::string& pressed_id,
                        std::function<void()> onClick) : 

renderer(renderer),
sprite(renderer.getTextureManager().get(idle_id)),
onClick(onClick)
{
}

UI_Button::State UI_Button::getState() const {

    return currentState;

}

sf::Sprite& UI_Button::getSprite() {

    return sprite;

}


void UI_Button::update(float dt) {

    if (disabled) {
        sprite.setColor(sf::Color::Blue);
        return;
    }

    sf::Vector2i mousePos = sf::Mouse::getPosition(renderer.getWindow());


    if (not sprite.getGlobalBounds().contains(static_cast<sf::Vector2f>(mousePos)))
    {
                currentState = State::Idle;
                sprite.setColor(sf::Color::White);

                if (resizeFlag) {
                    sprite.setScale(sprite.getScale() / 1.1f);
                    resizeFlag = false;
                }

    }
    else if (currentState == State::Idle and //Idle -> Hover
        sprite.getGlobalBounds().contains(static_cast<sf::Vector2f>(mousePos)) )
    { 

        currentState = State::Hovered;
        sprite.setColor(sf::Color::Red);
        
        if (not resizeFlag) {
            sprite.setScale(sprite.getScale() * 1.1f);
            resizeFlag = true;
        }


    }
    else if (   currentState == State::Hovered and  // Hover -> Pressed
                sprite.getGlobalBounds().contains(static_cast<sf::Vector2f>(mousePos)) and
                sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) 
    {
        currentState = State::Pressed;
        sprite.setColor(sf::Color::Blue);

    }
    else if (currentState == State::Pressed and // Pressed -> Clicked
         not sf::Mouse::isButtonPressed(sf::Mouse::Button::Left) and  //has been released
         sprite.getGlobalBounds().contains(static_cast<sf::Vector2f>(mousePos))) // but still on the button
    {

        currentState = State::Idle;
        this->onClick();

    }
}

