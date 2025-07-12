#include "core/ShopEffect.h"
#include "core/Slot.h"
#include "core/Board.h"
#include "core/Turn.h"

#include <iostream>

void ShopEffect::onMerge(Slot* slot) {
    std::cout << "Shop activated at Slot (" << slot->col << ", " << slot->row << ")" << std::endl;
    slot->board->turn->requestShop();  // delega tutto al Turn
}

std::unique_ptr<SlotEffect> ShopEffect::clone() const {
    return std::make_unique<ShopEffect>(*this);
}
