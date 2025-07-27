#pragma once

#include <memory>

class Slot;
class Turn;

class SlotEffect {
public:
    virtual ~SlotEffect() = default;

    // Activated when a tile is merged on top
    virtual void onMerge(Slot* slot) = 0;


    // Clone method for deep copying
    virtual std::unique_ptr<SlotEffect> clone() const = 0;
    // In the future: onStep(), onRemove(), etc.
};
