#pragma once

#include <iostream>
#include <map>
#include <memory>
#include <vector>

#include "Coord.h"
#include "Slot.h"

#include "SFML/Graphics.hpp"

class Board {

public:

    Board();

    void render(sf::RenderWindow& window);

    //------

private:

    //------

    //------

    sf::Texture boardTexture;
    sf::Sprite board;

};

