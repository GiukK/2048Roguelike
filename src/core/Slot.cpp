#include "core/Slot.h"

bool Slot::isEmpty() const {
    return tile == nullptr;
}

void Slot::setTile(std::unique_ptr<Tile> newTile) {
    tile = std::move(newTile);
}

std::unique_ptr<Tile> Slot::removeTile() {
    return std::move(tile);
}

Tile* Slot::getTile() {
    return tile.get();
}
