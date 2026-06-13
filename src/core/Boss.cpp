#include "core/Boss.h"
#include "core/Tile.h"

Boss::Boss(const BossDef& def, Coord anchor)
    : defId(def.id),
      name(def.name),
      textureId(def.textureId),
      hp(def.baseHp),
      maxHp(def.baseHp),
      footprint{anchor},
      resolveIncomingFn(def.resolveIncoming),
      onTurnActionFn(def.onTurnAction),
      onDefeatFn(def.onDefeat)
{
    // Mount the def's initial effects, fresh per spawn (the registry holds
    // factories, not live effects — same as a clone-pure Boss copies lambdas
    // out of the def rather than pointing back at it).
    for (const auto& factory : def.effects) {
        if (factory) effects.push_back(factory());
    }
}

Boss::Boss(const Boss& other)
    : defId(other.defId),
      name(other.name),
      textureId(other.textureId),
      hp(other.hp),
      maxHp(other.maxHp),
      footprint(other.footprint),
      resolveIncomingFn(other.resolveIncomingFn),
      onTurnActionFn(other.onTurnActionFn),
      onDefeatFn(other.onDefeatFn)
{
    // Deep-clone each effect (NOT re-instantiate from the def): clone()
    // preserves runtime state, so the Sleeper's phase counter rewinds with the
    // turn instead of resetting. Mirrors Slot's copy ctor.
    effects.reserve(other.effects.size());
    for (const auto& effect : other.effects) {
        effects.push_back(effect->clone());
    }
}

bool Boss::occupies(Coord c) const {
    for (const Coord& cell : footprint) {
        if (cell == c) return true;
    }
    return false;
}

IncomingResolution Boss::resolveIncoming(const Tile& attacker) const {
    if (resolveIncomingFn) {
        return resolveIncomingFn(*this, attacker);
    }
    // The pinned default (boss-design §3): generous — every contact is a Hit
    // for the attacker's own value. Content RESTRICTS from here; the engine
    // never needs to know how.
    return {IncomingResolution::Kind::Hit, attacker.getValue()};
}

void Boss::runTurnAction(EffectContext& ctx) {
    if (onTurnActionFn) {
        onTurnActionFn(*this, ctx);
    }
}

void Boss::runDefeat(EffectContext& ctx) {
    if (onDefeatFn) {
        onDefeatFn(*this, ctx);
    }
    // Default: nothing extra — the footprint cells are freed intact when the
    // caller removes the body (boss-design §4).
}

std::string Boss::statusText() const {
    // First non-empty wins: one phase mechanic at a time today, and a single
    // line is all the scrappy banner shows.
    for (const auto& effect : effects) {
        std::string s = effect->statusText();
        if (!s.empty()) return s;
    }
    return {};
}
