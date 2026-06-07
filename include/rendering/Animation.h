#pragma once

#include <SFML/Graphics.hpp>
#include <string>

class RenderSystem;

class Animation {
public:
    Animation(RenderSystem& renderer,
              const std::string& textureId,
              sf::Vector2i frameSize,
              int frameCount,
              sf::Vector2f position);

    void update(float dt);
    bool isFinished() const;

    bool looping = true;

    sf::Sprite& getSprite();

private:
    RenderSystem& renderer;
    std::string textureId;
    sf::Vector2i frameSize;
    int frameCount;
    sf::Sprite sprite;

    int currentFrame = 0;
    float elapsed = 0.f;
    static constexpr float FRAME_DURATION = 1.f / 12.f;
};
