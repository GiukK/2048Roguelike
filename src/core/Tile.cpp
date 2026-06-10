#include "core/Tile.h"
#include "core/Slot.h"
#include "core/Board.h"
#include "core/Turn.h"
#include "core/GameRun.h"
#include "effects/MergeContext.h"
#include "effects/TileTags.h"
#include "rendering/RenderSystem.h"

#include <algorithm>
#include <cmath>

Tile::Tile(RenderSystem& renderer, Slot* slot, int value)
    : renderer(renderer),
      slot(slot),
      value(value),
      sprite(renderer.getTextureManager().get(std::to_string(value)))
{
    initVisuals();
}

void Tile::initVisuals() {
    sprite.setOrigin({sprite.getGlobalBounds().size.x / 2.f,
                      sprite.getGlobalBounds().size.y / 2.f});
    sprite.setPosition(slot->getSlotSprite().getPosition());
    renderer.resizeSprite("2", sprite);
    baseScale = sprite.getScale();
}

void Tile::render(RenderSystem& renderer) {
    applyVisualStyle();
    renderer.draw(sprite);

    // Effects may declare a marker drawn over the tile (the brick today). The
    // first declared one wins; the sprite is rebuilt only when the id changes.
    const char* wanted = nullptr;
    for (const auto& effect : effects) {
        if ((wanted = effect->overlayTextureId())) break;
    }
    if (wanted) {
        if (!overlay || overlayTexture != wanted) {
            // Drawn semi-transparent so the tile's value stays readable under it.
            overlay = std::make_unique<sf::Sprite>(
                renderer.getTextureManager().get(wanted));
            overlay->setOrigin(overlay->getLocalBounds().getCenter());
            overlay->setColor(sf::Color(255, 255, 255, 150));
            overlayTexture = wanted;
        }
        // Marker textures share the tile's native size, so matching the tile's
        // current scale makes the overlay cover it exactly — including the hover
        // zoom applied in applyVisualStyle() and any in-progress slide.
        overlay->setScale(sprite.getScale());
        overlay->setPosition(sprite.getPosition());
        renderer.draw(*overlay);
    }
}

void Tile::addEffect(std::unique_ptr<Effect> effect) {
    effects.push_back(std::move(effect));
}

bool Tile::isImmobilized() const {
    for (const auto& effect : effects) {
        if (effect->immobilizesOwner()) return true;
    }
    return false;
}

void Tile::setBricked(bool b) {
    if (b == isBricked()) return;
    if (b) {
        effects.push_back(std::make_unique<BrickEffect>());
    } else {
        effects.erase(std::remove_if(effects.begin(), effects.end(),
                          [](const std::unique_ptr<Effect>& e) {
                              return dynamic_cast<BrickEffect*>(e.get()) != nullptr;
                          }),
                      effects.end());
    }
}

bool Tile::isBricked() const {
    return findEffect<BrickEffect>() != nullptr;
}

bool Tile::isFrozenThisTurn() const {
    return findEffect<FrozenEffect>() != nullptr;
}

void Tile::applyVisualStyle() {
    float scaleMul = 1.f;
    sf::Color color = sf::Color::White;

    if (selected) {
        color = (visualState == Visual::Hovered)
            ? sf::Color(255, 120, 120)
            : sf::Color::Red;
    } else {
        switch (visualState) {
        case Visual::Hovered: color = sf::Color(200, 200, 200); break;
        case Visual::Pressed: color = sf::Color(160, 160, 160); break;
        default: break;
        }
    }

    if (visualState == Visual::Hovered || visualState == Visual::Pressed) {
        scaleMul = 1.1f;
    }

    sprite.setScale(baseScale * scaleMul);
    sprite.setColor(color);
}

void Tile::setVisual(Visual state) {
    visualState = state;
}

void Tile::setSelected(bool sel) {
    selected = sel;
}

void Tile::update(float deltaTime) {
    if (!animating) return;

    animTime += deltaTime;
    float t = animTime / animDuration;

    if (t >= 1.f) {
        sprite.setPosition(targetPos);
        animating = false;
        return;
    }

    float p = 1.f;
    float s = p / 4.f;
    float elastic = std::pow(2.f, -12.f * t)
                  * std::sin((t - s) * (2.f * 3.14159f) / p) + 1.f;

    sprite.setPosition(startPos + (targetPos - startPos) * elastic);
}

sf::Vector2f Tile::getPosition() const {
    return sprite.getPosition();
}

sf::FloatRect Tile::getBounds() const {
    return sprite.getGlobalBounds();
}

int Tile::getValue() const {
    return value;
}

void Tile::setValue(int newValue) {
    value = newValue;
}

void Tile::changeSprite() {
    sprite.setTexture(renderer.getTextureManager().get(std::to_string(value)));
}

void Tile::changeSlot(Slot* from, Slot* to) {
    slot = to;
    to->setTile(from->releaseTile());

    startPos = sprite.getPosition();
    targetPos = to->getSlotSprite().getPosition();
    animTime = 0.f;
    animating = true;
}

void Tile::animateTo(sf::Vector2f target) {
    startPos = sprite.getPosition();
    targetPos = target;
    animTime = 0.f;
    animating = true;
}

void Tile::mergeIntoSlot(Slot* target) {
    // Capture the immutable inputs of this merge NOW: after changeSlot() this tile
    // becomes target->tile and setValue() below overwrites `value` with the result,
    // so reading `value` later would report the merged value, and the target tile
    // we read here is destroyed by removeTile().
    const int sourceValue = value;
    // Was the tile we merge into bricked? If so the brick breaks now (this turn),
    // but the resulting tile must stay put for the rest of the turn rather than
    // become movable.
    const bool targetWasBricked = target->tile->isBricked();

    // The merge's MUTABLE outcome. Threaded through the target slot's effects so a
    // chip / special slot may change it before it is applied and logged. Default
    // resultValue = the two tiles summed; no effect changes it today, so behavior
    // is unchanged. Run the pipeline BEFORE applying so modifiers act on the value
    // that actually lands on the tile and in the event.
    MergeContext merge{target, sourceValue, value + target->tile->getValue()};
    target->resolveMerge(merge);

    target->removeTile();
    changeSlot(slot, target);
    target->tile->setValue(merge.resultValue);
    target->tile->changeSprite();

    if (targetWasBricked) {
        // Transient tag (FrozenEffect::isPersistent == false): the clone path
        // drops it, so the tile is normal again next turn with no extra event.
        // The brick-break event stays in this turn.
        target->tile->addEffect(std::make_unique<FrozenEffect>());
    }

    // Log the merge at its action site with the FINAL (post-modifier) value, in the
    // acting turn. Emitted exactly once per merge: Board::move resolves once per
    // input (one sweep), and mergedThisSweep blocks a second merge within it.
    // `flag` records the brick-break so consumers can react without re-deriving.
    if (target->board && target->board->turn) {
        target->board->turn->log().push(
            TurnEvent::tileMerged(merge.resultValue, sourceValue, target->getCoord(), targetWasBricked));
    }

    // Coin reward accumulated by the merge modifiers, routed through the coin
    // pipeline AFTER the merge applied and logged (cause before consequence in
    // the log) — so coin chips on this slot scale what merge chips granted.
    if (merge.coinReward != 0 && target->board && target->board->turn) {
        target->board->turn->gameRun->addCoins(merge.coinReward, target);
    }

    mergedThisSweep = true;
}
