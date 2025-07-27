#pragma once
#include "effects/SlotEffect.h"

class ShopEffect : public SlotEffect {
public:
    void onMerge(Slot* slot) override;

    std::unique_ptr<SlotEffect> clone() const override;
};
