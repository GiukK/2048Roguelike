#pragma once

#include <iostream>

#include <SFML/Graphics.hpp>

class Tile{

public:

    //------

    //------

    Tile();

    void render(sf::RenderWindow& window);

    void spawn(sf::Sprite board, int x, int y);

private:


    //------

    int value{ 2 };

    sf::Texture tileTexture;
    sf::Sprite tile;

};

