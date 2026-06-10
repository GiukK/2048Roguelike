#pragma once

#include "effects/Effect.h"

class ShopEffect : public Effect {
public:
    void onMerge(Slot* slot) override;
    std::unique_ptr<Effect> clone() const override;

    // True once a tile has merged into this shop's slot (the shop has been
    // "used"). The board keeps a triggered shop around for the rest of the
    // current turn, then removes it before the next turn — see
    // Board::removeConsumedShops. The flag is value-copied by clone(), so it
    // survives the turn-to-turn board cloning that powers the undo stack.
    bool isTriggered() const { return triggered; }

private:
    bool triggered = false;
};
