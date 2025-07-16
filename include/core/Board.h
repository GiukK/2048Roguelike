#pragma once

#include <iostream>
#include <map>
#include <memory>
#include <vector>
#include <random>

#include "Coord.h"
#include "Slot.h"
#include "Direction.h"
#include "MovementQueue.h"

#include "SFML/Graphics.hpp"

class Turn;

class Board {

public:

    Board(Turn* turn);
    //This constructor is useful when creating a deepcopy of the previous board of the previous turn
    Board(const Board& other, Turn* turn);

    Board(Board&&) noexcept = default;
    Board& operator=(Board&&) noexcept = default;

    //-----------------------------------------------------------------
    void render(sf::RenderWindow& window);
    void update(float deltaTime);
    //-----------------------------------------------------------------


    //hard sets rng (useful for future seed replication) (GameRun should handle it fully?)
    void setRng(std::mt19937 rng);

    //spawns random tile in random empty slot (to be made modular) - uses internal rng
    void spawnTileInRandomEmptySlot();
    
    //handles board movement logic
    void move(Direction dir);
    //initializes Deque of slots to be moved and considered - move dependent
    void initializeMovementQueue(Direction dir);
    //helper function to manage slot movement in Deque
    void resolveNextTileMove(Direction dir);
    //helper to decide tile position
    Coord getNextCoord(Coord from, Direction dir);

    //clears Board's TILES
    void clear();

    //raw ptr to owner - Turn
    Turn* turn;

    //getters
    bool isResolvingMovement() const;
    bool moveIsPermitted() const;

    //-----------------------------------------------TO BE ADDED
    //void reset() //to reset all flags and state

private:

    //internal slots
    std::map<Coord, std::unique_ptr<Slot>> slots;

    //fundamental movement Deque class
    MovementQueue movementQueue;


    //------
    //range of coloumns and rows in the board (to be deprecated)
    std::pair<int, int> col_range{ -1, 3 };
    std::pair<int, int> row_range{ 0, 3 };
    //------
    
    //rendering utils
    sf::Texture boardTexture;
    sf::Sprite board;


    //internal rng (GameRun should handle it fully?)
    std::mt19937 rng{};
    //------ rng utils
    int getRandomInt(int min, int max);
    float getRandomFloat(float min, float max);
    //------





    //trivial flag for movement handling
    bool isResolvingMovementFlag = false;

    bool moveIsPermittedFlag = false;
};

