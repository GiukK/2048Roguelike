#pragma once

#include <memory>

class Slot;

class SlotEffect {
public:
    virtual ~SlotEffect() = default;
    virtual void onMerge(Slot* slot) = 0;
    virtual std::unique_ptr<SlotEffect> clone() const = 0;
};
