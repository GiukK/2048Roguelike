#pragma once

#include <iostream>
#include <map>
#include <memory>
#include <vector>
#include <random>

#include "utils/Coord.h"
#include "Slot.h"
#include "utils/Direction.h"
#include "utils/MovementQueue.h"

#include "SFML/Graphics.hpp"

class Turn;
class RenderSystem;


class Board {

public:

    Board(RenderSystem& renderer, Turn* turn, bool doInitialSetup = true);

    static Board cloneFrom(const Board& other, Turn* turn);
    //copy for go_back()
    void copyStateFrom(const Board& other);


    //-----------------------------------------------------------------
    void render(RenderSystem& renderer);

    void update(float deltaTime);
    //-----------------------------------------------------------------


    //spawns random tile in random empty slot (to be made modular)
    void spawnTileInRandomEmptySlot();
    //
    void generateCoins();
    
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
    bool animationFinished() const;


    bool isResolvingMovement() const;
    bool moveIsPermitted() const;

    //-----------------------------------------------TO BE ADDED
    //void reset() //to reset all flags and state



    //test
    void handleClick(sf::Vector2f worldPos);

private:

    void firstBoardSetUp();

    //Sprite
    RenderSystem& renderer;
    sf::Sprite board;

    void fixVisualAssets();

    //internal slots
    std::map<Coord, std::unique_ptr<Slot>> slots;

    //fundamental movement Deque class
    MovementQueue movementQueue;


    //------
    //range of coloumns and rows in the board (to be deprecated)
    std::pair<int, int> col_range{ -1, 3 };
    std::pair<int, int> row_range{ 0, 3 };
    //------


    //------ rng utils
    int getRandomInt(int min, int max);
    float getRandomFloat(float min, float max);
    //------





    //trivial flag for movement handling
    bool isResolvingMovementFlag = false;

    bool moveIsPermittedFlag = false;
};

