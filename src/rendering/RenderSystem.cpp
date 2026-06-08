#include "rendering/RenderSystem.h"

RenderSystem::RenderSystem(sf::RenderWindow& window)
    : window(window)
{}

void RenderSystem::initialize(const sf::Vector2u& size) {
    windowSize = size;
    textureManager.initialize();

    // Scale rules: {virtualWidth, virtualHeight}
    // sprite.scale = windowSize / virtual => larger virtual = smaller sprite

    // Menu
    scalingRules["background"]  = {192, 108};
    scalingRules["startb"]      = {192, 108};
    scalingRules["optionsb"]    = {192, 108};

    // Shop
    scalingRules["shop"]        = {192 * 4, 108 * 4};
    scalingRules["coin_bag"]    = {192, 108};

    // GameRun UI
    scalingRules["backUI"]           = {192, 108};
    scalingRules["coin_animation"]   = {192 * 2, 108 * 2};
    scalingRules["merge_animation"]  = {192 * 2, 108 * 2};
    scalingRules["use_button"]       = {192 * 4, 108 * 4};
    scalingRules["discard_button"]   = {192 * 4, 108 * 4};
    scalingRules["exit_button"]      = {192 * 2, 108 * 2};

    // Board & Slots
    scalingRules["board"]     = {192, 108};
    scalingRules["slot"]      = {192 * 2, 108 * 2};
    scalingRules["shopslot"]  = {192, 108};
    scalingRules["monstro"]   = {192 * 4, 108 * 4};
    scalingRules["bomb"]      = {192, 108};
    scalingRules["bomb_2"]     = {192, 108};
    scalingRules["bomb_3"]     = {192, 108};
    // "brick" rule sizes the inventory/shop icon. The on-tile overlay does NOT
    // use this rule — it matches the tile's own scale directly (see Tile::render).
    scalingRules["brick"]      = {192, 108};
    scalingRules["black_hole"] = {192, 108};
    scalingRules["die"]        = {192, 108};
    scalingRules["hourglass"]  = {192, 108};
    scalingRules["switch"]     = {192, 108};

    // Tiles (all share the same scale)
    ScaleRule tileScale = {192 * 2, 108 * 2};
    for (const auto& id : {"2","4","8","16","32","64","128","256","512","1024","2048"}) {
        scalingRules[id] = tileScale;
    }
}

void RenderSystem::resizeSprite(const std::string& id, sf::Sprite& sprite) {
    auto it = scalingRules.find(id);
    if (it == scalingRules.end()) {
        std::cerr << "No scaling rule for asset: " << id << std::endl;
        return;
    }
    float sx = static_cast<float>(windowSize.x) / it->second.width;
    float sy = static_cast<float>(windowSize.y) / it->second.height;
    sprite.setScale({sx, sy});
}

void RenderSystem::draw(sf::Drawable& drawable) {
    window.draw(drawable);
}

void RenderSystem::drawNumber(unsigned int value, sf::Vector2f center, float scale) {
    static constexpr float DIGIT_W = 5.f;
    static constexpr float DIGIT_H = 7.f;

    std::string text = std::to_string(value);
    float totalWidth = static_cast<float>(text.size()) * DIGIT_W * scale;
    float startX = center.x - totalWidth / 2.f;

    const sf::Texture& tex = textureManager.get("digits");

    for (size_t i = 0; i < text.size(); ++i) {
        int digit = text[i] - '0';
        sf::Sprite s(tex);
        s.setTextureRect(sf::IntRect(
            {static_cast<int>(digit * DIGIT_W), 0},
            {static_cast<int>(DIGIT_W), static_cast<int>(DIGIT_H)}));
        s.setScale({scale, scale});
        s.setPosition({startX + i * DIGIT_W * scale, center.y});
        window.draw(s);
    }
}

void RenderSystem::close() {
    window.close();
}

const TextureManager& RenderSystem::getTextureManager() const {
    return textureManager;
}

const sf::Vector2u& RenderSystem::getWindowSize() const {
    return windowSize;
}

sf::RenderWindow& RenderSystem::getWindow() {
    return window;
}
