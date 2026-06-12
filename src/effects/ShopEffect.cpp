#include "effects/ShopEffect.h"
#include "effects/MergeContext.h"
#include "core/Slot.h"
#include "core/Board.h"
#include "core/Turn.h"

void ShopEffect::onMergeApplied(const MergeContext& merge) {
    // The merge already applied and logged: mark the shop used so the board
    // removes it next turn, log the trigger (now correctly AFTER its TileMerged),
    // and ask the turn to open the shop UI. The triggered flag is preserved
    // through cloning so the lifecycle survives the undo stack.
    triggered = true;
    Slot* slot = merge.slot;
    slot->board->turn->log().push(TurnEvent::shopTriggered(slot->getCoord()));
    slot->board->turn->requestShop();
}

std::unique_ptr<Effect> ShopEffect::clone() const {
    return std::make_unique<ShopEffect>(*this);
}
