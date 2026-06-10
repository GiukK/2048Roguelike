#pragma once

#include <memory>
#include <optional>
#include <random>
#include <stack>
#include <vector>
#include <functional>
#include <algorithm>
#include <utility>

#include <SFML/Graphics.hpp>
#include "rendering/Animation.h"
#include "core/Turn.h"
#include "core/ItemRegistry.h"

class RenderSystem;

class GameRun {
public:
    using AnimationCallback = std::function<void(std::unique_ptr<Animation>)>;
    using ShopCallback = std::function<void(GameRun*)>;
    using AnimationsActiveQuery = std::function<bool()>;

    // Decides the value of the phantom tile a freshly spawned shop holds.
    // Default: a copy of the board's current largest tile (see constructor).
    // Replaceable so abilities can change the shop-activation criterion later.
    using ShopTileValueStrategy = std::function<int(const Board&)>;

    // `seed` fixes the run's RNG for reproducible runs (tests, replaying a bug
    // report); nullopt = a fresh random seed. Whichever is used is kept readable
    // via getRunSeed() and printed in debug builds.
    GameRun(RenderSystem& renderer, AnimationCallback onAnimation, ShopCallback onShopOpen,
            std::optional<unsigned int> seed = std::nullopt);

    void enter();
    void exit();

    void newTurn(const Board& currentBoard);
    // Rewinds to the previous turn. Returns false if there is no earlier turn.
    // In normal play only the Hourglass item calls this; the debug B key also does.
    bool goBack();
    void openShop();

    // Drives the whole shop-spawn lifecycle for one completed turn. Called by
    // Turn::endTurn on the just-finished board, before it is cloned into the
    // next turn — so any spawn/removal it performs is inherited by the next
    // turn. Decrements the countdown, freezes it while shops are present,
    // removes consumed shops, and spawns a new shop when the countdown elapses.
    void advanceShopState(Board& board);

    // --- Modular shop tuning (safe to call at runtime, e.g. from abilities) ---
    void setShopSpawnInterval(int turns) { shopSpawnInterval = std::max(0, turns); }
    void setMaxShopsOnBoard(unsigned int count) { maxShopsOnBoard = count; }
    void setShopTileValueStrategy(ShopTileValueStrategy strategy) {
        shopTileValueStrategy = std::move(strategy);
    }
    int getShopCountdown() const { return shopCountdown; }

    void handleInput(sf::Event& event);
    void update(float deltaTime);

    // Renders the board (slots + tiles) in the board view. The HUD/inventory UI
    // lives in PlayUI now — GameRun is the game-state model, not the view.
    void renderBoard(RenderSystem& renderer);

    // World-space center / bounding box of the current board's content (for
    // aiming and snapping the camera).
    sf::Vector2f getBoardContentCenter();
    sf::FloatRect getBoardContentBounds();

    void addCoins(int amount);
    int getCoins() const;

    // Returns the final cost of an item after applying all active modifiers.
    // This is the single hook point for passive abilities, shop discounts,
    // and any future effect that alters item prices.
    int getEffectiveCost(const ItemDef& item) const;

    bool isInventoryFull() const;
    void addItem(const std::string& itemId);

    // --- Inventory queries / commands for the UI layer (PlayUI) ---
    const std::vector<std::string>& getInventoryItems() const { return inventoryItems; }
    int getSelectedIndex() const { return selectedIndex; }
    size_t getTurnCount() const { return turns.size(); }

    // Event log of the current (top) turn — the read side of the per-turn event
    // substrate. Score (Step 2) and reactive abilities (Step 3) consume this.
    const TurnLog& currentTurnLog() const;
    // Toggle which inventory item is held/selected (clicking the same one clears).
    void toggleSelectedItem(int index);

    // Held item — the single item the player is actively holding from inventory.
    // Game effects can query this to interact with it without going through UI
    // (e.g. "trigger held item for free", "copy held item", "double effect").
    bool hasHeldItem() const;
    const std::string& getHeldItemId() const;
    const ItemDef* getHeldItemDef() const;
    void useHeldItem();
    void discardHeldItem();

    // Board tile selection — delegated to the current turn's board.
    // Item effects use these to interact with targeted tiles.
    std::vector<Tile*> getSelectedTiles() const;
    void destroyTile(Tile* tile);
    void swapTiles(Tile* a, Tile* b);
    void clearBoardSelection();

    // Occupied tiles around `center` on the current board (delegates to Board).
    // Used by the area-effect items (Bomb II / Bomb III). Shops are excluded.
    std::vector<Tile*> getTilesInRadius(Tile* center, int radius, bool includeCenter) const;

    // Board-wide item effects, delegated to the current turn's board.
    void destroyAllTiles();        // Black Hole
    void spawnTile();              // seed a tile (e.g. after Black Hole empties the board)
    int  shuffleTiles();           // Die — returns the number of tiles shuffled
    bool addRandomSlot();          // Mount — adds a base slot; false if none possible
    bool removeSlotUnder(Tile* t); // Wrench — removes tile + slot; false on shop/null

    int getRandomInt(int min, int max);
    unsigned int getRunSeed() const { return runSeed; }
    std::vector<const ItemDef*> pickShopItems(int count);

    const AnimationCallback& getAnimationCallback() const { return animationCallback; }
    void setAnimationsActiveQuery(AnimationsActiveQuery query) { animationsActive = std::move(query); }
    bool hasActiveAnimations() const { return animationsActive && animationsActive(); }

    ItemRegistry& getItemRegistry() { return itemRegistry; }

    bool shopOpen = false;

private:
    void useItem(size_t index);
    void discardItem(size_t index);
    void clearSelection();

    RenderSystem& renderer;
    AnimationCallback animationCallback;
    ShopCallback shopCallback;
    AnimationsActiveQuery animationsActive;

    std::mt19937 rng;
    unsigned int runSeed = 0;  // the seed rng was actually seeded with

    std::stack<std::unique_ptr<Turn>> turns;

    std::vector<std::string> inventoryItems;

    // Selection: only one item at a time. -1 = nothing selected. This is the
    // "held item" the player is interacting with; PlayUI shows its action buttons.
    int selectedIndex = -1;

    ItemRegistry itemRegistry;

    int coins = 100;
    unsigned int maxInventorySize = 3;

    // --- Shop spawn state (all tunable; see the setters above) ---
    // shopSpawnInterval: turns the countdown starts from (the "10").
    // shopCountdown:     turns left before the next shop spawns; frozen at 0
    //                    while the board already holds the allowed shops.
    // maxShopsOnBoard:   how many shops may coexist; while this many are
    //                    present the countdown stays at 0 (no immediate respawn).
    int shopSpawnInterval = 10;
    int shopCountdown = 10; // mirrors shopSpawnInterval at construction
    unsigned int maxShopsOnBoard = 1;
    ShopTileValueStrategy shopTileValueStrategy;
};
