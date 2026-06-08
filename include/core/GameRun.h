#pragma once

#include <memory>
#include <random>
#include <stack>
#include <vector>
#include <functional>
#include <algorithm>
#include <utility>

#include <SFML/Graphics.hpp>
#include "rendering/UI_Button.h"
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

    GameRun(RenderSystem& renderer, AnimationCallback onAnimation, ShopCallback onShopOpen);

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

    // Render in three layers so the board can sit under the movable camera while
    // the HUD stays screen-space. Callers pick the view between layers (see
    // RenderSystem::useBoardView / useUIView). render() is a convenience that
    // does all three with the right views, for callers that don't interleave
    // their own board-space drawing (e.g. ShopState).
    void renderBackground(RenderSystem& renderer);  // full-screen UI backdrop
    void renderBoard(RenderSystem& renderer);       // slots + tiles (board view)
    void renderForeground(RenderSystem& renderer);  // counters + inventory (UI view)
    void render(RenderSystem& renderer);

    // World-space center of the current board's content (for aiming the camera).
    sf::Vector2f getBoardContentCenter();

    void addCoins(int amount);
    int getCoins() const;

    // Returns the final cost of an item after applying all active modifiers.
    // This is the single hook point for passive abilities, shop discounts,
    // and any future effect that alters item prices.
    int getEffectiveCost(const ItemDef& item) const;

    bool isInventoryFull() const;
    void addItem(const std::string& itemId);

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
    void rebuildInventoryButtons();
    void rebuildActionButtons();
    void drawDigitCounter(sf::RenderWindow& window, unsigned int value, float xOffset,
                          float y = 18.f, float scale = 10.f);

    RenderSystem& renderer;
    AnimationCallback animationCallback;
    ShopCallback shopCallback;
    AnimationsActiveQuery animationsActive;

    std::mt19937 rng;
    sf::Sprite backUI;

    std::stack<std::unique_ptr<Turn>> turns;

    std::vector<std::string> inventoryItems;
    std::vector<UI_Button> inventoryButtons;

    // Selection: only one item at a time. -1 = nothing selected.
    // When selected, use/discard action buttons appear next to the item.
    int selectedIndex = -1;
    std::vector<UI_Button> actionButtons;

    // Deferred actions — processed after button update loops to avoid iterator invalidation.
    int pendingSelectIndex = -1;
    enum class PendingAction { None, Use, Discard };
    PendingAction pendingAction = PendingAction::None;

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
