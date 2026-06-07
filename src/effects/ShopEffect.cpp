#include "effects/ShopEffect.h"
#include "core/Slot.h"
#include "core/Board.h"
#include "core/Turn.h"

void ShopEffect::onMerge(Slot* slot) {
    slot->board->turn->requestShop();
}

std::unique_ptr<SlotEffect> ShopEffect::clone() const {
    return std::make_unique<ShopEffect>(*this);
}
