#include "core/Slot.h"



Slot::Slot (int col, int row) :
    col(col),
    row(row),
    slotTexture("assets/textures/slot.png", false, sf::IntRect({ 0, 0 }, { 32, 32 })),
    slot(slotTexture)
{ 

    slot.setOrigin({ 16.f,16.f });
    slot.setScale({ 4.f, 4.f });


    float shift = 300.f;
    slot.setPosition({ 128.f * col + shift, 128.f * row + shift });
}

void Slot::render(sf::RenderWindow& window) {
    window.draw(slot);
    if (!isEmpty()) {
        tile->render(window);
    }
}

bool Slot::isEmpty() const {
    return tile == nullptr;
}

void Slot::setTile(std::unique_ptr<Tile> newTile) {
    tile = std::move(newTile);  // Takes ownership using move semantics
}

std::unique_ptr<Tile> Slot::releaseTile() {
    auto t = std::move(tile);
    removeTile();
    return t;
}


void Slot::removeTile() {
    tile.reset();  // Properly destroys the tile
}