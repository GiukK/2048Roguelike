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
    effects.push_back(std::move(effect));

    // swap sprite to the effect-specific texture
    sf::Sprite effectSprite(renderer.getTextureManager().get("shopslot"));
    effectSprite.setOrigin(sprite.getOrigin());
    effectSprite.setScale(sprite.getScale());
    effectSprite.setPosition(sprite.getPosition());
    sprite = effectSprite;
}

void Slot::triggerMergeEffects() {
    for (auto& effect : effects) {
        effect->onMerge(this);
    }
}
