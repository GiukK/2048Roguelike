#include "rendering/Animation.h"
#include "rendering/RenderSystem.h"

Animation::Animation(RenderSystem& renderer,
                     const std::string& textureId,
                     sf::Vector2i frameSize,
                     int frameCount,
                     sf::Vector2f position)
    : renderer(renderer),
      textureId(textureId),
      frameSize(frameSize),
      frameCount(frameCount),
      sprite(renderer.getTextureManager().get(textureId))
{
    sprite.setOrigin({static_cast<float>(frameSize.x) / 2.f,
                      static_cast<float>(frameSize.y) / 2.f});
    renderer.resizeSprite(textureId, sprite);
    sprite.setPosition(position);
    sprite.setTextureRect(sf::IntRect({0, 0}, {frameSize.x, frameSize.y}));
}

void Animation::update(float dt) {
    elapsed += dt;
    if (elapsed < FRAME_DURATION) return;

    elapsed = 0.f;
    currentFrame++;

    if (currentFrame >= frameCount) {
        if (looping) {
            currentFrame = 0;
        }
        return;
    }

    sprite.setTextureRect(sf::IntRect({currentFrame * frameSize.x, 0},
                                       {frameSize.x, frameSize.y}));
}

bool Animation::isFinished() const {
    return !looping && currentFrame >= frameCount;
}

sf::Sprite& Animation::getSprite() {
    return sprite;
}
