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
        TileSpawned,    // a new tile appeared.            valueA = value, coord = cell
        TileDestroyed,  // a tile was removed by an effect (bombs / black hole). valueA = value, coord = cell
        ShopTriggered,  // a shop was activated by a merge. coord = shop cell
        ItemUsed        // an inventory item was consumed.  itemId set
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
    static TurnEvent tileSpawned(int value, Coord at) {
        return {Type::TileSpawned, value, 0, at};
    }
    static TurnEvent tileDestroyed(int value, Coord at) {
        return {Type::TileDestroyed, value, 0, at};
    }
    static TurnEvent shopTriggered(Coord at) {
        return {Type::ShopTriggered, 0, 0, at};
    }
    static TurnEvent itemUsed(std::string id) {
        TurnEvent e{Type::ItemUsed};
        e.itemId = std::move(id);
        return e;
    }
};
