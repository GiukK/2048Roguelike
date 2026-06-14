#include "core/GameRun.h"
#include "content/BaseContent.h"
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

    // Content lives in its own unit (content/BaseContent): GameRun provides the
    // registries (mechanism), the catalogue fills them (policy).
    content::registerBaseItems(itemRegistry);
    content::registerBaseCards(cardRegistry);
    content::registerBaseBosses(bossRegistry);

    // Default shop-tile criterion: a copy of the board's current largest tile,
    // so activating the shop costs the player a rebuilt copy of their best tile.
    // Clamped to [2, MaxValue/2] so the activating merge (2x value) never
    // exceeds the highest tile artwork. Swap via setShopTileValueStrategy().
    shopTileValueStrategy = [](const Board& board) {
        return std::clamp(board.getMaxTileValue(), 2, Tile::MaxValue / 2);
    };

    // Default ante boss: the Sleeper, with placeholder linear HP scaling
    // (baseHp × ante). The whole "which boss, how hard per ante" policy lives
    // here behind the strategy seam, so the ante machine never names a boss
    // (see setBossForAnteStrategy / advanceAnteState).
    bossForAnteStrategy = [](const BossRegistry& reg, int ante) {
        BossDef def = reg.get("sleeper");
        def.baseHp *= ante;
        return def;
    };

    // Default post-defeat reward offer: 3 options from cards + consumables (see
    // defaultRewardOffer). Swappable via setRewardOfferStrategy.
    rewardOfferStrategy = &GameRun::defaultRewardOffer;

    shopCountdown = shopSpawnInterval;

    turns.push(std::make_unique<Turn>(renderer, this));
}

void GameRun::enter() {}
void GameRun::exit() {}

void GameRun::newTurn(const Board& currentBoard) {
    turns.push(std::make_unique<Turn>(renderer, this, currentBoard));
    ++turnNumber;

    // The double doors (boss-design §7), applied where the new top turn
    // becomes the stack FLOOR: armed by the ante transitions (fight start,
    // fight end), so there is no rewinding out of a fight once entered and
    // none past the killing blow once won. The cuts also bound the turn
    // stack; getTurnCount() lives on the run-level counter above.
    if (stackCutPending) {
        cutTurnStack();
        stackCutPending = false;
    }

    // The defeat check (docs/boss-design.md §8), at its safe point: a turn that
    // BEGINS with no legal move is the lost position. Checking the freshly
    // cloned board means everything end-of-turn did — resolution spawns, the
    // shop lifecycle, reactor mutations — has already settled, so a shop whose
    // phantom a tile can still feed counts as the escape valve it is. Items
    // are deliberately ignored (Board::hasLegalMove). Latched: one signal per
    // run, and the genesis turn (constructor) is never checked — a fresh board
    // with one tile always has a move.
    if (!defeated && !turns.top()->board.hasLegalMove()) {
        defeated = true;
        if (defeatCallback) defeatCallback(this);
    }
}

bool GameRun::goBack() {
    if (turns.size() <= 1) return false;
    turns.pop();
    --turnNumber;
    // "The world rewinds, the player persists" (docs/effect-engine-design.md §13):
    // the resumed turn replays with the shop AND ante countdowns it originally
    // started with (board/log rewind was already in place) — a replayed turn
    // must not tick either clock twice. Coins and inventory deliberately
    // survive the rewind — purchases stay bought, spent coins stay spent.
    shopCountdown = turns.top()->shopCountdownAtStart;
    anteCountdown = turns.top()->anteCountdownAtStart;
    return true;
}

void GameRun::cutTurnStack() {
    if (turns.size() <= 1) return;
    std::unique_ptr<Turn> top = std::move(turns.top());
    turns.pop();
    while (!turns.empty()) {
        // Retire, don't destroy: Turn::endTurn — still on the call stack when
        // newTurn applies this cut — belongs to one of these turns. They die
        // at the next update(), where no Turn method is executing.
        retiredTurns.push_back(std::move(turns.top()));
        turns.pop();
    }
    turns.push(std::move(top));
}

