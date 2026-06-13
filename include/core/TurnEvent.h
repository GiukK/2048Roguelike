#pragma once

#include <string>
#include <utility>

#include "core/utils/Coord.h"

// One thing that happened during a single turn (a merge, a spawn, an item use,
// ...). Events are emitted at the action site, DURING the turn in which they
// occur, and collected by TurnLog. They are the substrate that future systems
// read: score (Step 2) and reactive/passive abilities (Step 3).
//
// Design intent — "complete but lean, expandable":
//  - ONE POD with generic, per-type scalar fields. Adding a new Type must not
//    require a new struct nor change how events are stored, so the log and all
//    consumers keep working untouched.
//  - The meaning of valueA/valueB/coord/flag depends on Type (documented per
//    enumerator below). Use the named constructors at the bottom rather than
//    positional aggregate init, so call sites stay readable as fields grow.
struct TurnEvent {
    enum class Type {
        Moved,          // a valid board move resolved.   valueA = Direction (cast to int)
        TileMerged,     // two tiles merged into one.      valueA = resulting value, valueB = source value, coord = target cell, flag = a brick broke
        TileSlid,       // a tile ended the move in a different cell (once per tile, distance-independent; merge movers included). valueA = its post-move value, coord = final cell
        TileSpawned,    // a new tile appeared.            valueA = value, coord = cell
        TileDestroyed,  // a tile was removed by an effect (bombs / black hole). valueA = value, coord = cell
        ShopTriggered,  // a shop was activated by a merge. coord = shop cell
        ShopSpawned,    // a new shop slot appeared.        valueA = phantom tile value, coord = shop cell
        CoinsGained,    // coins were awarded (post-modifier). valueA = final amount, valueB = base amount, flag = sourced at a slot (then coord = that cell)
        CardTriggered,  // a card's onEvent reaction mutated the world (one per card per dispatched event). itemId = card id (empty for engine-level cards)
        ItemUsed,       // an inventory item was consumed.  itemId set
        BossSpawned,    // a boss body appeared.            valueA = max HP, coord = anchor cell
        BossDamaged,    // a tile hit the boss.             valueA = final (post-modifier) damage, valueB = HP after, coord = attacked cell
        BossDefeated,   // the boss died (follows its killing BossDamaged). coord = anchor cell
        AntePhaseChanged // the ante machine moved phase (fight start / boss down / next ante). valueA = the new GameRun::AntePhase (cast to int), valueB = the ante number
    };

    Type type;
    int valueA = 0;      // primary scalar   (meaning depends on Type, see above)
    int valueB = 0;      // secondary scalar (meaning depends on Type, see above)
    Coord coord{0, 0};   // board cell the event happened at (when meaningful)
    bool flag = false;   // per-type boolean (e.g. TileMerged: a brick broke)
    std::string itemId;  // set for ItemUsed; empty otherwise

    // --- Named constructors: one per Type. Keep these in sync with the enum. ---
    static TurnEvent moved(int direction) {
        return {Type::Moved, direction};
    }
    static TurnEvent tileMerged(int resultValue, int sourceValue, Coord at, bool brickBroke) {
        return {Type::TileMerged, resultValue, sourceValue, at, brickBroke};
    }
    static TurnEvent tileSlid(int value, Coord at) {
        return {Type::TileSlid, value, 0, at};
    }
    static TurnEvent tileSpawned(int value, Coord at) {
        return {Type::TileSpawned, value, 0, at};
    }
    static TurnEvent cardTriggered(std::string cardId) {
        TurnEvent e{Type::CardTriggered};
        e.itemId = std::move(cardId);
        return e;
    }
    static TurnEvent tileDestroyed(int value, Coord at) {
        return {Type::TileDestroyed, value, 0, at};
    }
    static TurnEvent shopTriggered(Coord at) {
        return {Type::ShopTriggered, 0, 0, at};
    }
    static TurnEvent shopSpawned(int tileValue, Coord at) {
        return {Type::ShopSpawned, tileValue, 0, at};
    }
    static TurnEvent coinsGained(int amount, int base, Coord at, bool sourced) {
        return {Type::CoinsGained, amount, base, at, sourced};
    }
    static TurnEvent itemUsed(std::string id) {
        TurnEvent e{Type::ItemUsed};
        e.itemId = std::move(id);
        return e;
    }
    static TurnEvent bossSpawned(int maxHp, Coord at) {
        return {Type::BossSpawned, maxHp, 0, at};
    }
    static TurnEvent bossDamaged(int damage, int hpAfter, Coord at) {
        return {Type::BossDamaged, damage, hpAfter, at};
    }
    static TurnEvent bossDefeated(Coord at) {
        return {Type::BossDefeated, 0, 0, at};
    }
    static TurnEvent antePhaseChanged(int newPhase, int ante) {
        return {Type::AntePhaseChanged, newPhase, ante};
    }
};
