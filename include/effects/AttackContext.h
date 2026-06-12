#pragma once

class Boss;
class Tile;

// The mutable outcome of one tile-vs-boss attack, mirroring MergeContext
// (boss-design §3): built at the interaction site by Board's attack
// resolution, threaded through the in-scope onAttackResolving modifiers
// BEFORE it is applied and logged — the chips/cards composition point
// ("+50% damage from this slot" scales `damage` here, no boss knows it
// exists).
//
// Pipeline scoping for this slice (pinned §3 DECISION): the attacker's tile
// effects, then the run scope (cards). Slot scopes join when the chip
// mounting layer lands.
struct AttackContext {
    Boss* boss;      // who is being hit (immutable input)
    Tile* attacker;  // the incoming tile (immutable input)
    int damage;      // MUTABLE: starts at the boss's proposal; floored at 0 at apply
    bool consumeAttacker = true;  // MUTABLE: future "ghost strike" effects keep the tile
};
