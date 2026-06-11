#include "core/GameRun.h"
#include "effects/Cards.h"
#include "effects/CoinContext.h"
#include "effects/EffectContext.h"
#include "rendering/RenderSystem.h"
#include "Debug.h"

#include <algorithm>
#include <iostream>

GameRun::GameRun(RenderSystem& renderer, AnimationCallback onAnimation, ShopCallback onShopOpen,
                 std::optional<unsigned int> seed)
    : renderer(renderer),
      animationCallback(std::move(onAnimation)),
      shopCallback(std::move(onShopOpen))
{
    // One seed per run, kept readable: a logged seed can be passed back in to
    // replay the exact run (the invariant tests rely on this determinism).
    runSeed = seed ? *seed : std::random_device{}();
    rng.seed(runSeed);
    if (debug::Enabled) std::cout << "[run] rng seed = " << runSeed << '\n';

    itemRegistry.registerItem({"coin_bag", "coin_bag",
        "Coin Bag", "Gives 50 coins.", 10, 1.0f,
        [](GameRun& run) -> bool { run.addCoins(50); return true; }
    });

    itemRegistry.registerItem({"bomb", "bomb",
        "Bomb", "Destroys the selected tile.", 50, 0.5f,
        [](GameRun& run) -> bool {
            auto tiles = run.getSelectedTiles();
            if (tiles.size() != 1) return false;
            run.destroyTile(tiles[0]);
            return true;
        }
    });

    itemRegistry.registerItem({"switch", "switch",
        "Switch", "Swaps the two selected tiles.", 50, 0.25f,
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
    itemRegistry.registerItem({"bomb_2", "bomb_2",
        "Bomb II", "Destroys up to two random tiles next to the selected one.", 75, 0.4f,
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
    itemRegistry.registerItem({"bomb_3", "bomb_3",
        "Bomb III", "Destroys every tile in the 3x3 block around the selected tile.", 120, 0.2f,
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
    itemRegistry.registerItem({"brick", "brick",
        "Brick", "Locks the selected tile in place until a merge frees it.", 60, 0.3f,
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
    itemRegistry.registerItem({"black_hole", "black_hole",
        "Black Hole", "Destroys every tile on the board, then spawns a new one.", 150, 0.15f,
        [](GameRun& run) -> bool {
            run.destroyAllTiles();
            run.spawnTile();
            return true;
        }
    });

    // Die: uniformly reshuffles the tiles among their occupied cells (the filled
    // cells stay the same; only which tile sits where changes). Needs >= 2 tiles.
    itemRegistry.registerItem({"die", "die",
        "Die", "Randomly reshuffles all tiles among their cells.", 40, 0.4f,
        [](GameRun& run) -> bool {
            return run.shuffleTiles() >= 2;
        }
    });

    // Hourglass: rewinds the game. In normal play this is the ONLY way to go back
    // (the B key is debug-only). The depth is card-modifiable (Back to Back):
    // rewind up to getRewindDepth() turns, stopping at the first turn; consumed
    // if at least one rewind happened, kept otherwise (nothing to rewind).
    itemRegistry.registerItem({"hourglass", "hourglass",
        "Hourglass", "Rewinds the game by one turn.", 80, 0.25f,
        [](GameRun& run) -> bool {
            const int depth = run.getRewindDepth();
            bool rewound = false;
            for (int i = 0; i < depth && run.goBack(); ++i) {
                rewound = true;
            }
            return rewound;
        }
    });

    // Mount: adds a base empty slot at a uniformly-random cell adjacent to the
    // board (a border cell or an interior hole), expanding the playfield.
    itemRegistry.registerItem({"mount", "mount",
        "Mount", "Adds a new empty slot at the edge of the board.", 70, 0.3f,
        [](GameRun& run) -> bool {
            return run.addRandomSlot();
        }
    });

    // Wrench: removes the selected tile AND the slot beneath it, punching a hole
    // in the board. Cannot target the shop.
    itemRegistry.registerItem({"wrench", "wrench",
        "Wrench", "Removes the selected tile and its slot, leaving a hole.", 50, 0.35f,
        [](GameRun& run) -> bool {
            auto sel = run.getSelectedTiles();
            if (sel.size() != 1) return false;
            return run.removeSlotUnder(sel[0]);
        }
    });

    // --- Card catalogue (data + lambdas, like the items above) --------------

    // Two for Two: the first real card. Reward sourced AT the merge cell, so a
    // future coin chip mounted on that slot scales it (card × chip composition).
    // valueB is the pre-merge source value: "two 2s" reads the tiles as they
    // were, regardless of what merge modifiers do to the result.
    cardRegistry.registerCard({"two_for_two", "two_for_two",
        "Two for Two", "Every time two 2 tiles merge, gain 2 coins.", 0, 1.0f,
        []() -> std::unique_ptr<Effect> {
            return std::make_unique<ReactorCard>(
                [](const TurnEvent& e, EffectContext& ctx) {
                    if (e.type == TurnEvent::Type::TileMerged && e.valueB == 2) {
                        ctx.addCoins(2, ctx.board().slotAt(e.coord));
                    }
                });
        }
    });

    // Economic Boom: reacts to bomb use. Plain id check FOR NOW — the intended
    // future is an item TAG system ("bomb" family covering Bomb II/III, plus
    // "X counts as a bomb" modifier effects), which needs ItemDef tags + a
    // tag-query hook on the run. Deliberately not built for one card.
    cardRegistry.registerCard({"economic_boom", "economic_boom",
        "Economic Boom", "Every time you use a Bomb, gain 3 coins.", 30, 1.0f,
        []() -> std::unique_ptr<Effect> {
            return std::make_unique<ReactorCard>(
                [](const TurnEvent& e, EffectContext& ctx) {
                    if (e.type == TurnEvent::Type::ItemUsed && e.itemId == "bomb") {
                        ctx.addCoins(3);
                    }
                });
        }
    });

    // Vase of Two: each board resolution rolls the spawn twice (independent
    // random draws, not copies). Copies multiply: two cards = x4, three = x8.
    cardRegistry.registerCard({"vase_of_two", "vase_of_two",
        "Vase of Two", "Twice as many tiles spawn each turn. Copies multiply.", 80, 1.0f,
        []() -> std::unique_ptr<Effect> {
            return std::make_unique<SpawnCountCard>(2);
        }
    });

    // Back to Back: +2 rewind depth, so one copy makes the Hourglass rewind 3
    // turns (additive: two copies = 5). The Hourglass itself reads the depth.
    cardRegistry.registerCard({"back_to_back", "back_to_back",
        "Back to Back", "An Hourglass rewinds 3 turns instead of 1.", 50, 1.0f,
        []() -> std::unique_ptr<Effect> {
            return std::make_unique<RewindDepthCard>(2);
        }
    });

    // Bob: the brick-break signal is the TileMerged event's flag (the only way
    // a brick breaks today). If breaks ever get more sources, promote the flag
    // to a dedicated BrickBroken event and Bob listens to that instead. The
    // granted brick is silently lost if the inventory is full.
    cardRegistry.registerCard({"bob", "bob",
        "Bob", "When a Brick breaks, gain a Brick item.", 40, 1.0f,
        []() -> std::unique_ptr<Effect> {
            return std::make_unique<ReactorCard>(
                [](const TurnEvent& e, EffectContext& ctx) {
                    if (e.type == TurnEvent::Type::TileMerged && e.flag) {
                        ctx.addItem("brick");
                    }
                });
        }
    });

    // Default shop-tile criterion: a copy of the board's current largest tile,
    // so activating the shop costs the player a rebuilt copy of their best tile.
    // Clamped to [2, MaxValue/2] so the activating merge (2x value) never
    // exceeds the highest tile artwork. Swap via setShopTileValueStrategy().
    shopTileValueStrategy = [](const Board& board) {
        return std::clamp(board.getMaxTileValue(), 2, Tile::MaxValue / 2);
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
    // "The world rewinds, the player persists" (docs/effect-engine-design.md §13):
    // the resumed turn replays with the shop countdown it originally started with
    // (board/log rewind was already in place). Coins and inventory deliberately
    // survive the rewind — purchases stay bought, spent coins stay spent.
    shopCountdown = turns.top()->shopCountdownAtStart;
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
    // Model/turn update only. The HUD/inventory widgets are owned and updated by
    // PlayUI (driven from PlayState), so the model stays free of view concerns.
    turns.top()->update(deltaTime);
}

void GameRun::renderBoard(RenderSystem& renderer) {
    turns.top()->render(renderer);
}

sf::Vector2f GameRun::getBoardContentCenter() {
    if (turns.empty()) return {0.f, 0.f};
    return turns.top()->board.getContentCenter();
}

sf::FloatRect GameRun::getBoardContentBounds() {
    if (turns.empty()) return sf::FloatRect({0.f, 0.f}, {0.f, 0.f});
    return turns.top()->board.getContentBounds();
}

int GameRun::getRandomInt(int min, int max) {
    std::uniform_int_distribution<int> dist(min, max);
    return dist(rng);
}

std::vector<const ItemDef*> GameRun::pickShopItems(int count) {
    return itemRegistry.pickMultiple(count, rng);
}

void GameRun::addCoins(int amount, Slot* source) {
    // Spending and no-ops apply directly: the modifier pipeline is for GAINS
    // only (a "×2 coins" chip amplifies income, not expenses).
    if (amount <= 0) {
        coins += amount;
        return;
    }

    // Gain pipeline: thread the mutable amount through the in-scope modifiers
    // (documented order: tile -> slot -> board -> run; the last two join when
    // those scopes carry effects), then apply and log the FINAL value.
    CoinContext coin{source, amount};
    if (source) {
        if (source->tile) {
            for (auto& effect : source->tile->effects) {
                effect->onCoinsResolving(coin);
            }
        }
        source->resolveCoins(coin);
    }
    coins += coin.amount;

    // Attribution: a sourced gain belongs to its board's acting turn (the same
    // channel every board emission uses, so it works for any Turn, not just the
    // run's top one); a sourceless gain to the current turn.
    TurnLog* log = nullptr;
    if (source && source->board && source->board->turn) {
        log = &source->board->turn->log();
    } else if (!turns.empty()) {
        log = &turns.top()->log();
    }
    if (log) {
        log->push(TurnEvent::coinsGained(coin.amount, amount,
            source ? source->getCoord() : Coord{0, 0}, source != nullptr));
    }
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

int GameRun::getEffectiveCost(const CardDef& card) const {
    if (debug::Enabled) return 0;

    int cost = card.cost;
    // Future: card-specific discounts/markups hook in here.
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
    // PlayUI rebuilds its buttons from this changed state (change detection).
}

void GameRun::useItem(size_t index) {
    if (index >= inventoryItems.size()) return;
    // Items act only while the turn awaits input (Begin): PlayUI's buttons poll in
    // every phase, but using one mid-resolution would mutate a board whose move is
    // re-resolved every frame (see Turn::update). Enforced here, in the model, so
    // every caller (UI today, effects tomorrow) inherits the rule.
    if (turns.empty() || turns.top()->currentPhase != Turn::Phase::Begin) return;

    const auto& def = itemRegistry.get(inventoryItems[index]);
    if (def.onUse) {
        bool consumed = def.onUse(*this);
        if (!consumed) return;
    }

    // Logged after the effect resolved (so it follows any TileDestroyed/etc. the
    // item emitted) but still within the same acting turn. PINNED semantics for
    // the Hourglass (whose effect pops the acting turn): the event lands in the
    // log of the turn the player ARRIVES in — chronologically accurate (rewind,
    // then replay) and observed exactly once when that turn completes. Reactors
    // only ever observe completed turns (docs/effect-engine-design.md §13).
    if (!turns.empty()) turns.top()->log().push(TurnEvent::itemUsed(def.id));

    inventoryItems.erase(inventoryItems.begin() + static_cast<ptrdiff_t>(index));
    clearSelection();
    // A consumed item finishes its interaction with the board, so any tiles it had
    // targeted should no longer stay selected (e.g. SWITCH leaving both tiles red).
    clearBoardSelection();
}

void GameRun::discardItem(size_t index) {
    if (index >= inventoryItems.size()) return;

    inventoryItems.erase(inventoryItems.begin() + static_cast<ptrdiff_t>(index));
    clearSelection();
}

void GameRun::toggleSelectedItem(int index) {
    selectedIndex = (selectedIndex == index) ? -1 : index;
}

bool GameRun::hasHeldItem() const {
    return selectedIndex >= 0 && selectedIndex < static_cast<int>(inventoryItems.size());
}

const std::string& GameRun::getHeldItemId() const {
    // Guarded like getHeldItemDef: a stale/absent selection must not index the
    // inventory (mirrors the static-fallback pattern of currentTurnLog).
    static const std::string noItem;
    return hasHeldItem() ? inventoryItems[static_cast<size_t>(selectedIndex)] : noItem;
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

bool GameRun::acquireCard(const std::string& cardId) {
    if (!cardRegistry.has(cardId)) return false;

    const CardDef& def = cardRegistry.get(cardId);
    if (!def.instantiate) return false;
    cards.push_back({cardId, def.instantiate()});
    return true;
}

void GameRun::toggleSelectedCard(int index) {
    selectedCardIndex = (selectedCardIndex == index) ? -1 : index;
}

bool GameRun::discardCard(size_t index) {
    if (index >= cards.size()) return false;
    cards.erase(cards.begin() + static_cast<ptrdiff_t>(index));
    // Indices shifted; a stale selection must not point at the wrong card.
    selectedCardIndex = -1;
    return true;
}

void GameRun::discardSelectedCard() {
    if (selectedCardIndex >= 0 && selectedCardIndex < static_cast<int>(cards.size())) {
        discardCard(static_cast<size_t>(selectedCardIndex));
    }
}

int GameRun::getSpawnCountPerTurn() const {
    // Capped so absurd debug stacks can't freeze a turn in a spawn loop (the
    // board self-limits anyway: spawns into a full board are no-ops).
    constexpr int MaxSpawnsPerTurn = 64;

    int count = 1;
    for (const auto& card : cards) {
        count *= card.effect->spawnCountFactor();
        if (count >= MaxSpawnsPerTurn) return MaxSpawnsPerTurn;
        if (count <= 0) return 0;  // a zero factor means "nothing spawns"
    }
    return count;
}

int GameRun::getRewindDepth() const {
    int depth = 1;
    for (const auto& card : cards) {
        depth += card.effect->rewindDepthBonus();
    }
    return std::max(depth, 0);
}

bool GameRun::ownsCard(const std::string& cardId) const {
    for (const auto& owned : cards) {
        if (owned.defId == cardId) return true;
    }
    return false;
}

void GameRun::addCard(std::unique_ptr<Effect> card) {
    if (card) cards.push_back({"", std::move(card)});
}

void GameRun::dispatchReactors(TurnLog& log, Board& board) {
    if (cards.empty()) return;

    EffectContext ctx(*this, board, log);

    // Iterate by index up to the count captured NOW: reactor mutations may
    // append events to this same log (and reallocate its storage), so no
    // reference into it may be held across a hook call — each event is copied
    // out before dispatch. Appended events are logged but not re-dispatched:
    // observations cannot cascade into more observations (no infinite loops).
    const size_t eventCount = log.events().size();
    for (size_t i = 0; i < eventCount; ++i) {
        const TurnEvent event = log.events()[i];
        for (auto& card : cards) {
            card.effect->onEvent(event, ctx);
        }
    }
    for (auto& card : cards) {
        card.effect->onTurnEnd(log, ctx);
    }
}

const TurnLog& GameRun::currentTurnLog() const {
    // turns is never empty in normal play (the constructor pushes the first turn);
    // the static fallback only guards a pathological empty stack.
    static const TurnLog emptyLog;
    return turns.empty() ? emptyLog : turns.top()->log();
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

bool GameRun::addRandomSlot() {
    if (turns.empty()) return false;
    return turns.top()->board.addRandomSlot();
}

bool GameRun::removeSlotUnder(Tile* t) {
    if (turns.empty()) return false;
    return turns.top()->board.removeSlotUnder(t);
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
}
