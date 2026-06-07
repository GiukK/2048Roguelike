#pragma once

#include <memory>
#include <random>
#include <stack>
#include <vector>
#include <functional>

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

    GameRun(RenderSystem& renderer, AnimationCallback onAnimation, ShopCallback onShopOpen);

    void enter();
    void exit();

    void newTurn(const Board& currentBoard);
    void goBack();
    void openShop();

    void handleInput(sf::Event& event);
    void update(float deltaTime);
    void render(RenderSystem& renderer);

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
    void clearBoardSelection();

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
    void drawDigitCounter(sf::RenderWindow& window, unsigned int value, float xOffset);

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
};
