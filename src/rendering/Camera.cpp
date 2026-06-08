#include "rendering/Camera.h"

#include <algorithm>

void Camera::setZoomLevel(float level) {
    zoomLevel = std::clamp(level, MinZoomLevel, MaxZoomLevel);
}

sf::View Camera::getView(sf::Vector2u windowSize) const {
    const sf::Vector2f size{ static_cast<float>(windowSize.x) / zoomLevel,
                             static_cast<float>(windowSize.y) / zoomLevel };
    return sf::View(center, size);
}
