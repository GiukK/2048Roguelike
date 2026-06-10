#pragma once

#include <memory>
#include <vector>
#include "core/Tile.h"
#include "core/utils/Coord.h"
#include "effects/Effect.h"

#include <SFML/Graphics.hpp>

class RenderSystem;
class Board;
struct MergeContext;

class Slot {
public:
    Slot(int col, int row, Board* board, RenderSystem& renderer);
    Slot(const Slot& other, Board* newBoard);

    Coord getCoord() const;
    sf::Sprite& getSlotSprite();

    bool isEmpty() const;
    void setTile(std::unique_ptr<Tile> newTile);
    void removeTile();
    std::unique_ptr<Tile> releaseTile();

    void addEffect(std::unique_ptr<Effect> effect);

    // Runs this slot's effects over a resolving merge, in order, so each may modify
    // the outcome (MergeContext) before it's applied and logged. This is the slot-
    // scope leg of the merge dispatch; tile/board/run scopes join as they land (see
    // docs/effect-engine-design.md §6 for the cross-scope order).
    void resolveMerge(MergeContext& merge);

    void update(float deltaTime);
    void render(RenderSystem& renderer);

    int col;
    int row;
    bool canTileStepIn = true;
    bool canTileStepOut = true;
    Board* board;
    std::unique_ptr<Tile> tile;
    std::vector<std::unique_ptr<Effect>> effects;

private:
    void initVisuals();

    RenderSystem& renderer;
    sf::Sprite sprite;
};
