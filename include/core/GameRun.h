#pragma once

#include <iostream>
#include <random>
#include <stack>

#include <SFML/Graphics.hpp>
#include "core/Turn.h"


class GameRun {

public:

    //------

    //------

    GameRun(sf::RenderWindow& window);

    void enter() ;
    void exit() ;

    void new_turn(Board initial_board);
    void go_back();

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

    std::stack<std::unique_ptr<Turn>> run_turns;


    //------


    void addScore(int score);
    void subtractScore(int score);

    int getRandomInt(int min, int max);
    float getRandomFloat(float min, float max);

};