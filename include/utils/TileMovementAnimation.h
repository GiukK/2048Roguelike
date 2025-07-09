#pragma once

#include <iostream>
#include <SFML/Graphics.hpp>

#pragma once
#include <SFML/Graphics.hpp>
#include <memory>
#include <algorithm>


class TileMovementAnimation {
private:
    sf::Vector2f startPos;
    sf::Vector2f endPos;
    float elapsedTime = 0.0f;
    float duration = 0.2f; // in seconds
    std::shared_ptr<sf::Sprite> tile;

    static sf::Vector2f lerp(const sf::Vector2f& a, const sf::Vector2f& b, float t) {
        return a + (b - a) * t;
    }
public:

    TileMovementAnimation(
        const sf::Vector2f& from,
        const sf::Vector2f& to,
        const std::shared_ptr<sf::Sprite>& tilePtr,
        float durationSeconds = 0.2f
    )
        : startPos(from), endPos(to), tile(tilePtr), duration(durationSeconds)
    {}

    void update(float deltaTime) {
        if (isDone()) return;

        elapsedTime += deltaTime;
        float t = std::min(elapsedTime / duration, 1.0f);
        sf::Vector2f currentPos = lerp(startPos, endPos, t);

        if (tile)
            tile->setPosition(currentPos);
    }

    void draw(sf::RenderTarget& target) const {
        if (tile)
            target.draw(*tile);
    }

    bool isDone() const {
        return elapsedTime >= duration;
    }


};

