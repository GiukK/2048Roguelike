#pragma once

#include <SFML/Graphics.hpp>
#include <string>
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

    Camera boardCamera;
};
