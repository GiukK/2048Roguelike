#include "effects/ShopEffect.h"
#include "core/Slot.h"
#include "core/Board.h"
#include "core/Turn.h"

void ShopEffect::onMerge(Slot* slot) {
    // Mark the shop used so the board can remove it next turn, and ask the turn
    // to open the shop UI. Both happen on the live board; the triggered flag is
    // preserved through cloning so the lifecycle survives the undo stack.
    triggered = true;
    slot->board->turn->log().push(TurnEvent::shopTriggered(slot->getCoord()));
    slot->board->turn->requestShop();
}

std::unique_ptr<SlotEffect> ShopEffect::clone() const {
    return std::make_unique<ShopEffect>(*this);
}
