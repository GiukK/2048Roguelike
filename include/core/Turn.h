#pragma once

#include <iostream>
#include "core/Board.h"

class GameRun; //   forward declaration GameRun -u-> Turn
               //                       Turn -raw-> GameRun

class Turn {

public:

    Turn(GameRun* game_run);
    Turn(GameRun* game_run, const Board& initial_board);

    void announce_event();
    void end_turn();

    void requestShop();

    void update(float deltaTime);
    void handleInput(sf::Event& event);


    //------TURNS CONTAINS THE BOARD.H ITSELF, IT WILL MANAGE IT INTRNALLY
    // 
    // each turn must contain every information to replicate the exact instance of the board i.e:
    // the disposition of the tiles at the start of the turn
    // the move made at the end of the turn
    // 
    // note : contrary to logic, the turn "ends" with the move, in the sense that the information to move the tiles will still be processed in that turn but the
    // final disposition of the tiles will be stored in the initial state of the next turn.
    // 
    // IN THE FUTURE : the turn will probably contain a field called "events" or similar, that will contain a list of effects and special events, so that the turn will
    // be analyzable in further detail and complexity. At the moment the core concept to recreate is the "go back" feature.
    
    //---------------------------------------------------------


    Board board;

    GameRun* game_run;


private:

    bool input_received = 0;



};
