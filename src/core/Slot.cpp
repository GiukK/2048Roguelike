#include "core/Slot.h"
#include "core/Board.h"
#include "rendering/RenderSystem.h"

Slot::Slot(int col, int row, Board* board, RenderSystem& renderer)
    : col(col), row(row), board(board), renderer(renderer),
      sprite(renderer.getTextureManager().get("slot"))
{
    initVisuals();
}

Slot::Slot(const Slot& other, Board* newBoard)
    : col(other.col), row(other.row), board(newBoard), renderer(other.renderer),
      sprite(other.sprite),
      canTileStepIn(other.canTileStepIn),
      canTileStepOut(other.canTileStepOut)
{
    for (const auto& effect : other.effects) {
        effects.push_back(effect->clone());
    }
}

void Slot::initVisuals() {
    renderer.resizeSprite("slot", sprite);
    sprite.setOrigin(sprite.getLocalBounds().getCenter());

    const float offset = 300.f;
    const float slotSize = 128.f;
    sprite.setPosition({slotSize * col + offset, slotSize * row + offset});
}

void Slot::render(RenderSystem& renderer) {
    renderer.draw(sprite);
    if (!isEmpty()) {
        tile->render(renderer);
    }
}

void Slot::update(float deltaTime) {
    if (!isEmpty()) {
        tile->update(deltaTime);
    }
}

Coord Slot::getCoord() const {
    return {col, row};
}

sf::Sprite& Slot::getSlotSprite() {
    return sprite;
}

bool Slot::isEmpty() const {
    return tile == nullptr;
}

void Slot::setTile(std::unique_ptr<Tile> newTile) {
    tile = std::move(newTile);
}

void Slot::removeTile() {
    tile.reset();
}

std::unique_ptr<Tile> Slot::releaseTile() {
    return std::move(tile);
}

void Slot::addEffect(std::unique_ptr<Effect> effect) {
    // A mounted effect may re-skin its slot (shop, dark shop, ...): swap the
    // texture, keep the geometry. Effects without a skin leave the look alone,
    // so mounting a future chip here won't turn the slot into a shop.
    if (const char* skin = effect->slotTextureId()) {
        sf::Sprite skinned(renderer.getTextureManager().get(skin));
        skinned.setOrigin(sprite.getOrigin());
        skinned.setScale(sprite.getScale());
        skinned.setPosition(sprite.getPosition());
        sprite = skinned;
    }
    effects.push_back(std::move(effect));
}

void Slot::resolveMerge(MergeContext& merge) {
    // In effect-insertion order (deterministic). A slot carries at most a couple
    // of effects today; the documented tile->slot->board->run cross-scope order
    // is applied by the caller as those scopes arrive.
    for (auto& effect : effects) {
        effect->onMergeResolving(merge);
    }
}
