#pragma once

#include <memory>

struct MergeContext;

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

    // MODIFIER hook: runs DURING a merge that resolves on this effect's slot, on
    // the merge's MUTABLE outcome, and may change it (today: the merged value;
    // future knobs — coin reward, destroy-on-merge — join MergeContext with their
    // slices). Default no-op, so an effect that doesn't touch merges costs nothing.
    // A side-effecting effect (e.g. ShopEffect) may also use it to fire at merge
    // time without altering the outcome. Dispatched by Slot::resolveMerge.
    virtual void onMergeResolving(MergeContext& /*merge*/) {}

    // Optional slot skin: texture id the owning slot adopts while this effect is
    // mounted (nullptr = leave the slot's look unchanged). Each slot TYPE (shop,
    // dark shop, event...) carries its own look here instead of Slot::addEffect
    // hardcoding one. Chips will likely want an overlay, not a replacement —
    // design that with the mounting layer.
    virtual const char* slotTextureId() const { return nullptr; }
};
