#pragma once

#include <iostream>
#include <map>
#include <memory>
#include <vector>
#include <random>

#include "Coord.h"
#include "Slot.h"

#include "SFML/Graphics.hpp"

class Board {

public:

    Board();

    void render(sf::RenderWindow& window);

    void start();

    void setRng(std::mt19937 rng);
    //------
    void spawnTileInRandomEmptySlot();
    
    void moveLeft();
    void moveRight();
    void moveUp();
    void moveDown();

    void clear();


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

