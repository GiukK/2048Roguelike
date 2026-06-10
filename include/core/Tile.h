#pragma once

#include <SFML/Graphics.hpp>
#include <memory>
#include <string>
#include <vector>

#include "effects/Effect.h"

class Slot;
class RenderSystem;

class Tile {
public:
    enum class Visual { Idle, Hovered, Pressed };

    // Highest value with artwork (textures stop at "2048"). Merges that would
    // exceed it are refused (Board::resolveNextTileMove), and any future modifier
    // touching MergeContext::resultValue must respect it — changeSprite() on an
    // unbacked value would throw on the missing texture.
    static constexpr int MaxValue = 2048;

    Tile(RenderSystem& renderer, Slot* slot, int value);

    void render(RenderSystem& renderer);
    void update(float deltaTime);

    sf::Vector2f getPosition() const;
    sf::FloatRect getBounds() const;
    int getValue() const;
    void setValue(int newValue);

    void changeSprite();
    void changeSlot(Slot* from, Slot* to);
    void mergeIntoSlot(Slot* target);
    void animateTo(sf::Vector2f target);

    bool isAnimating() const { return animating; }

    void setVisual(Visual state);
    void setSelected(bool sel);
    bool isSelected() const { return selected; }

    // --- Tile tags (per-tile state as tile-scoped Effects) -----------------
    // Per-tile state lives in `effects` (BrickEffect, FrozenEffect, ...): each
    // tag declares its own persistence and capabilities, and the board clone
    // path copies whatever isPersistent() — no per-flag special cases anywhere.
    // The methods below are the domain-vocabulary facade over that store.

    void addEffect(std::unique_ptr<Effect> effect);

    // First effect of concrete type E, or nullptr. The ONE typed lookup point —
    // capability questions should use the aggregate queries instead.
    template <typename E>
    E* findEffect() const {
        for (const auto& effect : effects) {
            if (auto* typed = dynamic_cast<E*>(effect.get())) return typed;
        }
        return nullptr;
    }

    // True if any effect immobilizes this tile (brick, frozen): it cannot slide
    // or initiate a merge (enforced in Board::resolveNextTileMove), exactly like
    // a shop slot. It can still be a merge *target*.
    bool isImmobilized() const;

    // Brick tag (applied by the "brick" item): persistent — survives the
    // turn-to-turn clone until a merge breaks it (merging into the tile destroys
    // it, brick included). setBricked toggles the BrickEffect tag.
    void setBricked(bool b);
    bool isBricked() const;

    // Frozen tag: transient one-turn immobility carried by the tile produced
    // when something merges INTO a bricked tile. The brick breaks during that
    // merge (the event belongs to the merge turn) but the result stays immobile
    // for the rest of it; the clone path drops the tag (isPersistent == false),
    // so the tile is normal again next turn with no further event firing then.
    bool isFrozenThisTurn() const;

    std::vector<std::unique_ptr<Effect>> effects;

    bool mergedThisSweep = false;
    Slot* slot;

private:
    void initVisuals();
    void applyVisualStyle();

    RenderSystem& renderer;
    sf::Sprite sprite;
    int value;

    Visual visualState = Visual::Idle;
    bool selected = false;
    sf::Vector2f baseScale;

    // Marker sprite for the first effect declaring an overlayTextureId (brick
    // today). Lazily (re)built when that id changes; re-synced to the tile's
    // transform each frame in render() so it tracks hover zoom and slides.
    std::unique_ptr<sf::Sprite> overlay;
    std::string overlayTexture;

    // slide animation
    sf::Vector2f startPos;
    sf::Vector2f targetPos;
    float animTime = 0.f;
    float animDuration = 0.1f;
    bool animating = false;
};
