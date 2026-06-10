#include "effects/ShopEffect.h"
#include "effects/MergeContext.h"
#include "core/Slot.h"
#include "core/Board.h"
#include "core/Turn.h"

void ShopEffect::onMergeResolving(MergeContext& merge) {
    // A shop doesn't alter the merge outcome — it fires at merge time as a side
    // effect: mark the shop used so the board removes it next turn, and ask the
    // turn to open the shop UI. Both act on the live board; the triggered flag is
    // preserved through cloning so the lifecycle survives the undo stack.
    triggered = true;
    Slot* slot = merge.slot;
    slot->board->turn->log().push(TurnEvent::shopTriggered(slot->getCoord()));
    slot->board->turn->requestShop();
}

std::unique_ptr<Effect> ShopEffect::clone() const {
    return std::make_unique<ShopEffect>(*this);
}
