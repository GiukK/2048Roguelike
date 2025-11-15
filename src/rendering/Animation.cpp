#include "rendering/Animation.h"
#include "rendering/RenderSystem.h"  // if RenderSystem is defined there
#include <iostream>

// Constructor
Animation::Animation(RenderSystem& renderer,
    const std::string& idle_id,
    const sf::Vector2i size,
    int nframes,
    sf::Vector2f pos)
    : renderer(renderer),
    idle_id(idle_id),
    size(size),
    nframes(nframes),
    sprite(renderer.getTextureManager().get(idle_id))
{
    //ANI TRY -------
    sprite.setOrigin({(float)size.x/2,(float)size.y/2});

    renderer.resizeSprite(idle_id, sprite);
    sprite.setPosition(pos);
    //animation starts at 0, avoids full sprite artifact
    sprite.setTextureRect(sf::IntRect({ int(currentframe * size.x), 0 }, { size.x, size.y }));
    //----------
}

// Update animation state based on delta time
void Animation::update(float dt)
{
    time_elapsed += dt;

    if (time_elapsed > 1.f / 12.f) {

        currentframe += 1;

        if (currentframe >= nframes and shouldLoop) {
            currentframe = 0;
        }
        else if (currentframe > nframes and not shouldLoop) {

            std::cout << "animation should be deleted!";
        }

        sprite.setTextureRect(sf::IntRect({ int(currentframe * size.x), 0 }, { size.x, size.y })); //animation starts at 0

        time_elapsed = 0;
    }

    return;
}

// Check if animation has finished playing
bool Animation::isEnded()
{
    return (currentframe == nframes);
}

// Accessor for the sprite
sf::Sprite& Animation::getSprite()
{
    return sprite;
}
