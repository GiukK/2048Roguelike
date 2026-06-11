#pragma once

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
// GameRun::dispatchReactors for the duration of one reactor pass — not a
// long-lived object. Modifier hooks do NOT receive it (they mutate their
// context instead; re-entrancy rule on Effect::onCoinsResolving).
class EffectContext {
public:
    EffectContext(GameRun& run, Board& board, const TurnLog& log)
        : run_(run), board_(board), log_(log) {}

    // --- queries ---
    int coins() const;
    const TurnLog& log() const { return log_; }
    Board& board() { return board_; }

    // --- mutations (each routes through the existing guarded entry point) ---
    // Coin gains run the coin pipeline (chips on `source` scale them) and are
    // logged; the reactor pass does NOT re-dispatch the events this appends.
    void addCoins(int amount, Slot* source = nullptr);
    void destroyTile(Tile* tile);          // honors slot protection, logs
    Tile* spawnTile(Coord at, int value);  // nullptr if the cell is taken/missing

private:
    GameRun& run_;
    Board&   board_;
    const TurnLog& log_;
};
