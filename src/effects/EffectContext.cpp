#include "effects/EffectContext.h"
#include "core/Board.h"
#include "core/GameRun.h"
#include "core/TurnLog.h"

int EffectContext::coins() const {
    return run_.getCoins();
}

void EffectContext::addCoins(int amount, Slot* source) {
    noteActivation();
    run_.addCoins(amount, source);
}

void EffectContext::destroyTile(Tile* tile) {
    noteActivation();
    board_.destroyTile(tile);
}

Tile* EffectContext::spawnTile(Coord at, int value) {
    noteActivation();
    return board_.spawnTileAt(at, value);
}

void EffectContext::addItem(const std::string& itemId) {
    noteActivation();
    run_.addItem(itemId);
}

void EffectContext::beginCardDispatch(const std::string& cardId) {
    activeCardId = &cardId;
    activationLogged = false;
}

void EffectContext::endCardDispatch() {
    activeCardId = nullptr;
    activationLogged = false;
}

void EffectContext::noteActivation() {
    // Outside the onEvent phase (onTurnEnd, or no dispatch in flight) there is
    // nothing to attribute — end-of-turn reactions are not "activations".
    if (!activeCardId || activationLogged) return;
    activationLogged = true;
    log_.push(TurnEvent::cardTriggered(*activeCardId));
}
