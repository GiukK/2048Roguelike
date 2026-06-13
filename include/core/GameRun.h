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
#include "core/CardRegistry.h"
#include "core/BossRegistry.h"

class RenderSystem;
struct AttackContext;

class GameRun {
public:
    using AnimationCallback = std::function<void(std::unique_ptr<Animation>)>;
    using ShopCallback = std::function<void(GameRun*)>;
    using AnimationsActiveQuery = std::function<bool()>;
    // Fired once, when a new turn begins with no legal move on its board (the
    // defeat rule, docs/boss-design.md §8). Same decoupling as ShopCallback:
    // GameRun signals, the state layer decides how to present game over.
    using DefeatCallback = std::function<void(GameRun*)>;

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
    // In normal play only the Hourglass item calls this — synchronously, which
    // its pinned ItemUsed log semantics rely on (the event lands in the
    // arrival turn).
    bool goBack();
    // Defers goBack() to the start of the next update(), where no Turn method
    // is on the call stack. REQUIRED for callers inside the turn machinery
    // (the debug B key in Turn::handleBeginInput): a synchronous goBack there
    // would pop — and destroy — the very Turn whose member function is still
    // executing.
    void requestGoBack() { goBackRequested = true; }
    void openShop();

    // Set like setAnimationsActiveQuery (after construction, by the state
    // layer); tests run headless without one — isDefeated() still latches.
    void setDefeatCallback(DefeatCallback callback) { defeatCallback = std::move(callback); }
    // True once a turn began with no legal move. Latched: the run is over, the
    // callback never re-fires (the game-over presentation is terminal anyway).
    bool isDefeated() const { return defeated; }

    // Drives the whole shop-spawn lifecycle for one completed turn. Called by
    // Turn::endTurn on the just-finished board, before it is cloned into the
    // next turn — so any spawn/removal it performs is inherited by the next
    // turn. Decrements the countdown, freezes it while shops are present,
    // removes consumed shops, and spawns a new shop when the countdown elapses.
    // Frozen entirely outside FreePlay (boss-design §6): consumed shops still
    // vanish, but the countdown holds and nothing new spawns mid-fight.
    void advanceShopState(Board& board);

    // --- The ante machine (boss-design §6/§7) -------------------------------
    // Per ante: free play → shop(s) → BOSS FIGHT → reward → next ante. Ante
    // state is RUN-scoped (it survives undo; the double doors below make the
    // fight boundaries unrewindable anyway).

    enum class AntePhase { FreePlay, BossFight, Reward };

    // Drives one completed turn of the ante clock. Called by Turn::endTurn
    // right after advanceShopState, on the same finished board:
    //  - FreePlay: a boss already on the board (debug key V, future content)
    //    is adopted as this ante's fight IMMEDIATELY — a visible boss that is
    //    mechanically not a fight would be a lie. Otherwise ticks the
    //    countdown; at 0 the ante's boss arrives on the board (time-triggered,
    //    not player-chosen — ignoring shops cannot stall it). A full board
    //    simply retries next turn end.
    //  - BossFight: detects the kill (no body left), grants the run-scoped
    //    reward, moves to Reward.
    //  - Reward: lasts exactly one turn (presentation hooks live there), then
    //    the next ante begins: ante+1, countdown reset, back to FreePlay.
    // All three phase transitions arm a stack CUT (the triple doors, §7),
    // applied by the next newTurn: no rewinding out of a fight, none past the
    // killing blow, none back into a spent ante's reward turn.
    // Every phase transition is logged as an AntePhaseChanged event in the
    // finishing turn's log — observable in the debug turn dump and by the
    // reactors ("when the fight starts ..." content needs no new plumbing).
    void advanceAnteState(Board& board);

    // Runs the boss's per-turn action during BossFight (no-op otherwise), then
    // reaps the boss if that action drove its HP to <= 0
    // (Board::resolveBossDefeatIfDead — the non-sweep death path). Called by
    // Turn::endTurn BEFORE advanceAnteState, so BOTH the action's board
    // consequences (tile destruction, etc.) AND a self-kill's BossDefeated
    // event are present when the kill check runs — a boss that suicides via its
    // own action is genuinely detected, not left as a 0-HP zombie.
    void resolveBossAction(Board& board);

    int getAnte() const { return ante; }
    AntePhase getAntePhase() const { return antePhase; }
    // Stable display names for the phases (debug dumps, future fight UI).
    static const char* antePhaseName(AntePhase phase);
    int getAnteCountdown() const { return anteCountdown; }
    // Tunes the free-play budget; also re-clamps the live countdown while in
    // FreePlay so the new pacing applies to the current ante too.
    void setAnteFreePlayTurns(int turns) {
        anteFreePlayTurns = std::max(1, turns);
        if (antePhase == AntePhase::FreePlay) {
            anteCountdown = std::min(anteCountdown, anteFreePlayTurns);
        }
    }

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