void GameRun::openShop() {
    if (shopOpen) return;
    shopOpen = true;
    if (shopCallback) {
        shopCallback(this);
    }
}

void GameRun::openReward() {
    if (rewardOpen) return;
    // No presenter wired (headless harness, or a reward armed before the state
    // layer attached) → skip rather than soft-lock the Phase::Begin gate, which
    // would otherwise wait forever for a modal that can never open.
    if (!rewardCallback) { dismissReward(); return; }
    rewardOpen = true;
    rewardCallback(this);
}

void GameRun::pickReward(size_t index) {
    // Apply the chosen grant (addItem / acquireCard / ...), then close. An
    // out-of-range index still closes the picker — the reward is consumed.
    if (index < rewardOffer.size() && rewardOffer[index].apply) {
        rewardOffer[index].apply(*this);
    }
    dismissReward();
}

void GameRun::dismissReward() {
    rewardPending = false;
    rewardOpen = false;
    rewardOffer.clear();
}

std::vector<RewardOption> GameRun::defaultRewardOffer(const RewardContext& ctx) {
    GameRun& run = ctx.run;

    // Build the candidate pool from BOTH families, each option carrying its own
    // grant lambda (so the picker stays content-agnostic): every consumable,
    // plus every card the player does not already own (the shop's normal policy).
    std::vector<RewardOption> pool;
    for (const ItemDef* def : run.getItemRegistry().getAll()) {
        pool.push_back({"item:" + def->id, def->textureId, def->name, def->description,
                        [id = def->id](GameRun& r) { r.addItem(id); }});
    }
    for (const CardDef* def : run.getCardRegistry().getAll()) {
        if (!run.ownsCard(def->id)) {
            pool.push_back({"card:" + def->id, def->textureId, def->name, def->description,
                            [id = def->id](GameRun& r) { r.acquireCard(id); }});
        }
    }

    // Up to 3 DISTINCT options, uniformly drawn (remove-on-pick, so no dupes).
    constexpr size_t OfferCount = 3;
    std::vector<RewardOption> offer;
    while (offer.size() < OfferCount && !pool.empty()) {
        int idx = run.getRandomInt(0, static_cast<int>(pool.size()) - 1);
        offer.push_back(std::move(pool[static_cast<size_t>(idx)]));
        pool.erase(pool.begin() + idx);
    }
    return offer;
}

