#pragma once

#include <memory>
#include "effects/Effect.h"

// The tile-tag family: per-tile state expressed as tile-scoped Effects instead
// of ad-hoc bool flags. Each tag declares its own persistence (isPersistent)
// and capabilities, so the board clone path and the movement rules never need
// per-tag special cases. Future tags (golden, locked, multiplier...) join here.

// Applied by the "brick" item. The tile is immovable until a merge clears it:
// being merged INTO destroys the tile, and the brick dies with it. Survives the
// turn-to-turn board clone (persistent by default) — a brick stays until broken.
class BrickEffect : public Effect {
public:
    std::unique_ptr<Effect> clone() const override {
        return std::make_unique<BrickEffect>(*this);
    }
    bool immobilizesOwner() const override { return true; }
    const char* overlayTextureId() const override { return "brick"; }
};

// Carried by the tile produced when something merges into a bricked tile: the
// brick breaks during that merge, but the result stays immobile (and marked)
// for the REST of that turn. Transient: isPersistent() == false means the clone
// path simply drops it, so the tile is a normal movable tile again from the
// next turn — with no extra event firing then.
class FrozenEffect : public Effect {
public:
    std::unique_ptr<Effect> clone() const override {
        return std::make_unique<FrozenEffect>(*this);
    }
    bool isPersistent() const override { return false; }
    bool immobilizesOwner() const override { return true; }
    const char* overlayTextureId() const override { return "brick"; }
};
