#pragma once

#include <string>

#include "effects/Effect.h"

// Effects mounted ON a boss — the boss as effect OWNER (boss-design §2). They
// carry per-boss STATE that clones and rewinds with the body (Boss deep-copies
// them via Effect::clone, like Slot does its own effects), so undo undoes boss
// progress with everything else. The boss's BEHAVIOR stays in the BossDef
// lambdas (content); these are the state those lambdas read and advance.

// The Sleeper's phase state (boss-design §5, catalogue): invincible for the
// first InvincibleTurns fight turns, then awake. Pure state — the Sleeper's
// def lambdas decide what asleep/awake MEANS (resolveIncoming answers Block
// while asleep, onTurnAction self-damages from adjacency once awake). Keeping
// the behavior out of here is the "state = effect, behavior = def" split that
// lets the engine stay boss-agnostic.
class SleeperState : public Effect {
public:
    static constexpr int InvincibleTurns = 3;

    std::unique_ptr<Effect> clone() const override {
        // Copies turnsAsleep — the whole point: the phase rewinds with the turn.
        return std::make_unique<SleeperState>(*this);
    }
    bool isPersistent() const override { return true; }

    // Asleep for the first InvincibleTurns fight turns, awake afterwards.
    bool isVulnerable() const { return turnsAsleep >= InvincibleTurns; }
    int turnsUntilWake() const {
        const int left = InvincibleTurns - turnsAsleep;
        return left > 0 ? left : 0;
    }

    // Advanced once per BossFight turn by the Sleeper's onTurnAction. Not a
    // reactor hook — the def drives it, so the engine needs no boss-turn pass.
    void advance() { ++turnsAsleep; }

    std::string statusText() const override {
        return isVulnerable()
            ? std::string("AWAKE - vulnerable")
            : "ASLEEP - invulnerable (" + std::to_string(turnsUntilWake()) + ")";
    }

    // The body's look follows the phase — the same asleep/awake split statusText
    // narrates, but for the sprite. Board::render reads it via Boss::currentTextureId,
    // so the two phase textures are pure content, no render-side boss-type checks.
    const char* bodyTextureId() const override {
        return isVulnerable() ? "sleeper_awake" : "sleeper_asleep";
    }

private:
    int turnsAsleep = 0;
};
