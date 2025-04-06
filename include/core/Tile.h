#pragma once

#include <iostream>

#include <SFML/Graphics.hpp>


class Slot;

class Tile{

public:

    //------
    std::shared_ptr<Slot> slot;
    //------

    Tile(std::shared_ptr<Slot> slot, int value);

    void render(sf::RenderWindow& window);

    sf::Vector2f getPosition();


    void changeSlot(std::shared_ptr<Slot> first, std::shared_ptr<Slot> second);
    void mergeIntoSlot(std::shared_ptr<Slot> other);

    int getValue();
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

