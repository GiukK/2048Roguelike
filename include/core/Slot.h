#pragma once

#include <memory>
#include "Tile.h"
#include "SlotEffect.h"

#include "SFML/Graphics.hpp"

class Board;
class Tile;

class Slot {
private:



public:

    int col;//x
    int row;//y

    bool canTileStepIn{1};
    bool canTileStepOut{ 1 };


    Board* board;

    std::unique_ptr<Tile> tile = nullptr;

    sf::Texture slotTexture;
    sf::Sprite slot;

    Slot(int col, int row, Board* board);

    Slot(const Slot& other, Board* board);



    void render(sf::RenderWindow& window);

    // In Slot.h - Change isEmpty() from a non-const to a const method:
    bool isEmpty() const;  // Note the const keyword

    void setTile(std::unique_ptr<Tile> newTile);

    void removeTile();

    std::unique_ptr<Tile> releaseTile();

    std::vector<std::unique_ptr<SlotEffect>> effects;

    void addEffect(std::unique_ptr<SlotEffect> effect);

    void triggerMergeEffects();


};
