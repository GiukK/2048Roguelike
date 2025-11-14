#pragma once

#include <SFML/Graphics.hpp>

class RenderSystem;

class Animation {
public:

    Animation(RenderSystem& renderer,
              const std::string& idle_id,
                const sf::Vector2i size,
              int nframes,
              sf::Vector2f pos);


    void update(float dt);

    bool isEnded();

    bool shouldLoop{ true };

    sf::Sprite& getSprite();

private:

    RenderSystem& renderer;

    const std::string& idle_id;
    const sf::Vector2i size;
    int nframes{0};

    sf::Sprite sprite;

    //---

    int currentframe{ 0 };
    float time_elapsed{ 0.f };

};