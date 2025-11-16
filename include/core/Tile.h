#pragma once

#include <iostream>

#include <SFML/Graphics.hpp>
#include "rendering/Animation.h"

class Slot;
class RenderSystem;

class Tile{

public:

    enum class State {
        Idle,
        Hovered,
        Pressed
    };

    bool selected{ false };

    State currentState = State::Idle;
    bool resizeFlag{ false };

    Tile(RenderSystem& renderer , Slot* slot, int value);
    //raw ptr to owner - Slot
    Slot* slot;

    //------
    void render( RenderSystem& renderer);
    void update(float deltaTime);
    //returns pos in float terms (sprite)
    sf::Vector2f getPosition();
    //changes sprite when merged
    void changeSprite();
    //------


    //tile movement
    void changeSlot(Slot* first, Slot* second);
    //tile merge
    void mergeIntoSlot(Slot* other);

    bool mergedThisSweep = false;

    //game value of tile
    int getValue() const;
    void setValue(int x);

    bool isAnimating() const { return animating; }
private:

    RenderSystem& renderer;
    sf::Sprite tile;

    // rendering (bad)
    bool mouseDownLastFrame = false;
    bool wasHovering = false;
    bool pressedInside = false;


    void fixVisualAssets();


    //game value
    int value{2};

    //------MOVING ANIMATION
    sf::Vector2f targetPosition; // Destination to move toward
    float moveSpeed = 800.f;   // Pixels per second
    bool animating = false;

    sf::Vector2f startPosition;
    float animationTime = 0.f;
    float animationDuration = 0.1f; // Total animation time in seconds
    //-----------------


};

