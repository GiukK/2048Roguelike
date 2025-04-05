#pragma once

#include <iostream>

#include <SFML/Graphics.hpp>


class Slot;

class Tile{

public:

    //------
    std::shared_ptr<Slot> slot;
    //------

    Tile(std::shared_ptr<Slot> slot);

    void render(sf::RenderWindow& window);

    sf::Vector2f getPosition();


    void changeSlot(std::shared_ptr<Slot> first, std::shared_ptr<Slot> second);



private:


    //------

    int value{};

    sf::Texture tileTexture;
    sf::Sprite tile;

};

