#include "core/GameRun.h"
#include "rendering/RenderSystem.h"
#include "Debug.h"

#include <algorithm>

GameRun::GameRun(RenderSystem& renderer, AnimationCallback onAnimation, ShopCallback onShopOpen)
    : renderer(renderer),
      animationCallback(std::move(onAnimation)),
      shopCallback(std::move(onShopOpen)),
      backUI(renderer.getTextureManager().get("backUI"))
{
    renderer.resizeSprite("backUI", backUI);

    std::random_device rd;
    rng.seed(rd());

    itemRegistry.registerItem({"coin_bag", "coin_bag", 10, 1.0f,
        [](GameRun& run) -> bool { run.addCoins(50); return true; }
    });

    itemRegistry.registerItem({"bomb", "bomb", 50, 0.5f,
        [](GameRun& run) -> bool {
            auto tiles = run.getSelectedTiles();
            if (tiles.size() != 1) return false;
            run.destroyTile(tiles[0]);
            return true;
        }
    });

    itemRegistry.registerItem({"switch", "switch", 50, 0.25f,
        [](GameRun& run) -> bool {
            auto tiles = run.getSelectedTiles();
            if (tiles.size() != 2) return false;
            run.swapTiles(tiles[0], tiles[1]);
            return true;
        }
    });

    // Bomb II: anchored on the selected tile, destroys up to two RANDOM tiles
    // among its 8 neighbours (diagonals included). The anchor itself is left
    // intact — the payload is purely the surrounding tiles. Refuses (does not
    // consume) if nothing surrounds the anchor.
    itemRegistry.registerItem({"bomb_2", "bomb_2", 75, 0.4f,
        [](GameRun& run) -> bool {
            auto sel = run.getSelectedTiles();
            if (sel.size() != 1) return false;

            auto neighbours = run.getTilesInRadius(sel[0], 1, /*includeCenter=*/false);
            if (neighbours.empty()) return false;

            int hits = std::min<int>(2, static_cast<int>(neighbours.size()));
            for (int i = 0; i < hits; ++i) {
                int idx = run.getRandomInt(0, static_cast<int>(neighbours.size()) - 1);
                run.destroyTile(neighbours[idx]);
                neighbours.erase(neighbours.begin() + idx);
            }
            return true;
        }
    });

    // Bomb III: destroys every tile in the full 3x3 block centred on the selected
    // tile (the anchor included).
    itemRegistry.registerItem({"bomb_3", "bomb_3", 120, 0.2f,
        [](GameRun& run) -> bool {
            auto sel = run.getSelectedTiles();
            if (sel.size() != 1) return false;

            for (Tile* target : run.getTilesInRadius(sel[0], 1, /*includeCenter=*/true)) {
                run.destroyTile(target);
            }
            return true;
        }
    });

    // Brick: locks the selected tile in place (immovable, like a shop slot) until
    // a merge clears it. Refuses if the tile is already bricked.
    itemRegistry.registerItem({"brick", "brick", 60, 0.3f,
        [](GameRun& run) -> bool {
            auto sel = run.getSelectedTiles();
            if (sel.size() != 1) return false;
            if (sel[0]->isBricked()) return false;

            sel[0]->setBricked(true);
            return true;
        }
    });

    // Black Hole: wipes every (non-shop) tile, then seeds one fresh tile so the
    // board stays playable — an empty board has no legal move and would soft-lock.
    itemRegistry.registerItem({"black_hole", "black_hole", 150, 0.15f,
        [](GameRun& run) -> bool {
            run.destroyAllTiles();
            run.spawnTile();
            return true;
        }
    });

    // Die: uniformly reshuffles the tiles among their occupied cells (the filled
    // cells stay the same; only which tile sits where changes). Needs >= 2 tiles.
    itemRegistry.registerItem({"die", "die", 40, 0.4f,
        [](GameRun& run) -> bool {
            return run.shuffleTiles() >= 2;
        }
    });

    // Hourglass: rewinds one turn. In normal play this is the ONLY way to go back
    // (the B key is debug-only); refuses if there is no earlier turn to return to.
    itemRegistry.registerItem({"hourglass", "hourglass", 80, 0.25f,
        [](GameRun& run) -> bool {
            return run.goBack();
        }
    });

    // Default shop-tile criterion: a copy of the board's current largest tile,
    // so activating the shop costs the player a rebuilt copy of their best tile.
    // Clamped to [2, 1024] so the activating merge (2x value) never exceeds the
    // highest tile artwork (2048). Swap via setShopTileValueStrategy().
    shopTileValueStrategy = [](const Board& board) {
        return std::clamp(board.getMaxTileValue(), 2, 1024);
    };
    shopCountdown = shopSpawnInterval;

    turns.push(std::make_unique<Turn>(renderer, this));
}

