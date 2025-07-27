#pragma once

#include <string>
#include <SFML/Graphics.hpp>

class RenderSystem;

struct saleItem {
    
    saleItem(std::string name, RenderSystem& renderer);

    std::string name;
    sf::Sprite sprite;

    bool sold{false};

    short int price{ 1 };

};