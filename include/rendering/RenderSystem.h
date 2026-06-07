#pragma once

#include <SFML/Graphics.hpp>
#include <string>
#include <unordered_map>
#include "rendering/TextureManager.h"

class RenderSystem {
public:
    RenderSystem(sf::RenderWindow& window);

    void initialize(const sf::Vector2u& windowSize);
    void resizeSprite(const std::string& id, sf::Sprite& sprite);

    const TextureManager& getTextureManager() const;
    const sf::Vector2u& getWindowSize() const;
    sf::RenderWindow& getWindow();

    void draw(sf::Drawable& drawable);

    // Draws an integer using the "digits" spritesheet, horizontally centered at `center`.
    // Each digit is 5x7 px in the sheet, scaled by `scale`.
    void drawNumber(unsigned int value, sf::Vector2f center, float scale = 10.f);

    void close();

private:
    struct ScaleRule {
        float width;
        float height;
    };

    sf::RenderWindow& window;
    sf::Vector2u windowSize;
    TextureManager textureManager;
    std::unordered_map<std::string, ScaleRule> scalingRules;
};
