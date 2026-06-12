#pragma once

#include <string>

#include "core/utils/Coord.h"

class GameRun;
class Board;
class Slot;
class Tile;
class TurnLog;

// The mutation façade REACTORS act through (cards reacting to the turn log):
// instead of poking GameRun/Board internals, a reactor speaks this vocabulary,
// so every mutation stays interceptable (addCoins routes through the coin
// pipeline, destroyTile honors slot protection and logs, ...).
//
// It is a thin adapter BOUND to one acting turn's board + log, built by
// GameRun::flushReactors for the duration of one reactor pass — not a
// long-lived object. Modifier hooks do NOT receive it (they mutate their
// context instead; re-entrancy rule on Effect::onCoinsResolving).
//
// ACTIVATION OBSERVABILITY: during the onEvent phase the dispatcher marks which
// card is acting (beginCardDispatch); the FIRST mutation that card performs for
// that event logs one CardTriggered — so "a card fired" is itself an event
// other effects can read (Card Ruler). One per (card, event): reacting to three
// events logs three ("how many times" stays parsable); two mutations inside one
// reaction log one. onTurnEnd-phase mutations log nothing — end-of-turn cards
// are deliberately outside the activation count.
class EffectContext {
public:
    EffectContext(GameRun& run, Board& board, TurnLog& log)
        : run_(run), board_(board), log_(log) {}

    // --- queries ---
    int coins() const;
    const TurnLog& log() const { return log_; }
    Board& board() { return board_; }

    // The board-resident owner of the effect CURRENTLY being dispatched: the
    // slot it is mounted on (slot effects) or under (a tile effect's current
    // slot). Null while run-scoped cards dispatch — cards have no position.
    // THE owner-identity interface (boss-design §9.2), decided once: the
    // dispatcher passes the owner through the context at dispatch time, it is
    // never bound into the effect at mount — so board clones never rebind
    // back-pointers. "Destroy a slot adjacent to ME" reads this; the Boss
    // will join with its own owner accessor when the entity lands.
    Slot* ownerSlot() const { return ownerSlot_; }

    // --- mutations (each routes through the existing guarded entry point) ---
    // Coin gains run the coin pipeline (chips on `source` scale them) and are
    // logged; the reactor pass does NOT re-dispatch the events this appends.
    void addCoins(int amount, Slot* source = nullptr);
    void destroyTile(Tile* tile);          // honors slot protection, logs
    // nullptr if the cell is taken/missing OR `value` has no artwork
    // (Tile::isValidValue) — an unbacked spawn is refused, never crashes.
    Tile* spawnTile(Coord at, int value);
    // Grants an inventory item (Bob). Silently lost when the inventory is full
    // — the standard "reward you had no room for" roguelike rule.
    void addItem(const std::string& itemId);

    // --- dispatcher-only (called by GameRun::flushReactors) -----------------
    // Marks the card whose onEvent is about to run; resets the once-per-
    // dispatch activation latch. `cardId` must outlive the dispatch (it points
    // at the OwnedCard's defId, which nothing mutates mid-pass).
    void beginCardDispatch(const std::string& cardId);
    void endCardDispatch();

    // Dispatcher-only: sets the owner for the board-scope legs (nullptr when
    // moving on to the run scope). See ownerSlot().
    void setDispatchOwner(Slot* owner) { ownerSlot_ = owner; }

private:
    // Logs the CardTriggered activation on a card's first mutation (see above).
    // Called BEFORE the mutation executes, so cause precedes consequence in
    // the log (CardTriggered, then the CoinsGained it produced).
    void noteActivation();

    GameRun& run_;
    Board&   board_;
    TurnLog& log_;

    const std::string* activeCardId = nullptr;
    bool activationLogged = false;
    Slot* ownerSlot_ = nullptr;
};
