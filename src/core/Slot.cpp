#include "core/Slot.h"
#include "core/Board.h"
#include "rendering/RenderSystem.h"




Slot::Slot(int col, int row, Board* board, RenderSystem& renderer) :
    renderer(renderer),
    slot(renderer.getTextureManager().get("slot")),
    col(col),
    row(row),
    board(board)
{ 
    fixVisualAssets();
}

Slot::Slot(const Slot& other, Board* newBoard) :
    renderer(other.renderer),
    slot(other.slot),
    board(newBoard),
    col(other.col),
    row(other.row),
    canTileStepIn(other.canTileStepIn),
    canTileStepOut(other.canTileStepOut)
{
    // Copia profonda degli effetti
    for (const auto& effect : other.effects) {
        effects.push_back(effect->clone());
    }

    // Nota: la tile non viene copiata qui, viene gestita separatamente dalla Board
}

void Slot::fixVisualAssets() {

    //asset resizing
    renderer.resizeSprite("slot", slot);
    //asset origin setting

    slot.setOrigin(slot.getLocalBounds().getCenter());

    //asset positioning
    auto windowSize = renderer.getWindowSize();

    //slot.setPosition({ float(windowSize.x) / 2, float(windowSize.y) / 2 }); //  up shift (-) 

    const float shift = 300.f;
    slot.setPosition({ 198.f * col + shift, 198.f * row + shift });

    std::cout << "Slot visual assets: ready" << std::endl;
}

void Slot::render(RenderSystem& renderer) {

    renderer.draw(slot);

    if (!isEmpty()) {
        tile->render(renderer);
    }
}

bool Slot::isEmpty() const {
    return tile == nullptr;
}

Coord Slot::getCoord() const {
    return { col, row };
}

void Slot::setTile(std::unique_ptr<Tile> newTile) {
    tile = std::move(newTile);  // Takes ownership using move semantics
}

std::unique_ptr<Tile> Slot::releaseTile() {
    auto t = std::move(tile);
    removeTile();
    return t;
}


void Slot::removeTile() {
    tile.reset();  // Properly destroys the tile
}

void Slot::addEffect(std::unique_ptr<SlotEffect> effect) {

    //this function has no longer access to slot's sprite
    
    effects.push_back(std::move(effect));

    //this only adds the shop effect rn
    //Effect -> toString()
    sf::Sprite spriteload(renderer.getTextureManager().get("shopslot"));

    spriteload.setOrigin(slot.getOrigin());
    spriteload.setScale(slot.getScale());
    spriteload.setPosition(slot.getPosition());

    slot = spriteload;

}

void Slot::triggerMergeEffects() {
    for (auto& effect : effects) {
        effect->onMerge(this);
    }
}

//GETTERS


sf::Sprite& Slot::getSlotSprite() {

    return slot;

}
