#pragma once

#include <iostream>

#include <SFML/Graphics.hpp>
#include "core/Tile.h"

class GameRun {

public:

    //------





    //------

    GameRun(sf::RenderWindow& window);

    void enter() ;
    void exit() ;

    void handleInput(sf::Event& event) ;
    void update(float deltaTime) ;
    void render(sf::RenderWindow& window);

private:

    //------

    sf::RenderWindow& window;

    //------

    int score{};

    sf::Texture boardTexture;
    sf::Sprite board;

    std::vector<std::unique_ptr<Tile>> tiles;

    //------


    void addScore(int score);
    void subtractScore(int score);

};


