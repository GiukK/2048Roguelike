#pragma once

#include <SFML/Graphics.hpp>
#include <string>
#include <unordered_map>
#include "rendering/TextureManager.h"


//------------------ VISUAL ASSET LOADING ------------------

//this class handles screen-window scaling for sprites
//
//to add a new asset remember to:

//1) add its scalingRules in RenderSystem->initialize() in .cpp
//2) add its loading in TextureManager->initialize() in .cpp

//-----------------------------------------------------------

class RenderSystem {
public:

    RenderSystem(sf::RenderWindow& window);

    void initialize(const sf::Vector2u& windowSize);
    void resizeSprite(const std::string& id, sf::Sprite& sprite);

    const TextureManager& getTextureManager() const;
    const sf::Vector2u& getWindowSize() const;
    sf::RenderWindow& getWindow();

    void draw(sf::Drawable& sprite);
    void close();

private:

    struct AssetScale {
        float width;
        float height;
    };

    sf::RenderWindow& window;

    sf::Vector2u windowSize;

    TextureManager textureManager;

    std::unordered_map<std::string, AssetScale> scalingRules;



};
