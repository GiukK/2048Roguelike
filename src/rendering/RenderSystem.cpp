#include "rendering/RenderSystem.h"

#include <algorithm>
#include <cmath>
#include <sstream>

RenderSystem::RenderSystem(sf::RenderWindow& window)
    : window(window)
{}

void RenderSystem::initialize(const sf::Vector2u& size) {
    windowSize = size;
    textureManager.initialize();

    // Real font for the data-driven UI: Pixelify Sans (SIL OFL 1.1, license kept in
    // assets/fonts/). Smoothing off keeps the pixel look crisp.
    if (!font.openFromFile("assets/fonts/PixelifySans-Regular.ttf")) {
        std::cerr << "Failed to load font: assets/fonts/PixelifySans-Regular.ttf" << std::endl;
    }
    font.setSmooth(false);

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

    // Cards (sprites are 31x41; this rule shows them at a readable card size)
    scalingRules["two_for_two"]   = {192 * 2, 108 * 2};
    scalingRules["economic_boom"] = {192 * 2, 108 * 2};
    scalingRules["vase_of_two"]   = {192 * 2, 108 * 2};
    scalingRules["back_to_back"]  = {192 * 2, 108 * 2};
    scalingRules["bob"]           = {192 * 2, 108 * 2};
    scalingRules["consume"]       = {192 * 2, 108 * 2};
    scalingRules["red_light"]     = {192 * 2, 108 * 2};
    scalingRules["card_ruler"]    = {192 * 2, 108 * 2};

    // Board & Slots
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
    scalingRules["mount"]      = {192, 108};
    scalingRules["wrench"]     = {192, 108};
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

void RenderSystem::useBoardView() {
    window.setView(boardCamera.getView(windowSize));
}

void RenderSystem::useUIView() {
    // The default view is the 1:1 window-sized view, so HUD/buttons keep using
    // plain screen coordinates.
    window.setView(window.getDefaultView());
}

sf::Vector2f RenderSystem::mapPixelToBoard(sf::Vector2i pixel) const {
    return window.mapPixelToCoords(pixel, boardCamera.getView(windowSize));
}

void RenderSystem::zoomBoardTowardPixel(sf::Vector2i pixel, float factor) {
    // The board point currently under the cursor.
    const sf::Vector2f worldBefore = mapPixelToBoard(pixel);

    boardCamera.setZoomLevel(boardCamera.getZoomLevel() * factor);
    const float zoom = boardCamera.getZoomLevel();  // actual value after clamping

    // The view maps a pixel to world as:
    //   world = center + (pixel - windowSize/2) / zoom
    // Solve for the center that keeps worldBefore under the same pixel. If the
    // zoom clamped (unchanged), this reproduces the current center exactly.
    const sf::Vector2f half{ static_cast<float>(windowSize.x) / 2.f,
                             static_cast<float>(windowSize.y) / 2.f };
    const sf::Vector2f offset{ (static_cast<float>(pixel.x) - half.x) / zoom,
                               (static_cast<float>(pixel.y) - half.y) / zoom };
    boardCamera.setCenter(worldBefore - offset);
}

void RenderSystem::panBoardByPixels(sf::Vector2i pixelDelta) {
    // Moving the cursor right by N px should slide the board right by N px, which
    // means shifting the view center LEFT by N/zoom world units (hence minus).
    const float zoom = boardCamera.getZoomLevel();
    const sf::Vector2f worldDelta{ static_cast<float>(pixelDelta.x) / zoom,
                                   static_cast<float>(pixelDelta.y) / zoom };
    boardCamera.setCenter(boardCamera.getCenter() - worldDelta);
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

// --- Real text ---

void RenderSystem::drawText(const std::string& text, sf::Vector2f topLeft,
                            unsigned int charSize, sf::Color color) {
    sf::Text t(font, text, charSize);
    t.setFillColor(color);
    // Round to whole pixels so the glyphs stay crisp (no sub-pixel blur).
    t.setPosition({std::round(topLeft.x), std::round(topLeft.y)});
    window.draw(t);
}

sf::Vector2f RenderSystem::measureText(const std::string& text, unsigned int charSize) const {
    sf::Text t(font, text, charSize);
    // findCharacterPos(length) is the pen position after the last glyph — i.e. the
    // advance width, which is what layout wants (more stable than visual bounds).
    float width = t.findCharacterPos(text.size()).x;
    float height = font.getLineSpacing(charSize);
    return {width, height};
}

std::vector<std::string> RenderSystem::wrapText(const std::string& text, float maxWidth,
                                                unsigned int charSize) const {
    std::vector<std::string> lines;
    std::istringstream iss(text);
    std::string word;
    std::string current;
    while (iss >> word) {
        std::string candidate = current.empty() ? word : current + " " + word;
        if (current.empty() || measureText(candidate, charSize).x <= maxWidth) {
            current = std::move(candidate);
        } else {
            lines.push_back(current);
            current = word;
        }
    }
    if (!current.empty()) lines.push_back(current);
    return lines;
}

void RenderSystem::drawImage(const std::string& textureId, sf::FloatRect rect) {
    const sf::Texture& tex = textureManager.get(textureId);
    const sf::Vector2u ts = tex.getSize();
    if (ts.x == 0 || ts.y == 0) return;

    sf::Sprite sprite(tex);
    sprite.setPosition(rect.position);
    sprite.setScale({ rect.size.x / static_cast<float>(ts.x),
                      rect.size.y / static_cast<float>(ts.y) });
    window.draw(sprite);
}

// --- Procedural shapes ---

void RenderSystem::fillRoundedRect(sf::FloatRect rect, float radius, sf::Color color) {
    const float x = rect.position.x;
    const float y = rect.position.y;
    const float w = rect.size.x;
    const float h = rect.size.y;
    if (w <= 0.f || h <= 0.f) return;

    radius = std::clamp(radius, 0.f, std::min(w, h) / 2.f);
    const float step = std::max(1.f, RoundedRectPixelStep);

    // Horizontal slabs, full width except where the rounded corners curve in.
    for (float sy = 0.f; sy < h; sy += step) {
        const float slabH = std::min(step, h - sy);
        const float cy = sy + slabH / 2.f;   // slab centre, for the inset curve

        float inset = 0.f;
        if (cy < radius) {
            const float dy = radius - cy;
            inset = radius - std::sqrt(std::max(0.f, radius * radius - dy * dy));
        } else if (cy > h - radius) {
            const float dy = cy - (h - radius);
            inset = radius - std::sqrt(std::max(0.f, radius * radius - dy * dy));
        }

        sf::RectangleShape slab({w - 2.f * inset, slabH});
        slab.setPosition({x + inset, y + sy});
        slab.setFillColor(color);
        window.draw(slab);
    }
}

void RenderSystem::drawPixelRoundedRect(sf::FloatRect rect, float radius, sf::Color fill,
                                        sf::Color border, float borderThickness) {
    if (borderThickness > 0.f && border.a > 0) {
        fillRoundedRect(rect, radius, border);
        sf::FloatRect inner({rect.position.x + borderThickness, rect.position.y + borderThickness},
                            {rect.size.x - 2.f * borderThickness, rect.size.y - 2.f * borderThickness});
        fillRoundedRect(inner, std::max(0.f, radius - borderThickness), fill);
    } else {
        fillRoundedRect(rect, radius, fill);
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
