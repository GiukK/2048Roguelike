
#include <SFML/Graphics.hpp>
#include "rendering/RenderSystem.h"
#include <iostream>


RenderSystem::RenderSystem( sf::RenderWindow& window):
    window(window)
{ }


void RenderSystem::initialize(const sf::Vector2u& size) {
    windowSize = size; //stores window size for screen scaling 
    textureManager.initialize();

    std::cout << "Assets loading..." << std::endl;


    //Menu
    scalingRules["background"] = { 192 , 108};
    scalingRules["startb"] = { 192, 108 };
    scalingRules["optionsb"] = { 192, 108 };

    //shop
    scalingRules["shop"] = { 192 * 4, 108 * 4 };
    scalingRules["coin_bag"] = { 192, 108 };

    //GameRun
    scalingRules["backUI"] = {192, 108 };
    scalingRules["coin_animation"] = { 192 * 4, 108 * 4 };

    scalingRules["use_button"] = { 192 * 4, 108 * 4 }; //better ui in the future
    scalingRules["discard_button"] = { 192 * 4, 108 * 4 }; //better ui in the future
    scalingRules["exit_button"] = { 192 * 2, 108 * 2 }; //better ui in the future

    //Turn (no)
    scalingRules["monstro"] = { 192 * 4, 108 * 4 };
    //Board
    scalingRules["board"] = { 192, 108 };

    //Slot
    scalingRules["slot"] = { 192 * 2, 108 * 2 };
    scalingRules["shopslot"] = { 192, 108 }; // we probably dont even call this (consider using tags for groups of assets as "slots" and "tiles"

    //Tile
    scalingRules["2"] = { 192 * 2, 108 *2}; //all tiles are the same beacuase we set scale only at start
    scalingRules["4"] = { 192, 108 };
    scalingRules["8"] = { 192, 108 };
    scalingRules["16"] = { 192, 108 };
    scalingRules["32"] = { 192, 108 };
    scalingRules["64"] = { 192, 108 };
    scalingRules["128"] = { 192, 108 };
    scalingRules["256"] = { 192, 108 };
    scalingRules["512"] = { 192, 108 };
    scalingRules["1024"] = { 192, 108 };
    scalingRules["2048"] = { 192, 108 };

    //end

    std::cout << "Loading ended!" << std::endl;

}



void RenderSystem::resizeSprite(const std::string& id, sf::Sprite& sprite) {
    auto it = scalingRules.find(id);
    if (it == scalingRules.end()) {
        std::cerr << "No scaling rule for asset: " << id << std::endl;
        return;
    }

    const auto& scale = it->second;

    float xscaling = windowSize.x / scale.width;
    float yscaling = windowSize.y / scale.height;

    sprite.setScale({xscaling, yscaling});
}

void RenderSystem::draw(sf::Drawable& sprite) {

    window.draw(sprite);

}

void RenderSystem::close() {

    window.close();
}

//------GETTERS

const TextureManager& RenderSystem::getTextureManager() const {
    return textureManager;
}

const sf::Vector2u& RenderSystem::getWindowSize() const {
    return windowSize;
}

sf::RenderWindow& RenderSystem::getWindow() {
    return window;
}