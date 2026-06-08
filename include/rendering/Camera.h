#pragma once

#include <SFML/Graphics.hpp>

// Board camera: a pan/zoom window onto board-space, realised as an sf::View.
//
// Phase 1 uses only `center` (fixed native zoom). Zoom (scroll wheel) and pan
// (left-drag) will drive this same state in later phases. `center` is left
// intentionally UNCLAMPED: a future "elastic snap-back" is meant to move it
// freely during a drag and spring it back toward the board on release, so any
// bounds/clamping logic belongs in that controller, not here.
class Camera {
public:
    // Zoom is a level where 1.0 renders board content at its native (drawn)
    // size — the most zoomed-IN the player may go. Smaller values zoom out
    // (content shrinks); MinZoomLevel caps how far. Tweak the bounds here; do
    // not hardcode them at call sites.
    static constexpr float MaxZoomLevel = 1.0f;  // most zoomed-in (native size)
    static constexpr float MinZoomLevel = 0.5f;  // most zoomed-out (half size)

    // Multiplicative zoom change per wheel notch (tweak feel here). The actual
    // factor applied is ZoomStepPerNotch ^ wheelDelta, so it scales smoothly with
    // high-precision/multi-notch scrolls.
    static constexpr float ZoomStepPerNotch = 1.1f;

    // Sets the center directly and CANCELS any running snap animation — this is
    // the path used by manual pan/zoom, so grabbing or zooming overrides a snap.
    void         setCenter(sf::Vector2f c) { center = c; animating = false; }
    sf::Vector2f getCenter() const         { return center; }

    void  setZoomLevel(float level);          // clamped to [Min, Max]
    float getZoomLevel() const { return zoomLevel; }

    // --- Elastic snap-back -------------------------------------------------
    // Springs the center toward a target with an elastic ease (same family as
    // the tile slide). update(dt) must be ticked each frame to advance it.
    void animateTo(sf::Vector2f target);
    // Clamps the center into `bounds` and animates there if it was outside. Used
    // to snap the board back under the screen centre after a pan drifts off it.
    void snapCenterInto(sf::FloatRect bounds);
    void update(float dt);
    bool isAnimating() const { return animating; }

    // View mapping board-space -> window. A smaller zoom level (or larger window)
    // shows more of the board: view size = windowSize / zoomLevel.
    sf::View getView(sf::Vector2u windowSize) const;

private:
    sf::Vector2f center{0.f, 0.f};
    float        zoomLevel = MaxZoomLevel;  // start fully zoomed-in (native)

    // Snap-back animation state.
    static constexpr float SnapDuration = 0.4f;  // seconds; tweak the snap feel
    sf::Vector2f startCenter{0.f, 0.f};
    sf::Vector2f targetCenter{0.f, 0.f};
    float        animTime = 0.f;
    bool         animating = false;
};
