#include "effects/ShopEffect.h"
#include "core/Slot.h"
#include "core/Board.h"
#include "core/Turn.h"
#include "core/GameRun.h"

#include <iostream>

void ShopEffect::onMerge(Slot* slot) {
    std::cout << "Shop merged at Slot (" << slot->col << ", " << slot->row << ")" << std::endl;
    slot->board->turn->requestShop();  // delega tutto al Turn

    int coin_gain = slot->tile->getValue();

    slot->board->turn->game_run->addCoins(coin_gain);

}

std::unique_ptr<SlotEffect> ShopEffect::clone() const {
    return std::make_unique<ShopEffect>(*this);
}
