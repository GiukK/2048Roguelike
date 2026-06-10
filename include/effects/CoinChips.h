#pragma once

#include <memory>
#include "effects/Effect.h"
#include "effects/CoinContext.h"
#include "effects/MergeContext.h"

// The first chips: slot-scoped coin economy modifiers. A "chip" is nothing but
// an Effect mounted on a slot (or, later, the board) — see the design doc §2.2.
// No mounting UI exists yet, so today they are attached programmatically
// (tests, debug); the player-facing mount/unmount layer is its own slice.

// "+N coins when a merge resolves on this slot." A merge-modifier: it adds to
// the merge's coinReward accumulator, which the merge site then routes through
// the coin pipeline (where multiplier chips may scale it further).
class CoinBonusChip : public Effect {
public:
    explicit CoinBonusChip(int bonus) : bonus(bonus) {}

    void onMergeResolving(MergeContext& merge) override {
        merge.coinReward += bonus;
    }
    std::unique_ptr<Effect> clone() const override {
        return std::make_unique<CoinBonusChip>(*this);
    }

private:
    int bonus;
};

// "×N every coin gain sourced over this slot." A coin-modifier: it scales the
// amount inside the pipeline, whatever granted it (merge chips today, future
// coin sources tomorrow). Mounted alongside a CoinBonusChip it multiplies the
// bonus — insertion order doesn't matter between the two, since they act on
// different stages (merge accumulation vs. coin resolution).
class CoinMultiplierChip : public Effect {
public:
    explicit CoinMultiplierChip(int factor) : factor(factor) {}

    void onCoinsResolving(CoinContext& coin) override {
        coin.amount *= factor;
    }
    std::unique_ptr<Effect> clone() const override {
        return std::make_unique<CoinMultiplierChip>(*this);
    }

private:
    int factor;
};
