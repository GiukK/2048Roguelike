#pragma once

#include <memory>
#include "Tile.h"

#include "SFML/Graphics.hpp"

class Tile;

class Slot {
private:

public:

    int col;//x
    int row;//x

    std::unique_ptr<Tile> tile = nullptr;

    sf::Texture slotTexture;
    sf::Sprite slot;

    Slot(int col, int row);

    void render(sf::RenderWindow& window);

    // In Slot.h - Change isEmpty() from a non-const to a const method:
    bool isEmpty() const;  // Note the const keyword

    void setTile(std::unique_ptr<Tile> newTile);

    void removeTile();

    std::unique_ptr<Tile> releaseTile();
};