void GameRun::advanceShopState(Board& board) {
    // Order matters. We mutate `board` (the turn that just ended, already past
    // its move + tile spawn) so the result is baked into the clone the next turn
    // is built from. See Turn::endTurn for the call site.
    const int maxShops = static_cast<int>(maxShopsOnBoard);

    // 1. A shop merged during this turn disappears from the next board.
    int consumed = board.removeConsumedShops();

    // Frozen outside FreePlay (boss-design §6): consumed shops still vanish
    // (above — a merged shop may not haunt the board), but the countdown holds
    // and nothing new spawns during a fight or the reward turn. A consumption
    // mid-fight therefore skips the usual countdown restart: the clock simply
    // resumes from its frozen value next ante — accepted, the fight froze it.
    if (antePhase != AntePhase::FreePlay) return;

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

const char* GameRun::antePhaseName(AntePhase phase) {
    switch (phase) {
    case AntePhase::FreePlay:  return "FreePlay";
    case AntePhase::BossFight: return "BossFight";
    case AntePhase::Reward:    return "Reward";
    }
    return "Unknown";
}

void GameRun::logAnteTransition(Board& board) {
    if (board.turn) {
        board.turn->log().push(TurnEvent::antePhaseChanged(
            static_cast<int>(antePhase), ante));
    }
}

void GameRun::resolveBossAction(Board& board) {
    if (antePhase != AntePhase::BossFight) return;
    Boss* boss = board.getBoss();
    if (!boss) return;
    if (board.turn) {
        EffectContext ctx(*this, board, board.turn->log());
        boss->runTurnAction(ctx);
        // An action that drove the boss's HP to <= 0 (self-damage; later a
        // damageBoss primitive) kills it HERE — the non-sweep death path. We
        // are at end of turn, not mid-sweep, so the death resolves inline and
        // safely. Without this a self-damaging boss would be an unreaped 0-HP
        // zombie the kill check below never sees (it reads the BossDefeated
        // event, which only resolveBossDefeat emits).
        board.resolveBossDefeatIfDead();
    }
}

void GameRun::advanceAnteState(Board& board) {
    switch (antePhase) {
    case AntePhase::FreePlay: {
        // A boss already on the board (debug key V today; future content that
        // drops a body early) preempts the clock: it IS this ante's fight,
        // starting now. Waiting out the countdown would leave a visible boss
        // that is mechanically not a fight — no phase, no reward on the kill.
        Boss* boss = board.getBoss();
        if (boss) {
            if (debug::Enabled) {
                std::cout << "[ante " << ante
                          << "] boss already on the board - adopted as the fight\n";
            }
        } else {
            if (anteCountdown > 0) --anteCountdown;
            if (anteCountdown > 0) return;

            // Budget exhausted: the ante's boss arrives (the strategy picks
            // and scales it) — a full board leaves the countdown at 0 and
            // simply retries here next turn end.
            BossDef def = bossForAnteStrategy(bossRegistry, ante);
            boss = board.spawnBossInRandomEmptySlot(def);
            if (!boss && debug::Enabled) {
                std::cout << "[ante " << ante
                          << "] boss due, but no free slot - retrying next turn\n";
            }
        }
        if (boss) {
            antePhase = AntePhase::BossFight;
            stackCutPending = true;  // door one: no rewinding out of the fight
            logAnteTransition(board);
            if (debug::Enabled) {
                std::cout << "[ante " << ante << "] boss fight: "
                          << boss->getName() << " (" << boss->getMaxHp()
                          << " hp)\n";
            }
        }
        break;
    }

    case AntePhase::BossFight:
        // The kill is read from the LOG, not from body-absence. A BossDefeated
        // event means the ante's boss DIED this turn (one boss at a time; the
        // log is per-turn and cleared at endTurn, so no stale kill leaks in).
        // Polling !getBoss() would conflate "killed" with "body gone for any
        // reason": a future flee / banish / transform mechanic removes the body
        // WITHOUT a kill and must not pay the reward. The event is the meaning;
        // the body removal is merely its mechanism. Both sweep and self-damage
        // kills route their BossDefeated through Board::resolveBossDefeat, so
        // this one check covers every death path.
        if (board.turn) {
            // Find the kill event (one boss at a time, one per turn) to learn
            // WHICH boss fell — the offer can branch on it. The body is already
            // reaped by now, so the defId rides the event, not live state.
            const TurnEvent* defeat = nullptr;
            for (const TurnEvent& e : board.turn->log().events()) {
                if (e.type == TurnEvent::Type::BossDefeated) { defeat = &e; break; }
            }
            if (defeat) {
                // Build the offer NOW (deterministic, with the run RNG) and arm
                // the picker; the GRANT happens when the player chooses (start
                // of the Reward turn — Turn::update Phase::Begin → openReward).
                // The reward is run-scoped and door two (armed below) makes it
                // unrewindable.
                RewardContext ctx{defeat->itemId, ante, *this};
                rewardOffer = rewardOfferStrategy ? rewardOfferStrategy(ctx)
                                                  : std::vector<RewardOption>{};
                rewardPending = true;
                antePhase = AntePhase::Reward;
                stackCutPending = true;  // door two: no rewinding past the blow
                logAnteTransition(board);
                if (debug::Enabled) {
                    std::cout << "[ante " << ante << "] boss down: "
                              << rewardOffer.size() << " reward option(s)\n";
                }
            }
        }
        break;

    case AntePhase::Reward:
        // The reward turn is over: the next ante begins, harder.
        ++ante;
        anteCountdown = anteFreePlayTurns;
        antePhase = AntePhase::FreePlay;
        stackCutPending = true;  // door three: no rewinding into a spent ante
        logAnteTransition(board);
        if (debug::Enabled) {
            std::cout << "[ante " << ante << "] free play, boss in "
                      << anteCountdown << " turns\n";
        }
        break;
    }
}

void GameRun::handleInput(sf::Event& event) {
    if (!turns.empty()) {
        turns.top()->handleInput(event);
    }
}

void GameRun::update(float deltaTime) {
    // Turns retired by a double-door cut die HERE, for the same reason the
    // rewind below is deferred: no Turn method is on the call stack now, so
    // destroying Turn objects is safe (see cutTurnStack).
    retiredTurns.clear();

    // A rewind requested from inside the current turn (debug B) is processed
    // HERE, before entering Turn::update: at this point no Turn method is on
    // the call stack, so popping (destroying) the turn is safe.
    if (goBackRequested) {
        goBackRequested = false;
        goBack();
    }

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
    // Debug mode: everything is free so items can be grabbed and tested
    // instantly. Runtime-gated (debug::active) so the DEBUG toggle can switch
    // to the real economy without a rebuild.
    if (debug::active()) return 0;

    int cost = item.cost;
    // Future: apply passive modifiers, shop discount abilities, etc.
    return std::max(cost, 0);
}

int GameRun::getEffectiveCost(const CardDef& card) const {
    if (debug::active()) return 0;

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
    ++inventoryVersion;
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
    // the Hourglass (whose effect pops acting turns): the event lands in the
    // log of the turn the player ARRIVES in — chronologically accurate (rewind,
    // then replay). The flush below makes item reactions INSTANT (Economic
    // Boom pays the moment the bomb goes off), not end-of-turn.
    if (!turns.empty()) {
        turns.top()->log().push(TurnEvent::itemUsed(def.id));
        flushReactors(*turns.top());
    }

    inventoryItems.erase(inventoryItems.begin() + static_cast<ptrdiff_t>(index));
    ++inventoryVersion;
    clearSelection();
    // A consumed item finishes its interaction with the board, so any tiles it had
    // targeted should no longer stay selected (e.g. SWITCH leaving both tiles red).
    clearBoardSelection();
}

void GameRun::discardItem(size_t index) {
    if (index >= inventoryItems.size()) return;

    inventoryItems.erase(inventoryItems.begin() + static_cast<ptrdiff_t>(index));
    ++inventoryVersion;
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
    ++cardsVersion;
    return true;
}

void GameRun::toggleSelectedCard(int index) {
    selectedCardIndex = (selectedCardIndex == index) ? -1 : index;
}

bool GameRun::discardCard(size_t index) {
    if (index >= cards.size()) return false;
    cards.erase(cards.begin() + static_cast<ptrdiff_t>(index));
    ++cardsVersion;
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
    // Sum of what the cards grant; they REPLACE the base single rewind, they
    // don't add to it (two Back to Back = 6, "as if you used 6 Hourglasses").
    int granted = 0;
    for (const auto& card : cards) {
        granted += card.effect->hourglassRewinds();
    }
    return granted > 0 ? granted : 1;
}

bool GameRun::ownsCard(const std::string& cardId) const {
    for (const auto& owned : cards) {
        if (owned.defId == cardId) return true;
    }
    return false;
}

void GameRun::addCard(std::unique_ptr<Effect> card) {
    if (card) {
        cards.push_back({"", std::move(card)});
        ++cardsVersion;
    }
}

void GameRun::flushReactors(Turn& turn) {
    TurnLog& log = turn.log();

    // Board-resident reactors (effects mounted on tiles/slots — boss maluses
    // later) join the run-scoped cards in this pass. The dispatch list is
    // SNAPSHOT up front: reactors may mount or destroy board effects
    // mid-flush, and an effect mounted mid-flush joins the NEXT flush — the
    // same no-cascade family as the events reactors append.
    const std::vector<Effect*> boardEffects = turn.board.collectBoardEffects();

    if (boardEffects.empty() && cards.empty()) {
        // Keep the cursor honest even with no reactors mounted, so one bought
        // mid-run doesn't retroactively see the whole backlog.
        turn.reactorCursor = log.events().size();
        return;
    }

    EffectContext ctx(*this, turn.board, log);

    // Dispatch [cursor, size-at-entry): reactor mutations may append events to
    // this same log (and reallocate its storage), so no reference into it may
    // be held across a hook call — each event is copied out before dispatch.
    const size_t eventCount = log.events().size();
    for (size_t i = turn.reactorCursor; i < eventCount; ++i) {
        const TurnEvent event = log.events()[i];

        // Board scope first — the pinned cross-scope order (engine doc §6:
        // tile → slot → board-owned → run). Every owner is RE-VALIDATED on the
        // live board before its effect fires: a reactor earlier in this very
        // pass may have destroyed it (the owner-lifetime wrinkle, boss-design
        // §9.2) — a vanished owner means the effect is skipped, never called.
        // Accepted corner: should the allocator hand a destroyed effect's
        // address to one mounted mid-flush, the newcomer is dispatched early;
        // benign (it would legitimately see all later events anyway).
        for (Effect* effect : boardEffects) {
            Slot* owner = turn.board.findOwnerSlot(effect);
            if (!owner) continue;
            ctx.setDispatchOwner(owner);
            effect->onEvent(event, ctx);
        }
        ctx.setDispatchOwner(nullptr);

        for (auto& card : cards) {
            // Attribute the upcoming reaction: its first mutation logs one
            // CardTriggered (activation observability — see EffectContext).
            ctx.beginCardDispatch(card.defId);
            card.effect->onEvent(event, ctx);
        }
        // Close attribution before the next event's board leg: board-effect
        // mutations must never be billed to this event's last card. (Board
        // reactions are attribution-free for now — revisit with boss content
        // if "the boss's malus fired" needs to be a readable event.)
        ctx.endCardDispatch();
    }
    // Jump PAST anything the reactors appended (CardTriggered included):
    // reactions are logged (the turn record stays truthful) but never
    // re-dispatched — no cascades, no loops.
    turn.reactorCursor = log.events().size();
}

void GameRun::dispatchTurnEnd(Turn& turn) {
    const std::vector<Effect*> boardEffects = turn.board.collectBoardEffects();
    if (boardEffects.empty() && cards.empty()) return;

    EffectContext ctx(*this, turn.board, turn.log());

    // Same scope order and same defensive owner re-validation as the
    // streaming pass — an onTurnEnd reaction can destroy a later owner too.
    for (Effect* effect : boardEffects) {
        Slot* owner = turn.board.findOwnerSlot(effect);
        if (!owner) continue;
        ctx.setDispatchOwner(owner);
        effect->onTurnEnd(turn.log(), ctx);
    }
    ctx.setDispatchOwner(nullptr);

    for (auto& card : cards) {
        card.effect->onTurnEnd(turn.log(), ctx);
    }
}

void GameRun::resolveAttackRunScope(AttackContext& attack) {
    for (auto& card : cards) {
        card.effect->onAttackResolving(attack);
    }
}

const Boss* GameRun::getCurrentBoss() const {
    return turns.empty() ? nullptr : turns.top()->board.getBoss();
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

void GameRun::swapTiles(Tile* a, Tile* b, bool allowProtected) {
    if (!turns.empty()) {
        turns.top()->board.swapTiles(a, b, allowProtected);
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
