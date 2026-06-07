#include "core/GameRun.h"
#include "rendering/RenderSystem.h"

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
            // Bomb requires exactly one tile selected as target
            if (tiles.size() != 1) return false;
            run.destroyTile(tiles[0]);
            return true;
        }
    });

    turns.push(std::make_unique<Turn>(renderer, this));
}

void GameRun::enter() {}
void GameRun::exit() {}

void GameRun::newTurn(const Board& currentBoard) {
    turns.push(std::make_unique<Turn>(renderer, this, currentBoard));
}

void GameRun::goBack() {
    if (turns.size() <= 1) return;
    turns.pop();
}

void GameRun::openShop() {
    if (shopOpen) return;
    shopOpen = true;
    if (shopCallback) {
        shopCallback(this);
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

void GameRun::drawDigitCounter(sf::RenderWindow& window, unsigned int value, float xOffset) {
    std::string text = std::to_string(value);
    float totalWidth = static_cast<float>(text.size()) * 5.f * 10.f;
    renderer.drawNumber(value, {xOffset + totalWidth / 2.f, 18.f}, 10.f);
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

void GameRun::destroyTile(Tile* tile) {
    if (!turns.empty()) {
        turns.top()->board.destroyTile(tile);
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
