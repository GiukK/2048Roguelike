#pragma once

#include <iostream>

#include <SFML/Graphics.hpp>


class Slot;

class Tile{

public:

    //------
    Slot* slot;
    //------

    Tile(Slot* slot, int value);

    void render(sf::RenderWindow& window);

    sf::Vector2f getPosition();


    void changeSlot(Slot* first, Slot* second);
    void mergeIntoSlot(Slot* other);

    int getValue() const;
    void setValue(int x);

    void changeSprite();

private:


    int value{2};

    //------ANIMATION

    sf::Vector2f targetPosition; // Destination to move toward
    float moveSpeed = 800.f;   // Pixels per second
    //-----------------

    sf::Texture tileTexture;
    sf::Sprite tile;

};