    // The single entry point for coin changes. A positive amount is a GAIN and
    // runs the coin pipeline first — in-scope effects (chips) may scale it via
    // onCoinsResolving — then is applied and logged as a CoinsGained event.
    // `source` is the slot the gain originates over (nullptr = no board origin,
    // e.g. Coin Bag), which decides whose chips see it. Negative amounts
    // (spending) apply directly: chips amplify income, not expenses.
    void addCoins(int amount, Slot* source = nullptr);
    int getCoins() const;

    // Returns the final cost of an item after applying all active modifiers.
    // This is the single hook point for passive abilities, shop discounts,
    // and any future effect that alters item prices.
    int getEffectiveCost(const ItemDef& item) const;
    // Same hook for card prices (kept separate: card and item discounts will
    // likely diverge).
    int getEffectiveCost(const CardDef& card) const;

    bool isInventoryFull() const;
    void addItem(const std::string& itemId);

    // --- Inventory queries / commands for the UI layer (PlayUI) ---
    const std::vector<std::string>& getInventoryItems() const { return inventoryItems; }
    int getSelectedIndex() const { return selectedIndex; }
    // The run-level turn number — NOT the stack size: the double-door cuts
    // drop history without rewinding time (boss-design §7's noted follow-on).
    // newTurn increments it, goBack decrements it (a rewound turn is replayed
    // under its original number), cuts leave it alone.
    size_t getTurnCount() const { return turnNumber; }

    // Monotonic content-change counters for the UI's change detection: bumped
    // on EVERY mutation of the respective list, not just size changes — a
    // same-size replacement (an item consumed while a reactor grants another
    // in the same frame) must still trigger a rebuild, which keying on size
    // would miss. PlayUI compares these against its last observed values.
    unsigned int getInventoryVersion() const { return inventoryVersion; }
    unsigned int getCardsVersion() const { return cardsVersion; }

    // Event log of the current (top) turn — the read side of the per-turn event
    // substrate. Score (Step 2) and reactive abilities (Step 3) consume this.
    const TurnLog& currentTurnLog() const;

    // The acting turn's live board — the engine-level seam (tests, debug
    // tooling), like Board::spawnTileAt/getAllTiles. Gameplay code goes
    // through the specific delegates below (getSelectedTiles, destroyTile,
    // ...) so the interaction surface stays curated; reach for this only when
    // they don't suffice. turns is never empty in normal play (the
    // constructor pushes the first turn).
    Board& currentBoard() { return turns.top()->board; }

    // --- Cards: run-scoped reactors ("the player persists") -----------------
    // Held here, outside the board/turn stack, so they survive undo and are
    // never cloned. Reacting happens via dispatchReactors below.

    // An owned card keeps its registry id alongside the live effect, so the UI
    // (cards panel, shop "already owned" check) can show what the player holds
    // without the Effect itself needing an identity.
    struct OwnedCard {
        std::string defId;  // empty for engine-level cards added without a def
        std::unique_ptr<Effect> effect;
    };

    // Instantiates the registered card and mounts it at run scope. Refuses only
    // unknown ids. STACKING IS LEGAL at the model level: two copies are two
    // reactors and both fire (Balatro-style synergy); whether duplicates are
    // OFFERED is shop policy. Does NOT charge coins: pricing is the shop's job.
    bool acquireCard(const std::string& cardId);
    bool ownsCard(const std::string& cardId) const;
    const std::vector<OwnedCard>& getOwnedCards() const { return cards; }

    // Card selection + discard, mirroring the inventory commands so PlayUI
    // drives both columns the same way. Discarding unmounts the reactor — its
    // effects simply stop. No phase gate: like an item discard, it never
    // touches the board.
    void toggleSelectedCard(int index);
    int getSelectedCardIndex() const { return selectedCardIndex; }
    bool discardCard(size_t index);
    void discardSelectedCard();

    // Engine-level escape hatch (tests, debug): mount a card effect that has no
    // registry def. It won't appear in the cards panel (empty id).
    void addCard(std::unique_ptr<Effect> card);

    // --- Run-scope capability aggregations (over the owned cards) -----------
    // How many tiles spawn during board resolution each turn: the product of
    // every card's spawnCountFactor (Vase of Two stacks multiplicatively),
    // clamped to a sanity cap. Each spawn is an independent random draw.
    int getSpawnCountPerTurn() const;
    // How many turns an Hourglass rewinds: 1 + the sum of every card's
    // rewindDepthBonus (Back to Back), floored at 0.
    int getRewindDepth() const;

    // Streaming reactor dispatch (design doc §9): dispatches the turn's not-yet-
    // seen events (from turn.reactorCursor) to the cards, in log order, then
    // advances the cursor PAST anything the reactors appended — so reactions
    // are never re-dispatched (no cascades) and every event fires exactly once
    // no matter how often this is called. Called at the SAFE POINTS where the
    // world is between atomic operations: after an item use, after the move
    // sweep resolves, after board-resolution spawns, and at end of turn —
    // never mid-sweep, so a reactor can't corrupt an in-flight move.
    void flushReactors(Turn& turn);

