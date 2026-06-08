#pragma once

#include <SFML/Graphics.hpp>
#include <memory>

class Slot;
class RenderSystem;

class Tile {
public:
    enum class Visual { Idle, Hovered, Pressed };

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

    // Brick status (applied by the "brick" item). A bricked tile is immovable:
    // it cannot slide or initiate a merge (enforced in Board::resolveNextTileMove),
    // exactly like a shop slot. It can still be a merge *target* — being merged
    // into destroys the tile and therefore clears the brick. A semi-transparent
    // brick sprite is drawn over the tile so the player can recognise it.
    // The flag is copied across turn-to-turn board clones (see Board::copyStateFrom).
    void setBricked(bool b);
    bool isBricked() const { return bricked; }

    // Transient one-turn immobility carried by the tile produced when something
    // merges INTO a bricked tile. The brick breaks during that merge (so the
    // event belongs to the merge turn), but the resulting tile stays immobile —
    // and keeps the brick marker — for the rest of that turn instead of becoming
    // movable immediately. Unlike `bricked`, this flag is NOT copied into the
    // next turn's board clone (see Board::copyStateFrom), so the tile is a normal
    // movable tile again from the next turn, with no further event firing then.
    bool isFrozenThisTurn() const { return frozenThisTurn; }

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

    // Lazily created while the brick marker is shown (bricked OR frozenThisTurn);
    // re-synced to the tile's transform each frame in render() so it tracks hover
    // zoom and slide animations.
    bool bricked = false;
    bool frozenThisTurn = false;
    std::unique_ptr<sf::Sprite> brickOverlay;

    // slide animation
    sf::Vector2f startPos;
    sf::Vector2f targetPos;
    float animTime = 0.f;
    float animDuration = 0.1f;
    bool animating = false;
};
