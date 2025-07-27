#pragma once

#include <iostream>

#include <SFML/Graphics.hpp>


class Slot;
class RenderSystem;


class Tile{

public:

    Tile(RenderSystem& renderer , Slot* slot, int value);
    //raw ptr to owner - Slot
    Slot* slot;

    //------
    void render( RenderSystem& renderer);
    //returns pos in float terms (sprite)
    sf::Vector2f getPosition();
    //changes sprite when merged
    void changeSprite();
    //------


    //tile movement
    void changeSlot(Slot* first, Slot* second);
    //tile merge
    void mergeIntoSlot(Slot* other);


    //game value of tile
    int getValue() const;
    void setValue(int x);


private:

    RenderSystem& renderer;
    sf::Sprite tile;

    void fixVisualAssets();


    //game value
    int value{2};

    //------ANIMATION
    sf::Vector2f targetPosition; // Destination to move toward
    float moveSpeed = 800.f;   // Pixels per second
    //-----------------


};