    // The aggregate pass: one onTurnEnd per card over the full log. Called by
    // Turn::endTurn after the final flush, BEFORE the board is cloned into the
    // next turn — so reactor mutations to the board carry forward.
    void dispatchTurnEnd(Turn& turn);

    // The run-scope leg of the attack pipeline (boss-design §3): cards that
    // modify attacks ("+50% boss damage") hook onAttackResolving here.
    // Modifiers, not reactors — no attribution, no events; the BossDamaged
    // event is logged by the apply site after this returns. Called by Board's
    // attack resolution, mirroring how merge modifiers reach the run scope.
    void resolveAttackRunScope(AttackContext& attack);

    // The boss on the acting board, or nullptr. PlayUI reads the fight banner
    // through this (the countdown-display pattern: UI reads, never holds).
    const Boss* getCurrentBoss() const;
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
    // Swap refuses protected slots (the shop) by default, mirroring the other
    // board manipulations. allowProtected is the explicit opt-in for content
    // that moves shop tiles as a mechanic — the Switch item uses it; future
    // effects decide per-case as a balance call.
    void swapTiles(Tile* a, Tile* b, bool allowProtected = false);
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
    CardRegistry& getCardRegistry() { return cardRegistry; }
    BossRegistry& getBossRegistry() { return bossRegistry; }

    bool shopOpen = false;

private:
    void useItem(size_t index);
    void discardItem(size_t index);
    void clearSelection();

    // One door of §7: drops every turn below the current top, making it the
    // new stack floor (goBack already refuses at depth 1). Applied by newTurn
    // when an ante transition armed stackCutPending. The dropped turns are
    // RETIRED (moved to retiredTurns), not destroyed: newTurn's caller is
    // Turn::endTurn — a member function of one of the dropped turns — so
    // destroying them here would free the very Turn still executing (the same
    // hazard requestGoBack defers around). update() empties the graveyard.
    void cutTurnStack();

    // Logs the just-applied ante transition as an AntePhaseChanged event in
    // the finishing turn's log (see advanceAnteState). Call AFTER mutating
    // antePhase/ante so the event carries the new state.
    void logAnteTransition(Board& board);

    // Placeholder reward/scaling formulas until balance work: linear in the
    // ante number, routed through the normal hooks (addCoins; a def copy with
    // scaled baseHp) so replacing them later touches only these lines.
    int anteReward() const { return 50 * ante; }
    const std::string& bossIdForAnte() const;

    RenderSystem& renderer;
    AnimationCallback animationCallback;
    ShopCallback shopCallback;
    AnimationsActiveQuery animationsActive;
    DefeatCallback defeatCallback;
    bool defeated = false;

    std::mt19937 rng;
    unsigned int runSeed = 0;  // the seed rng was actually seeded with

    std::stack<std::unique_ptr<Turn>> turns;

    // The double doors' graveyard: turns dropped by cutTurnStack, kept alive
    // until the next update() because the cut fires while one of them is
    // still executing endTurn (see cutTurnStack).
    std::vector<std::unique_ptr<Turn>> retiredTurns;

    // Run-scoped reactors, in acquisition order — the deterministic dispatch
    // order for the run scope (no board position to sort by).
    std::vector<OwnedCard> cards;

    std::vector<std::string> inventoryItems;

    // Content versions (see getInventoryVersion/getCardsVersion above). Both
    // start at 0 = "never mutated", matching a fresh PlayUI's last-seen state.
    unsigned int inventoryVersion = 0;
    unsigned int cardsVersion = 0;

    // Selection: only one item at a time. -1 = nothing selected. This is the
    // "held item" the player is interacting with; PlayUI shows its action buttons.
    int selectedIndex = -1;

    // Card selection (independent of the item one — separate columns). -1 = none.
    int selectedCardIndex = -1;

    ItemRegistry itemRegistry;
    CardRegistry cardRegistry;
    BossRegistry bossRegistry;

    int coins = 100;
    unsigned int maxInventorySize = 3;

    // Set by requestGoBack(), consumed at the top of update() (see goBack docs).
    bool goBackRequested = false;

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

    // --- Ante state (see the public section above) ---
    // anteFreePlayTurns: the per-ante budget (the "~3 shop cycles").
    // anteCountdown:     free-play turns left before the boss arrives; per-turn
    //                    snapshots rewind it like the shop countdown.
    // stackCutPending:   armed by the fight-start/fight-end transitions,
    //                    consumed by the next newTurn (the double doors).
    int ante = 1;
    AntePhase antePhase = AntePhase::FreePlay;
    int anteFreePlayTurns = 30;
    int anteCountdown = 30; // mirrors anteFreePlayTurns at construction
    bool stackCutPending = false;

    // Run-level turn number (see getTurnCount).
    size_t turnNumber = 1; // the genesis turn pushed by the constructor
};
