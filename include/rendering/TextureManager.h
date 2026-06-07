#pragma once

#include <SFML/Graphics.hpp>
#include <unordered_map>
#include <string>
#include <iostream>
#include <stdexcept>

class TextureManager {
public:
    void initialize();
    void load(const std::string& id, const std::string& path);
    const sf::Texture& get(const std::string& id) const;

private:
    std::unordered_map<std::string, sf::Texture> textures;
};
