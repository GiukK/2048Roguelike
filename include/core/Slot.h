#pragma once

#include <memory>
#include "Tile.h"

class Slot {
private:
    std::unique_ptr<Tile> tile;

public:
    bool isEmpty() const;
    void setTile(std::unique_ptr<Tile> newTile);
    std::unique_ptr<Tile> removeTile();
    Tile* getTile();
};
