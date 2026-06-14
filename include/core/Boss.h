#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "core/utils/Coord.h"
// Complete type: the boss OWNS its effects (vector<unique_ptr<Effect>>), so
// every TU that destroys a Boss needs Effect's destructor, and findEffect's
// dynamic_cast needs the full type.
#include "effects/Effect.h"

class Tile;
class Boss;
class EffectContext;

// How an incoming tile resolves against a boss-occupied cell — the closed
// outcome vocabulary movement resolution APPLIES without ever knowing which
// boss (or future occupant) answered (docs/boss-design.md §3).
struct IncomingResolution {
    enum class Kind {
        Block,  // the tile stops before the cell (a wall)
        Hit     // the tile is consumed, the boss takes proposedDamage
    };
    Kind kind = Kind::Block;
    // The boss's own damage formula (attacker's value, flat 1, 0, ...). The
    // modifier pipeline (AttackContext) may still scale it before it applies.
    int proposedDamage = 0;
};

// One boss's content: display data + HP + behavior lambdas — the CardDef
// pattern applied to bosses (boss-design §5). A new boss is a registry entry,
// never new control flow in Board/GameRun; null lambdas mean the pinned
// defaults below.
struct BossDef {
    std::string id;
    std::string name;
    std::string textureId;
    int baseHp = 1;  // per-ante scaling joins with the ante machine (slice 4)

    // How an incoming tile resolves against this boss. Null = the DEFAULT
    // (boss-design §3, default-generous): Hit for the attacker's own value —
    // "high HP, damaged by everything" (the Brute) IS the default; other
    // bosses RESTRICT from there (target values, vulnerability windows).
    std::function<IncomingResolution(const Boss&, const Tile&)> resolveIncoming;

    // Per-turn boss action, run at the end of each turn during BossFight
    // (after the player's move resolves, before the kill check). Null =
    // passive (the Brute default). Routed through EffectContext so every
    // mutation is interceptable and logged.
    std::function<void(Boss&, EffectContext&)> onTurnAction;

    // Death effect (boss-design §4), run after BossDefeated is logged and
    // before the body is removed — its consequences (loot, scars) land after
    // the defeat in the log. Null = default: nothing extra; the footprint
    // cells are freed intact by the removal itself.
    std::function<void(Boss&, EffectContext&)> onDefeat;

    // Effects mounted on the boss at spawn — the boss as effect OWNER
    // (boss-design §2). Each factory builds one Effect carrying per-boss STATE
    // (the Sleeper's phase counter); the boss instantiates them in its ctor and
    // deep-clones them on the turn clone, so the state rewinds with the body.
    // Empty = a stateless boss (the Brute). Behavior reads this via findEffect.
    std::vector<std::function<std::unique_ptr<Effect>()>> effects;
};

// The boss BODY: a first-class board-resident entity (boss-design §2) — a
// footprint over existing slots, HP, and the behavior copied out of its def.
// It lives on Board, so it CLONES with the turn: HP (and any future per-boss
// state) rewinds with the world like every board state (§7).
//
// HEADLESS by the core boundary rule: no render types in here, ever —
// Board::render draws the body from textureId, PlayUI reads the banner data
// through GameRun.
//
// Self-contained on purpose: it copies what it needs from the def (name,
// texture, lambdas) instead of pointing back into the registry, so a cloned
// board never carries a pointer whose lifetime it doesn't control.
class Boss {
public:
    Boss(const BossDef& def, Coord anchor);  // 1x1 footprint; shapes are content (slice 6)

    // Deep-copy clone — the boss owns unique_ptr effects, so the implicit copy
    // ctor is gone; this one clones each effect via Effect::clone (mirroring
    // Slot's copy ctor) so per-boss STATE rewinds with the board (boss-design
    // §7). Used by Board::copyStateFrom on every turn clone.
    Boss(const Boss& other);

    bool occupies(Coord c) const;

    // The occupant decision point (§3): asked by movement resolution when a
    // tile would enter a boss-occupied cell. Const — DECIDING is not applying;
    // the caller routes a Hit through the AttackContext pipeline and applies
    // the final outcome. hasLegalMove asks it too (an answerable boss is the
    // escape valve on a packed board, §8).
    IncomingResolution resolveIncoming(const Tile& attacker) const;

    // Applied by Board's attack resolution with the PIPELINE'S final damage —
    // never call with the raw proposal. HP may go below zero; display clamps.
    void takeDamage(int amount) { hp -= amount; }

    // Runs the def's per-turn action (or no-op if passive). Called by
    // GameRun at end of turn during BossFight, before the kill check.
    void runTurnAction(EffectContext& ctx);

    // Runs the def's death effect (or the default no-op). Called by Board's
    // defeat resolution after BossDefeated is logged, before removal.
    void runDefeat(EffectContext& ctx);

    const std::string& getDefId() const { return defId; }
    const std::string& getName() const { return name; }
    const std::string& getTextureId() const { return textureId; }

    // The body's CURRENT look: the first mounted effect that declares a body
    // texture wins (the Sleeper's asleep/awake art), else the def's base
    // texture. Visual twin of statusText() — Board::render draws this, so a
    // phase mechanic changes the sprite from its own state with no render-side
    // type-switching on concrete effects. Headless: returns an id, not a sprite.
    std::string currentTextureId() const;
    int getHp() const { return hp; }
    int getMaxHp() const { return maxHp; }
    const std::vector<Coord>& getFootprint() const { return footprint; }

    // First mounted effect of concrete type E, or nullptr — the boss's own
    // typed lookup (mirrors Slot::findEffect). The boss's def lambdas read
    // their state through this (the Sleeper's resolveIncoming asks its
    // SleeperState whether it is vulnerable). Const: deciding is not mutating.
    template <typename E>
    E* findEffect() const {
        for (const auto& effect : effects) {
            if (auto* typed = dynamic_cast<E*>(effect.get())) return typed;
        }
        return nullptr;
    }

    // The one-line phase status for the fight banner — the first non-empty
    // statusText among the mounted effects, or "" (the Brute shows nothing).
    // PlayUI reads it through GameRun (the countdown-display pattern).
    std::string statusText() const;

private:
    std::string defId;
    std::string name;
    std::string textureId;
    int hp;
    int maxHp;
    std::vector<Coord> footprint;

    std::function<IncomingResolution(const Boss&, const Tile&)> resolveIncomingFn;
    std::function<void(Boss&, EffectContext&)> onTurnActionFn;
    std::function<void(Boss&, EffectContext&)> onDefeatFn;

    // Effects mounted on the boss (boss-design §2): per-boss state that clones
    // and rewinds with the body. Populated from BossDef::effects in the ctor.
    std::vector<std::unique_ptr<Effect>> effects;
};
