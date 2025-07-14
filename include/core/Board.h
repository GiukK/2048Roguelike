#pragma once

#include <iostream>
#include <map>
#include <memory>
#include <vector>
#include <random>

#include "Coord.h"
#include "Slot.h"

#include "SFML/Graphics.hpp"

class Turn;

class Board {

public:

    Board(Turn* turn);

    //This constructor is useful when creating a deepcopy of the previous board of the previous turn

    Board(const Board& other, Turn* turn);
    Board& operator=(const Board& other) = delete;


    void render(sf::RenderWindow& window);

    void start();

    void setRng(std::mt19937 rng);
    //------
    void spawnTileInRandomEmptySlot();
    
    bool moveLeft();
    bool moveRight();
    bool moveUp();
    bool moveDown();

    void clear();

    Turn* turn;

private:

    //------
    //{coloumn,row} : slot_POINTER


    std::map<Coord, std::unique_ptr<Slot>> slots;

    //how many coloumns are there
    std::pair<int, int> col_range{ -1, 3 };
    //how many rows are there
    std::pair<int, int> row_range{ 0, 3 };

    //------

    sf::Texture boardTexture;
    sf::Sprite board;




    //------
    std::mt19937 rng{};

    int getRandomInt(int min, int max);
    float getRandomFloat(float min, float max);
    //------







};

