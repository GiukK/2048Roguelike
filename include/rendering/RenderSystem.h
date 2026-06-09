#pragma once

#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include "rendering/TextureManager.h"
#include "rendering/Camera.h"

class RenderSystem {
public:
    RenderSystem(sf::RenderWindow& window);

    void initialize(const sf::Vector2u& windowSize);
    void resizeSprite(const std::string& id, sf::Sprite& sprite);

    const TextureManager& getTextureManager() const;
    const sf::Vector2u& getWindowSize() const;
    sf::RenderWindow& getWindow();

    // --- Render layers -----------------------------------------------------
    // Draw board content (slots, tiles, board-anchored effects) between a
    // useBoardView() and the next useUIView(); draw HUD/buttons/overlays under
    // useUIView(). The UI layer is the window's default 1:1 view, so screen-space
    // coordinates and raw-pixel button hit-tests stay valid.
    void useBoardView();
    void useUIView();

    Camera& getBoardCamera() { return boardCamera; }

    // Maps a window pixel to board-space through the current board camera. Board
    // input (hover/selection) must use this so picking follows the camera.
    sf::Vector2f mapPixelToBoard(sf::Vector2i pixel) const;

    // Zooms the board camera by `factor` (multiplicative) while keeping the board
    // point currently under `pixel` fixed under the cursor (zoom-toward-cursor).
    // Zoom is clamped by the camera; when it clamps, the view does not move.
    void zoomBoardTowardPixel(sf::Vector2i pixel, float factor);

    // Pans the board camera so content follows a mouse drag: `pixelDelta` is the
    // mouse movement since the last step (converted to world units by the zoom).
    void panBoardByPixels(sf::Vector2i pixelDelta);

    void draw(sf::Drawable& drawable);

    // Draws an integer using the "digits" spritesheet, horizontally centered at `center`.
    // Each digit is 5x7 px in the sheet, scaled by `scale`.
    void drawNumber(unsigned int value, sf::Vector2f center, float scale = 10.f);

    // --- Real text (sf::Font) ----------------------------------------------
    // Foundations for the data-driven UI: arbitrary text rendering + measurement
    // so layout can size boxes to their content. Font loaded in initialize().
    void drawText(const std::string& text, sf::Vector2f topLeft,
                  unsigned int charSize, sf::Color color);
    // Advance width + line height of a single line (no newlines).
    sf::Vector2f measureText(const std::string& text, unsigned int charSize) const;
    // Greedy word-wrap into lines that each fit within `maxWidth`.
    std::vector<std::string> wrapText(const std::string& text, float maxWidth,
                                      unsigned int charSize) const;

    // Draws the texture `textureId` scaled to fill `rect` (for UI Image nodes).
    void drawImage(const std::string& textureId, sf::FloatRect rect);

    // --- Procedural shapes -------------------------------------------------
    // Pixel-stepped rounded rectangle (no texture). Drawn as horizontal slabs
    // whose width curves in near the corners. An optional border is drawn by
    // filling an outer rounded rect in `border`, then an inset one in `fill`.
    void drawPixelRoundedRect(sf::FloatRect rect, float radius, sf::Color fill,
                              sf::Color border = sf::Color::Transparent,
                              float borderThickness = 0.f);

    void close();

private:
    struct ScaleRule {
        float width;
        float height;
    };

    // Fills a pixel-stepped rounded rect in one color (the body of the public
    // drawPixelRoundedRect, called once or twice for the border).
    void fillRoundedRect(sf::FloatRect rect, float radius, sf::Color color);

    // Size (in screen px) of one staircase step on rounded corners — tune for
    // chunkier/finer pixel corners.
    static constexpr float RoundedRectPixelStep = 4.f;

    sf::RenderWindow& window;
    sf::Vector2u windowSize;
    TextureManager textureManager;
    std::unordered_map<std::string, ScaleRule> scalingRules;
    sf::Font font;

    Camera boardCamera;
};
