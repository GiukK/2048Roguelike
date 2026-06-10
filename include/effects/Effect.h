#pragma once

#include <memory>

class Slot;

// Base class for every gameplay effect — the single mechanism behind slot
// effects, chips (effects mounted on a slot/board), tile tags and run-level
// cards. An effect's SCOPE is decided by what owns it (a Slot, a Tile, the
// Board, the GameRun), not by its type, so the same Effect can live in any scope.
//
// An effect plays one or both ROLES (see docs/effect-engine-design.md):
//   - MODIFIER: acts DURING an interaction on a mutable outcome and may change
//     it (chips, special slots). Added via the merge/coin pipeline hooks.
//   - REACTOR: observes what already resolved (the turn event log) and acts
//     (cards). Cannot rewrite the past.
// Hooks are default no-ops so each effect overrides only what it needs (one base
// instead of split capability interfaces — resolved design decision).
class Effect {
public:
    virtual ~Effect() = default;

    // Effects are value-cloned when the board is cloned between turns (undo
    // stack); must deep-copy any state that should survive that clone.
    virtual std::unique_ptr<Effect> clone() const = 0;

    // Whether this effect survives the turn-to-turn board clone. Persistent by
    // default; a transient effect (e.g. "frozen for this turn only") returns
    // false and is dropped by the clone path — so the persistent-vs-transient
    // rule lives here, ONCE, instead of as per-field special cases scattered in
    // the clone code. (Used once the tile-tag slice lands.)
    virtual bool isPersistent() const { return true; }

    // Fired when a tile merges into the slot carrying this effect. Default
    // no-op. NOTE: this is the current behavior-preserving hook; it becomes the
    // modifier-aware merge pipeline (onMergeResolving(MergeContext&)) in the
    // next slice. Kept signature-compatible for now.
    virtual void onMerge(Slot* /*slot*/) {}
};