void GameRun::enter() {}
void GameRun::exit() {}

void GameRun::newTurn(const Board& currentBoard) {
    turns.push(std::make_unique<Turn>(renderer, this, currentBoard));
}

bool GameRun::goBack() {
    if (turns.size() <= 1) return false;
    turns.pop();
    return true;
}

void GameRun::openShop() {
    if (shopOpen) return;
    shopOpen = true;
    if (shopCallback) {
        shopCallback(this);
    }
}

void GameRun::advanceShopState(Board& board) {
    // Order matters. We mutate `board` (the turn that just ended, already past
    // its move + tile spawn) so the result is baked into the clone the next turn
    // is built from. See Turn::endTurn for the call site.
    const int maxShops = static_cast<int>(maxShopsOnBoard);

    // 1. A shop merged during this turn disappears from the next board.
    int consumed = board.removeConsumedShops();

    // 2. Count shops still awaiting a merge after that removal.
    int activeShops = board.countActiveShops();

    // 3. Advance / freeze / restart the countdown.
    if (consumed > 0) {
        // A shop just vanished -> restart the countdown from the interval.
        shopCountdown = shopSpawnInterval;
    } else if (activeShops >= maxShops) {
        // Board already holds the allowed number of shops -> freeze at 0 so it
        // does not immediately respawn (and the displayed counter sticks at 0).
        shopCountdown = 0;
    } else if (shopCountdown > 0) {
        --shopCountdown;
    }

    // 4. Spawn once the countdown elapses and there is still room for a shop.
    if (shopCountdown == 0 && activeShops < maxShops) {
        int tileValue = shopTileValueStrategy ? shopTileValueStrategy(board)
                                              : std::max(2, board.getMaxTileValue());
        board.spawnShop(tileValue);
    }
}

void GameRun::handleInput(sf::Event& event) {
    if (!turns.empty()) {
        turns.top()->handleInput(event);
    }
}

void GameRun::update(float deltaTime) {
    turns.top()->update(deltaTime);

    for (auto& btn : inventoryButtons) {
        btn.update(deltaTime);
    }

    for (auto& btn : actionButtons) {
        btn.update(deltaTime);
    }

    // Process deferred selection toggle
    if (pendingSelectIndex >= 0) {
        if (selectedIndex == pendingSelectIndex)
            selectedIndex = -1;
        else
            selectedIndex = pendingSelectIndex;
        pendingSelectIndex = -1;
        rebuildActionButtons();
    }

    // Process deferred use/discard
    if (pendingAction != PendingAction::None) {
        if (selectedIndex >= 0 && selectedIndex < static_cast<int>(inventoryItems.size())) {
            if (pendingAction == PendingAction::Use)
                useItem(static_cast<size_t>(selectedIndex));
            else
                discardItem(static_cast<size_t>(selectedIndex));
        }
        pendingAction = PendingAction::None;
    }
}

void GameRun::render(RenderSystem& renderer) {
    renderer.draw(backUI);
    turns.top()->render(renderer);

    drawDigitCounter(renderer.getWindow(), static_cast<unsigned int>(turns.size()), 350.f);
    drawDigitCounter(renderer.getWindow(), static_cast<unsigned int>(coins), 1380.f);

    // Shop spawn countdown: stacked just below the turn counter (top-left),
    // smaller so it reads as a sub-counter and stays clear of the board and the
    // score readouts. Shows 0 while a shop is on the board; restarts at the
    // interval once the shop is consumed.
    drawDigitCounter(renderer.getWindow(), static_cast<unsigned int>(shopCountdown),
                     350.f, 100.f, 7.f);

    for (size_t i = 0; i < inventoryButtons.size(); ++i) {
        auto& btn = inventoryButtons[i];
        bool held = (static_cast<int>(i) == selectedIndex);

        if (held) btn.getSprite().setColor(sf::Color::Red);
        renderer.draw(btn.getSprite());
        if (held) btn.getSprite().setColor(sf::Color::White);
    }

    for (auto& btn : actionButtons) {
        renderer.draw(btn.getSprite());
    }
}

void GameRun::drawDigitCounter(sf::RenderWindow& window, unsigned int value, float xOffset,
                               float y, float scale) {
    std::string text = std::to_string(value);
    float totalWidth = static_cast<float>(text.size()) * 5.f * scale;
    renderer.drawNumber(value, {xOffset + totalWidth / 2.f, y}, scale);
}

int GameRun::getRandomInt(int min, int max) {
    std::uniform_int_distribution<int> dist(min, max);
    return dist(rng);
}

std::vector<const ItemDef*> GameRun::pickShopItems(int count) {
    return itemRegistry.pickMultiple(count, rng);
}

void GameRun::addCoins(int amount) {
    coins += amount;
}

