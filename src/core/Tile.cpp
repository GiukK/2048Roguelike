#include "core/Tile.h"
#include "core/Slot.h"
#include "rendering/RenderSystem.h"

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

void Tile::mergeIntoSlot(Slot* target) {
    int sum = value + target->tile->getValue();

    target->removeTile();
    changeSlot(slot, target);
    target->tile->setValue(sum);
    target->tile->changeSprite();
    target->triggerMergeEffects();

    mergedThisSweep = true;
}
