#include "rendering/TextureManager.h"

void TextureManager::initialize() {

    //  KEY :: PATH
    //
    //Menu
    load("background", "assets/textures/background.png");
    load("startb", "assets/textures/startb.png");
    load("optionsb", "assets/textures/optionsb.png");
    //Shop
    load("shop", "assets/textures/shop.png");
    load("coin_bag", "assets/textures/coin_bag.png");
    //GameRun
    load("backUI", "assets/textures/backUI.png");
    load("coin_animation", "assets/textures/coin_animation.png");

    load("use_button", "assets/textures/use_button.png"); //better ui in the future
    //Turn(no)
    load("monstro", "assets/textures/monstro.png");   //bosses?

    //Board
    load("board", "assets/textures/board.png");
    //Slot
    load("slot", "assets/textures/slot.png");
    load("shopslot", "assets/textures/shopslot.png");

    //Tiles
    load("2", "assets/textures/2.png");
    load("4", "assets/textures/4.png");
    load("8", "assets/textures/8.png");
    load("16", "assets/textures/16.png");
    load("32", "assets/textures/32.png");
    load("64", "assets/textures/64.png");
    load("128", "assets/textures/128.png");
    load("256", "assets/textures/256.png");
    load("512", "assets/textures/512.png");
    load("1024", "assets/textures/1024.png");
    load("2048", "assets/textures/2048.png");

}

void TextureManager::load(const std::string& id, const std::string& path) {
    if (textures_.count(id) == 0) {
        sf::Texture texture;
        if (!texture.loadFromFile(path)) {
            std::cerr << "Failed to load texture: " << path << std::endl;
            return;
        }

        std::cout << "Texture loaded successfully: " << path << std::endl;
        textures_[id] = std::move(texture);
    }
}

const sf::Texture& TextureManager::get(const std::string& id) const {
    auto it = textures_.find(id);
    if (it == textures_.end()) {
        std::cerr << "Texture with id '" << id << "' not found!\n";
        throw std::runtime_error("Missing texture: " + id);
    }
    return it->second;
}
