#include "rendering/TextureManager.h"

void TextureManager::initialize() {
    // Menu
    load("background", "assets/textures/background.png");
    load("startb",     "assets/textures/startb.png");
    load("optionsb",   "assets/textures/optionsb.png");

    // Shop
    load("shop",       "assets/textures/shop.png");
    load("coin_bag",   "assets/textures/coin_bag.png");

    // GameRun UI
    load("backUI",          "assets/textures/backUI.png");
    load("merge_animation", "assets/textures/merge_animation.png");
    load("coin_animation",  "assets/textures/coin_animation.png");
    load("use_button",      "assets/textures/use_button.png");
    load("discard_button",  "assets/textures/discard_button.png");
    load("exit_button",     "assets/textures/exit_button.png");
    load("cards_button",    "assets/textures/cards_button.png");
    load("digits",          "assets/textures/digits.png");

    // Cards
    load("two_for_two", "assets/textures/two_for_two.png");

    // Board
    load("slot",     "assets/textures/slot.png");
    load("shopslot", "assets/textures/shopslot.png");
    load("monstro",  "assets/textures/monstro.png");
    load("bomb",     "assets/textures/bomb.png");
    load("bomb_2",     "assets/textures/bomb_2.png");
    load("bomb_3",     "assets/textures/bomb_3.png");
    load("brick",      "assets/textures/brick.png");
    load("black_hole", "assets/textures/black_hole.png");
    load("die",        "assets/textures/die.png");
    load("hourglass",  "assets/textures/hourglass.png");
    load("mount",      "assets/textures/mount.png");
    load("wrench",     "assets/textures/wrench.png");
    load("switch",     "assets/textures/switch.png");

    // Tiles
    load("2",    "assets/textures/2.png");
    load("4",    "assets/textures/4.png");
    load("8",    "assets/textures/8.png");
    load("16",   "assets/textures/16.png");
    load("32",   "assets/textures/32.png");
    load("64",   "assets/textures/64.png");
    load("128",  "assets/textures/128.png");
    load("256",  "assets/textures/256.png");
    load("512",  "assets/textures/512.png");
    load("1024", "assets/textures/1024.png");
    load("2048", "assets/textures/2048.png");
}

void TextureManager::load(const std::string& id, const std::string& path) {
    if (textures.count(id)) return;

    sf::Texture texture;
    if (!texture.loadFromFile(path)) {
        std::cerr << "Failed to load texture: " << path << std::endl;
        return;
    }
    textures[id] = std::move(texture);
}

const sf::Texture& TextureManager::get(const std::string& id) const {
    auto it = textures.find(id);
    if (it == textures.end()) {
        throw std::runtime_error("Missing texture: " + id);
    }
    return it->second;
}
