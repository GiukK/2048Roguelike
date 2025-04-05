#pragma once

#include <iostream>
#include <random>

#include <SFML/Graphics.hpp>
#include "core/Tile.h"
#include "core/Board.h"


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

    unsigned int randomSeed;
    std::mt19937 rng;

    bool gameStarted{ 0 };

    //------

    sf::RenderWindow& window;

    //------

    int score{};

    Board board;

    //------


    void addScore(int score);
    void subtractScore(int score);

    int getRandomInt(int min, int max);
    float getRandomFloat(float min, float max);

};