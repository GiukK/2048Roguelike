#pragma once

#include <map>
#include <memory>
#include <functional>

#include "core/utils/Coord.h"
#include "core/utils/Direction.h"
#include "core/utils/MovementQueue.h"
#include "core/Slot.h"
// Complete type required (not just forward-declared): the unique_ptr<Boss>
// member means every TU that destroys a Board must see Boss's destructor.
#include "core/Boss.h"

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

    // Deterministic spawn primitive: places a tile of `value` on the slot at `c`
    // if that slot exists and is empty; returns the tile, else nullptr. The
    // random spawn delegates here; tests and future effects ("spawn a 2 in the
    // corner") use it directly. Emits the TileSpawned event like any spawn.
    // Refuses values without artwork (Tile::isValidValue) the same way — so no
    // effect can ever crash the texture lookup by inventing a tile value.
    Tile* spawnTileAt(Coord c, int value);

    // Every tile currently on the board (shops included), in coord order.
    // Read-only iteration primitive for effects, debug tooling and the tests.
    std::vector<Tile*> getAllTiles() const;

    // The slot at `c`, or nullptr if no slot exists there. Lookup primitive for
    // effects acting on logged coordinates (e.g. a card rewarding the cell a
    // merge happened on).
    Slot* slotAt(Coord c) const;

    // Every effect mounted on a board resident, in the pinned board-scope
    // dispatch order: all tile effects (coord order), then all slot effects
    // (coord order) — the reactor-pass slice of the cross-scope rule
    // (tile → slot → board-owned → run, effect-engine doc §6). Effects on
    // EMPTY slots are included: a chip keeps reacting whether or not a tile
    // sits on it. Snapshot primitive for GameRun's dispatch legs.
    std::vector<Effect*> collectBoardEffects() const;

    // The slot a (possibly stale) effect pointer currently lives on — its own
    // slot for slot effects, the carrying tile's CURRENT slot for tile effects
    // — or nullptr if it is gone. The owner-lifetime re-validation primitive
    // (boss-design §9.2): a dispatch snapshot can outlive an owner destroyed
    // mid-pass, so the dispatcher re-asks before every hook call.
    Slot* findOwnerSlot(const Effect* effect) const;

    void move(Direction dir);
    void clear();

    // The defeat predicate (docs/boss-design.md §8): does ANY tile have a
    // slide, a merge, or a boss attack available in some direction? Pure const
    // — it mirrors resolveNextTileMove's decision logic (immobilized tiles,
    // step-in/step-out locks, holes, occupancy, the value cap) without
    // mutating anything. Items are deliberately NOT consulted: a full board
    // with a bomb in the inventory is still a loss. Attacking the boss IS a
    // legal move (exactly when it answers Hit) — feeding it tiles is the
    // escape valve on a packed board.
    bool hasLegalMove() const;

    // --- Boss occupancy (boss-design §2/§3) ---------------------------------
    // One boss at a time (the ante's final objective). The body lives on the
    // board, so it clones with the turn: HP rewinds with the world (§7).

    Boss* getBoss() const { return boss.get(); }
    bool isBossAt(Coord c) const;

    // Places a boss with its anchor on `c`: requires an existing, tile-empty,
    // unprotected slot and no boss already present — returns nullptr otherwise
    // (the spawn-primitive contract, like spawnTileAt). Emits BossSpawned.
    Boss* spawnBoss(const BossDef& def, Coord c);

    // Random-placement convenience for the debug key today, the ante machine
    // (slice 4) tomorrow. Returns nullptr if no valid cell exists.
    Boss* spawnBossInRandomEmptySlot(const BossDef& def);

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

    // World-space bounding box of all slot positions (slot centers). Recomputed
    // from the live slots, so it tracks Mount/Wrench growth and holes. Used to
    // aim and to snap the board camera.
    sf::FloatRect getContentBounds();

    // World-space center of that bounding box (camera aim point).
    sf::Vector2f getContentCenter();

    void setAnimationCallback(AnimationCallback callback);

    // Tile selection — managed by Board input, queried by item effects.
    // Items like bomb check getSelectedTiles().size() to validate targeting.
    std::vector<Tile*> getSelectedTiles() const;
    void clearSelection();
    void destroyTile(Tile* tile);
    // Refuses protected slots (the shop) by default, like every other board
    // manipulation. allowProtected is the deliberate escape hatch for effects
    // that move a shop's phantom tile as an explicit mechanic — the Switch
    // item is the first; future effects opt in per-case.
    void swapTiles(Tile* a, Tile* b, bool allowProtected = false);

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

    // Collects every cell that holds NO slot but is orthogonally adjacent to an
    // existing slot — i.e. border cells and interior holes. Each cell appears at
    // most once even when several slots border it (deduplicated via std::set).
    // Shared by shop spawning and the Mount item as the set of expansion targets.
    std::vector<Coord> getEmptyAdjacentCells() const;

    void initializeMovementQueue(Direction dir);
    void resolveNextTileMove(Direction dir);

    // The attack interaction (boss-design §3): asks the boss how the incoming
    // tile resolves, threads a Hit through the AttackContext modifier pipeline
    // (attacker's tile effects + run cards), applies the final outcome, logs
    // BossDamaged, and resolves the defeat (BossDefeated + onDefeat + removal)
    // when HP runs out. Called by resolveNextTileMove at the interaction site
    // — the sweep stays atomic; reactors see the events at the next safe point.
    void resolveBossAttack(Tile* attacker, Coord at);
    void resolveBossDefeat();
    // Pure coordinate math (static): shared by the movement sweep and the
    // const hasLegalMove predicate.
    static Coord getNextCoord(Coord from, Direction dir);

    // interaction
    void updateHoverState();
    void handleClick(sf::Vector2f worldPos);
    Tile* findTileAt(sf::Vector2f pos) const;

    int getRandomInt(int min, int max);

    RenderSystem& renderer;

    std::map<Coord, std::unique_ptr<Slot>> slots;
    // The boss body (nullptr = no fight). unique_ptr so the entity itself
    // stays headless and value-clonable (copyStateFrom deep-copies it).
    std::unique_ptr<Boss> boss;
    MovementQueue movementQueue;

    bool moveValidFlag = false;

    AnimationCallback animationCallback;

    Tile* hoveredTile = nullptr;
};
