#pragma once

#include <memory>
#include "Tile.h"
#include "effects/SlotEffect.h"
#include "utils/Coord.h"

#include "SFML/Graphics.hpp"

class RenderSystem;
class Board;
class Tile;

class Slot {
public:
    //------
    Slot(int col, int row, Board* board, RenderSystem& renderer);

    //for board copying to next turn
    Slot(const Slot& other, Board* board );
    //------


    Coord getCoord() const;
    int col;//x
    int row;//y

    sf::Sprite& getSlotSprite();

    //------

    //movement flag
    bool canTileStepIn{1};
    bool canTileStepOut{ 1 };

    //raw ptr to owner - Board
    Board* board;

    //single unique ptr to -> Tile
    std::unique_ptr<Tile> tile = nullptr;

    //rendering utils
    void render(RenderSystem& renderer);

    //--------------
    //is there a tile?
    bool isEmpty() const;  // const
    //hard sets tile 
    void setTile(std::unique_ptr<Tile> newTile);
    //hard remove tile
    void removeTile();
    //soft releasing
    std::unique_ptr<Tile> releaseTile();

    //------
    //vector of effects (concept to be expanded)
    std::vector<std::unique_ptr<SlotEffect>> effects;
    //adds effect to vector
    void addEffect(std::unique_ptr<SlotEffect> effect);
    //triggers optional merge effect FOR EACH EFFECT in vector
    void triggerMergeEffects();

private:

    RenderSystem& renderer;
    sf::Sprite slot;

    void fixVisualAssets();

};
