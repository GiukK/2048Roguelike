#include "effects/EffectContext.h"
#include "core/Board.h"
#include "core/GameRun.h"

int EffectContext::coins() const {
    return run_.getCoins();
}

void EffectContext::addCoins(int amount, Slot* source) {
    run_.addCoins(amount, source);
}

void EffectContext::destroyTile(Tile* tile) {
    board_.destroyTile(tile);
}

Tile* EffectContext::spawnTile(Coord at, int value) {
    return board_.spawnTileAt(at, value);
}