int GameRun::getCoins() const {
    return coins;
}

int GameRun::getEffectiveCost(const ItemDef& item) const {
    // Debug: everything is free so items can be grabbed and tested instantly.
    if (debug::Enabled) return 0;

    int cost = item.cost;
    // Future: apply passive modifiers, shop discount abilities, etc.
    return std::max(cost, 0);
}

bool GameRun::isInventoryFull() const {
    return inventoryItems.size() >= maxInventorySize;
}

void GameRun::addItem(const std::string& itemId) {
    if (isInventoryFull()) return;
    if (!itemRegistry.has(itemId)) return;

    inventoryItems.push_back(itemId);
    clearSelection();
    rebuildInventoryButtons();
}

void GameRun::useItem(size_t index) {
    if (index >= inventoryItems.size()) return;

    const auto& def = itemRegistry.get(inventoryItems[index]);
    if (def.onUse) {
        bool consumed = def.onUse(*this);
        if (!consumed) return;
    }

    inventoryItems.erase(inventoryItems.begin() + static_cast<ptrdiff_t>(index));
    clearSelection();
    // A consumed item finishes its interaction with the board, so any tiles it had
    // targeted should no longer stay selected (e.g. SWITCH leaving both tiles red).
    clearBoardSelection();
    rebuildInventoryButtons();
}

void GameRun::discardItem(size_t index) {
    if (index >= inventoryItems.size()) return;

    inventoryItems.erase(inventoryItems.begin() + static_cast<ptrdiff_t>(index));
    clearSelection();
    rebuildInventoryButtons();
}

bool GameRun::hasHeldItem() const {
    return selectedIndex >= 0 && selectedIndex < static_cast<int>(inventoryItems.size());
}

const std::string& GameRun::getHeldItemId() const {
    return inventoryItems[static_cast<size_t>(selectedIndex)];
}

const ItemDef* GameRun::getHeldItemDef() const {
    if (!hasHeldItem()) return nullptr;
    return &itemRegistry.get(inventoryItems[static_cast<size_t>(selectedIndex)]);
}

void GameRun::useHeldItem() {
    if (hasHeldItem()) useItem(static_cast<size_t>(selectedIndex));
}

void GameRun::discardHeldItem() {
    if (hasHeldItem()) discardItem(static_cast<size_t>(selectedIndex));
}

std::vector<Tile*> GameRun::getSelectedTiles() const {
    if (turns.empty()) return {};
    return turns.top()->board.getSelectedTiles();
}

std::vector<Tile*> GameRun::getTilesInRadius(Tile* center, int radius, bool includeCenter) const {
    if (turns.empty()) return {};
    return turns.top()->board.getTilesInRadius(center, radius, includeCenter);
}

void GameRun::destroyAllTiles() {
    if (!turns.empty()) turns.top()->board.destroyAllTiles();
}

void GameRun::spawnTile() {
    if (!turns.empty()) turns.top()->board.spawnTileInRandomEmptySlot();
}

int GameRun::shuffleTiles() {
    if (turns.empty()) return 0;
    return turns.top()->board.shuffleTiles();
}

void GameRun::destroyTile(Tile* tile) {
    if (!turns.empty()) {
        turns.top()->board.destroyTile(tile);
    }
}

void GameRun::swapTiles(Tile* a, Tile* b) {
    if (!turns.empty()) {
        turns.top()->board.swapTiles(a, b);
    }
}

void GameRun::clearBoardSelection() {
    if (!turns.empty()) {
        turns.top()->board.clearSelection();
    }
}

void GameRun::clearSelection() {
    selectedIndex = -1;
    actionButtons.clear();
}

void GameRun::rebuildInventoryButtons() {
    inventoryButtons.clear();
    for (size_t i = 0; i < inventoryItems.size(); ++i) {
        const auto& def = itemRegistry.get(inventoryItems[i]);
        size_t idx = i;
        inventoryButtons.emplace_back(renderer, def.textureId,
            sf::Vector2f{1500.f, 350.f + 200.f * static_cast<float>(i)},
            [this, idx]() { pendingSelectIndex = static_cast<int>(idx); });
    }
}

void GameRun::rebuildActionButtons() {
    actionButtons.clear();
    if (selectedIndex < 0 || selectedIndex >= static_cast<int>(inventoryItems.size()))
        return;

    float itemY = 350.f + 200.f * static_cast<float>(selectedIndex);
    float buttonX = 1350.f;

    actionButtons.emplace_back(renderer, "use_button",
        sf::Vector2f{buttonX, itemY - 55.f},
        [this]() { pendingAction = PendingAction::Use; });

    actionButtons.emplace_back(renderer, "discard_button",
        sf::Vector2f{buttonX, itemY + 55.f},
        [this]() { pendingAction = PendingAction::Discard; });
}
