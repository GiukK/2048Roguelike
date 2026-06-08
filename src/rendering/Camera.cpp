#include "rendering/Camera.h"

#include <algorithm>
#include <cmath>

void Camera::setZoomLevel(float level) {
    zoomLevel = std::clamp(level, MinZoomLevel, MaxZoomLevel);
}

void Camera::animateTo(sf::Vector2f target) {
    // Already there (within a tiny epsilon) -> nothing to animate.
    const sf::Vector2f d = target - center;
    if (d.x * d.x + d.y * d.y < 0.01f) {
        animating = false;
        return;
    }
    startCenter = center;
    targetCenter = target;
    animTime = 0.f;
    animating = true;
}

void Camera::snapCenterInto(sf::FloatRect bounds) {
    const sf::Vector2f target{
        std::clamp(center.x, bounds.position.x, bounds.position.x + bounds.size.x),
        std::clamp(center.y, bounds.position.y, bounds.position.y + bounds.size.y)
    };
    animateTo(target);
}

void Camera::update(float dt) {
    if (!animating) return;

    animTime += dt;
    const float t = animTime / SnapDuration;
    if (t >= 1.f) {
        center = targetCenter;
        animating = false;
        return;
    }

    // Elastic-out easing (same shape as Tile::update): overshoots slightly past
    // the target — toward the board interior, since the target is the in-bounds
    // clamp — then settles. Writes `center` directly so it doesn't self-cancel.
    constexpr float p = 1.f;
    constexpr float s = p / 4.f;
    const float e = std::pow(2.f, -12.f * t)
                  * std::sin((t - s) * (2.f * 3.14159265f) / p) + 1.f;
    center = startCenter + (targetCenter - startCenter) * e;
}

sf::View Camera::getView(sf::Vector2u windowSize) const {
    const sf::Vector2f size{ static_cast<float>(windowSize.x) / zoomLevel,
                             static_cast<float>(windowSize.y) / zoomLevel };
    return sf::View(center, size);
}
