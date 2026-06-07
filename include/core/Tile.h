#pragma once

#include <SFML/Graphics.hpp>

class Slot;
class RenderSystem;

class Tile {
public:
    enum class Visual { Idle, Hovered, Pressed };

    Tile(RenderSystem& renderer, Slot* slot, int value);

    void render(RenderSystem& renderer);
    void update(float deltaTime);

    sf::Vector2f getPosition() const;
    sf::FloatRect getBounds() const;
    int getValue() const;
    void setValue(int newValue);

    void changeSprite();
    void changeSlot(Slot* from, Slot* to);
    void mergeIntoSlot(Slot* target);
    void animateTo(sf::Vector2f target);

    bool isAnimating() const { return animating; }

    void setVisual(Visual state);
    void setSelected(bool sel);
    bool isSelected() const { return selected; }

    bool mergedThisSweep = false;
    Slot* slot;

private:
    void initVisuals();
    void applyVisualStyle();

    RenderSystem& renderer;
    sf::Sprite sprite;
    int value;

    Visual visualState = Visual::Idle;
    bool selected = false;
    sf::Vector2f baseScale;

    // slide animation
    sf::Vector2f startPos;
    sf::Vector2f targetPos;
    float animTime = 0.f;
    float animDuration = 0.1f;
    bool animating = false;
};
