#pragma once

#include <iostream>

#include <SFML/Graphics.hpp>


class Slot;

class Tile{

public:

    Tile(Slot* slot, int value);
    //raw ptr to owner - Slot
    Slot* slot;

    //------
    void render(sf::RenderWindow& window);
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

    //game value
    int value{2};

    //------ANIMATION
    sf::Vector2f targetPosition; // Destination to move toward
    float moveSpeed = 800.f;   // Pixels per second
    sf::Texture tileTexture;
    sf::Sprite tile;
    //-----------------


};

