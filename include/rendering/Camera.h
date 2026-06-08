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

    void         setCenter(sf::Vector2f c) { center = c; }
    sf::Vector2f getCenter() const         { return center; }

    void  setZoomLevel(float level);          // clamped to [Min, Max]
    float getZoomLevel() const { return zoomLevel; }

    // View mapping board-space -> window. A smaller zoom level (or larger window)
    // shows more of the board: view size = windowSize / zoomLevel.
    sf::View getView(sf::Vector2u windowSize) const;

private:
    sf::Vector2f center{0.f, 0.f};
    float        zoomLevel = MaxZoomLevel;  // start fully zoomed-in (native)
};
