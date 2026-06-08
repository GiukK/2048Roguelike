#pragma once

#include <map>
#include <memory>
#include <functional>

#include "core/utils/Coord.h"
#include "core/utils/Direction.h"
#include "core/utils/MovementQueue.h"
#include "core/Slot.h"

#include <SFML/Graphics.hpp>

class Turn;
class RenderSystem;
class Animation;

class Board {
public:
    using AnimationCallback = std::function<void(std::unique_ptr<Animation>)>;

    Board(RenderSystem& renderer, Turn* turn, bool doInitialSetup = true);

    static Board cloneFrom(const Board& other, Turn* turn);
    void copyStateFrom(const Board& other);

    // cloneFrom returns by value, so the slots (which hold a back-pointer to
    // their owning Board) must be re-parented to the destination after the
    // move; otherwise Slot::board dangles to the moved-from temporary.
    Board(Board&& other) noexcept;

    void handleInput(sf::Event& event);
    void render(RenderSystem& renderer);
    void update(float deltaTime);

    void spawnTileInRandomEmptySlot();
    void move(Direction dir);
    void clear();

    // --- Shop mechanics ---------------------------------------------------
    // A "shop" is a Slot carrying a ShopEffect. It is spawned outside the base
    // playfield (orthogonally adjacent to an existing slot, on an otherwise
    // empty border cell) holding a phantom tile the player must merge into to
    // activate the shop. These methods are the spatial primitives; the *policy*
    // (when to spawn, what tile value to use, how many shops are allowed) lives
    // in GameRun so it stays swappable for future abilities.

    // Largest tile value currently on the board (0 if the board holds no tiles).
    // Used by the default shop-tile-value strategy in GameRun.
    int getMaxTileValue() const;

    // Spawns a shop on a uniformly-random valid border cell, holding a phantom
    // tile of `tileValue`. Returns false if no valid cell exists.
    bool spawnShop(int tileValue);

    // Number of shops still waiting to be merged (their ShopEffect has not
    // triggered yet). Drives the spawn countdown / freeze logic in GameRun.
    int countActiveShops() const;

    // Erases every slot whose shop has already been triggered (merged into).
    // Called between turns so a used shop disappears from the next board.
    // Returns how many were removed. See ShopEffect::isTriggered.
    int removeConsumedShops();

    bool allAnimationsFinished() const;
    bool moveWasValid() const;

    // World-space center of the bounding box of all slots. Used to point the
    // board camera at the board's content (which can grow/shift via Mount/Wrench).
    sf::Vector2f getContentCenter();

    void setAnimationCallback(AnimationCallback callback);

    // Tile selection — managed by Board input, queried by item effects.
    // Items like bomb check getSelectedTiles().size() to validate targeting.
    std::vector<Tile*> getSelectedTiles() const;
    void clearSelection();
    void destroyTile(Tile* tile);
    void swapTiles(Tile* a, Tile* b);

    // Occupied tiles inside the square of Chebyshev radius `radius` around
    // `center` (radius 1 = the 3x3 block; diagonals included). Shop tiles are
    // never returned — they are protected from area effects like the bombs.
    // `includeCenter` toggles whether `center`'s own tile is part of the result.
    std::vector<Tile*> getTilesInRadius(const Tile* center, int radius,
                                        bool includeCenter) const;

    // Destroys every non-shop tile on the board (Black Hole item).
    void destroyAllTiles();

    // Uniformly permutes the tiles among their currently occupied cells: the set
    // of filled cells is unchanged, only which tile sits where. Shops excluded.
    // Returns the number of tiles shuffled (Die item).
    int shuffleTiles();

    // Adds a new base (empty, unrestricted) slot at a uniformly-random cell that
    // currently has no slot but is orthogonally adjacent to one (a border cell or
    // an interior hole). Returns false if no such cell exists (Mount item).
    bool addRandomSlot();

    // Removes the slot under `tile` entirely — tile and slot both — leaving a
    // hole that blocks movement through it. Refuses shop slots. Returns false if
    // the tile is null or sits on a shop (Wrench item).
    bool removeSlotUnder(Tile* tile);

    Turn* turn;

private:
    void setupInitialBoard();
    void initVisuals();

    // Collects every cell that holds NO slot but is orthogonally adjacent to an
    // existing slot — i.e. border cells and interior holes. Each cell appears at
    // most once even when several slots border it (deduplicated via std::set).
    // Shared by shop spawning and the Mount item as the set of expansion targets.
    std::vector<Coord> getEmptyAdjacentCells() const;

    void initializeMovementQueue(Direction dir);
    void resolveNextTileMove(Direction dir);
    Coord getNextCoord(Coord from, Direction dir);

    // interaction
    void updateHoverState();
    void handleClick(sf::Vector2f worldPos);
    Tile* findTileAt(sf::Vector2f pos) const;

    int getRandomInt(int min, int max);

    RenderSystem& renderer;
    sf::Sprite boardSprite;

    std::map<Coord, std::unique_ptr<Slot>> slots;
    MovementQueue movementQueue;

    bool moveValidFlag = false;

    AnimationCallback animationCallback;

    Tile* hoveredTile = nullptr;
};
