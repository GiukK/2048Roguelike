#include "rendering/UI_Button.h"
#include "rendering/RenderSystem.h"

UI_Button::UI_Button(RenderSystem& renderer,
                     const std::string& textureId,
                     sf::Vector2f position,
                     std::function<void()> onClick)
    : renderer(&renderer),
      sprite(renderer.getTextureManager().get(textureId)),
      onClick(std::move(onClick))
{
    sprite.setOrigin(sprite.getLocalBounds().getCenter());
    sprite.setPosition(position);
    renderer.resizeSprite(textureId, sprite);
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

    sf::Vector2i mousePixel = sf::Mouse::getPosition(renderer->getWindow());
    sf::Vector2f mousePos = static_cast<sf::Vector2f>(mousePixel);
    bool hovering = sprite.getGlobalBounds().contains(mousePos);
    bool mouseDown = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
    // Rising edge: pressed THIS frame. Only a press that begins on the button
    // counts, so a drag entering it with the button already held never arms it.
    bool mouseWentDown = mouseDown && !wasMouseDown;
    wasMouseDown = mouseDown;

    if (!hovering) {
        if (enlarged) {
            sprite.setScale(sprite.getScale() / 1.1f);
            enlarged = false;
        }
        currentState = State::Idle;
        sprite.setColor(sf::Color::White);
        return;
    }

    switch (currentState) {
    case State::Idle:
        currentState = State::Hovered;
        sprite.setColor(sf::Color::Red);
        if (!enlarged) {
            sprite.setScale(sprite.getScale() * 1.1f);
            enlarged = true;
        }
        break;

    case State::Hovered:
        if (mouseWentDown) {
            currentState = State::Pressed;
            sprite.setColor(sf::Color::Blue);
        }
        break;

    case State::Pressed:
        if (!mouseDown) {
            currentState = State::Idle;
            onClick();
        }
        break;
    }
}

bool UI_Button::contains(sf::Vector2f point) const {
    return sprite.getGlobalBounds().contains(point);
}
