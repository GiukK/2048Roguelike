#pragma once

#include <iostream>
#include <random>
#include <stack>

#include <SFML/Graphics.hpp>
#include "core/Turn.h"

class PlayState;


class GameRun {

public:

    //------

    //------

    GameRun(sf::RenderWindow& window, PlayState* playState);

    void enter() ;
    void exit() ;

    void new_turn(const Board& initial_board);
    void go_back();

    void openShop();

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

    PlayState* playState;
    std::stack<std::unique_ptr<Turn>> run_turns;


    //------


    void addScore(int score);
    void subtractScore(int score);

    int getRandomInt(int min, int max);
    float getRandomFloat(float min, float max);

};