#pragma once

#include <functional>
#include <memory>
#include <utility>
#include "effects/Effect.h"

// Cards: run-scoped reactors held on GameRun, observing each completed turn's
// event log (design doc §9). A card is just an Effect whose reactor hooks are
// implemented; it lives outside the board, so it persists through undo and is
// never cloned ("the player persists").
//
// ReactorCard is the generic data+lambda substrate (the ItemRegistry pattern):
// concrete card content becomes an id + handlers in the future CardRegistry,
// with no new control flow per card. A card needing its own STATE (counters,
// once-per-turn latches) subclasses Effect directly instead.
class ReactorCard : public Effect {
public:
    using EventHandler   = std::function<void(const TurnEvent&, EffectContext&)>;
    using TurnEndHandler = std::function<void(const TurnLog&, EffectContext&)>;

    explicit ReactorCard(EventHandler onEvt, TurnEndHandler onEnd = nullptr)
        : onEvt(std::move(onEvt)), onEnd(std::move(onEnd)) {}

    void onEvent(const TurnEvent& event, EffectContext& ctx) override {
        if (onEvt) onEvt(event, ctx);
    }
    void onTurnEnd(const TurnLog& log, EffectContext& ctx) override {
        if (onEnd) onEnd(log, ctx);
    }
    std::unique_ptr<Effect> clone() const override {
        return std::make_unique<ReactorCard>(*this);
    }

private:
    EventHandler   onEvt;
    TurnEndHandler onEnd;
};

// Capability-only cards: passive run-scope stats the model aggregates (no
// hooks, no handlers). Parametrized so future variants are registry entries,
// not new classes ("Vase of Three" = SpawnCountCard(3)).

// Multiplies the per-turn spawn count (Vase of Two).
class SpawnCountCard : public Effect {
public:
    explicit SpawnCountCard(int factor) : factor(factor) {}
    int spawnCountFactor() const override { return factor; }
    std::unique_ptr<Effect> clone() const override {
        return std::make_unique<SpawnCountCard>(*this);
    }
private:
    int factor;
};

// Adds extra turns to an Hourglass rewind (Back to Back).
class RewindDepthCard : public Effect {
public:
    explicit RewindDepthCard(int bonus) : bonus(bonus) {}
    int rewindDepthBonus() const override { return bonus; }
    std::unique_ptr<Effect> clone() const override {
        return std::make_unique<RewindDepthCard>(*this);
    }
private:
    int bonus;
};
