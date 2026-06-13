#pragma once

#include <memory>
#include <string>

struct MergeContext;
struct CoinContext;
struct AttackContext;
struct TurnEvent;
class TurnLog;
class EffectContext;

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
    // Pre-apply: nothing has been logged yet, so side effects that must FOLLOW
    // the TileMerged event in the log belong in onMergeApplied below.
    // Dispatched by Slot::resolveMerge.
    virtual void onMergeResolving(MergeContext& /*merge*/) {}

    // Post-apply notify: runs AFTER the (modifier-shaped) outcome has been
    // applied to the board and the TileMerged event logged. The context is
    // const — this hook reacts at the interaction site, it cannot rewrite the
    // outcome. Site side effects live here (ShopEffect opening the shop) so
    // whatever they emit lands AFTER the TileMerged that caused it — the log's
    // cause-before-consequence rule. Dispatched by Slot::notifyMergeApplied.
    virtual void onMergeApplied(const MergeContext& /*merge*/) {}

    // MODIFIER hook: runs on every coin GAIN resolving in this effect's scope and
    // may change the amount (e.g. a "×2 coins over this slot" chip). Re-entrancy
    // rule: mutate the context, never award coins from inside the hook — that
    // would recurse the pipeline. Dispatched by GameRun::addCoins.
    virtual void onCoinsResolving(CoinContext& /*coin*/) {}

    // MODIFIER hook: runs on a resolving tile-vs-boss attack (AttackContext)
    // before it is applied and the BossDamaged event logged. In-scope today:
    // the attacker's tile effects + the run cards (boss-design §3); slot
    // scopes join with the chip mounting layer. Dispatched by Board's attack
    // resolution.
    virtual void onAttackResolving(AttackContext& /*attack*/) {}

    // --- REACTOR hooks (cards): observe a COMPLETED turn, act via ctx --------
    // Dispatched by GameRun::dispatchReactors at end of turn, over the turn's
    // log. They cannot rewrite the past (the events are already applied); they
    // act on the world through EffectContext, and what they cause is logged in
    // the same turn but NOT re-dispatched (no reaction cascades).

    // Called once per logged event, in log order (event-major: every card sees
    // event i before any card sees event i+1 — the Balatro left-to-right rule).
    virtual void onEvent(const TurnEvent& /*event*/, EffectContext& /*ctx*/) {}

    // Called once after the per-event pass, for aggregate reactions over the
    // whole turn ("if you merged 3+ times this turn...").
    virtual void onTurnEnd(const TurnLog& /*log*/, EffectContext& /*ctx*/) {}

    // --- Capability queries (defaults: none) --------------------------------
    // An effect declares what its PRESENCE does to its owner; owners aggregate
    // them (Tile::isImmobilized, Slot::isProtected) so board logic asks "can
    // this move? is this protected?" without ever type-switching on concrete
    // effect classes. Add a capability here when a second effect needs it.

    // Owner cannot slide nor initiate a merge (it can still be merged INTO).
    // Brick / frozen tiles carry this.
    virtual bool immobilizesOwner() const { return false; }

    // Owner is off-limits to board manipulation: destroy, wrench, shuffle, and
    // area-effect targeting. The shop carries this (a broken shop would freeze
    // the spawn countdown forever).
    virtual bool protectsOwner() const { return false; }

    // --- Run-scope capability queries (defaults: neutral) --------------------
    // Aggregated by GameRun over the owned cards. Capabilities (not context
    // pipelines) because both are commutative — a product of factors and a sum
    // of bonuses need no dispatch ordering.

    // Multiplies how many tiles spawn during board resolution each turn
    // (Vase of Two = 2; copies multiply: two cards = x4). 0 is legal and means
    // "no spawns at all". Aggregated by GameRun::getSpawnCountPerTurn.
    virtual int spawnCountFactor() const { return 1; }

    // Turns an Hourglass use rewinds BECAUSE OF this effect (Back to Back = 3).
    // Copies stack their FULL effect — the game's card-stacking rule — so the
    // total is the SUM over cards, REPLACING the base single rewind: no cards
    // = 1, one Back to Back = 3, two = 6 ("as if you used 6 Hourglasses").
    // Aggregated by GameRun::getRewindDepth.
    virtual int hourglassRewinds() const { return 0; }

    // --- Presentation hints (defaults: none) --------------------------------
    // Optional slot skin: texture id the owning slot adopts while this effect is
    // mounted (nullptr = leave the slot's look unchanged). Each slot TYPE (shop,
    // dark shop, event...) carries its own look here instead of Slot::addEffect
    // hardcoding one. Chips will likely want an overlay, not a replacement —
    // design that with the mounting layer.
    virtual const char* slotTextureId() const { return nullptr; }

    // Optional tile marker: texture id drawn semi-transparent OVER the owning
    // tile while this effect is present (nullptr = none). Brick/frozen show the
    // brick marker; future tags (golden, locked...) bring their own.
    virtual const char* overlayTextureId() const { return nullptr; }

    // Optional one-line status surfaced in the boss-fight banner while this
    // effect is mounted (empty = nothing to show). The boss aggregates it
    // (Boss::statusText) so a phase mechanic can explain itself — today the
    // Sleeper's asleep/awake countdown. Data only (a string, no render types),
    // so it honors the headless boundary like the texture-id hints above.
    virtual std::string statusText() const { return {}; }
};
