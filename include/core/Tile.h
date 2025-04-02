#pragma once

#include <iostream>

#include <SFML/Graphics.hpp>

class Tile{

public:

    int value;
    //------
    bool canMergeWith(const Tile& other) const;
    void mergeWith(Tile& other);
    //------

    Tile();

    void render(sf::RenderWindow& window);

    void spawn(sf::Sprite board, int x, int y);

private:


    //------


    sf::Texture tileTexture;
    sf::Sprite tile;

};

