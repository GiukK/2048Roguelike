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
      onDefeatFn(def.onDefeat)
{}

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

void Boss::runDefeat(EffectContext& ctx) {
    if (onDefeatFn) {
        onDefeatFn(*this, ctx);
    }
    // Default: nothing extra — the footprint cells are freed intact when the
    // caller removes the body (boss-design §4).
}
