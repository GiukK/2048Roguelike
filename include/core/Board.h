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

    bool isAdjacent(const Coord& newCoord) const;
    bool addSlot(const Coord& newCoord);
    void spawnTile(const Coord& pos, int value);
    void moveTiles(Coord direction);

private:

    //------

    std::map<Coord, std::shared_ptr<Slot>> slots;
    std::vector<Coord> directions = { {0, 1}, {0, -1}, {1, 0}, {-1, 0} };

    //------

    sf::Texture boardTexture;
    sf::Sprite board;

};

