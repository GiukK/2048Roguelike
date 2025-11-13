#pragma once

#include <iostream>
#include "core/Board.h"
#include "core/utils/Direction.h"

class GameRun; //   forward declaration GameRun -unique-> Turn
               //                       Turn -raw-> GameRun
class RenderSystem;
class saleItem;


class Turn {

public:

    //FSM of phases
    enum class Phase {
        Begin,
        //ApplyConsumables,
        SelectingConsumables,
        Movement,
        //TriggerPassives,
        BoardResolution,
        //SpecialEvents,
        End
    };

    inline const char* toString(Phase phase) {
        switch (phase) {
        case Phase::Begin:                      return "Begin";
        case Phase::SelectingConsumables:       return "SelectingConsumables";
        //case Phase::ApplyEffects:             return "ApplyEffects";
        case Phase::Movement:                   return "Movement";
        case Phase::BoardResolution:            return "BoardResolution";
        //case Phase::TriggerPassives:          return "TriggerPassives";
        //case Phase::TriggerSpecialStates:     return "TriggerSpecialStates";
        case Phase::End:                        return "End";
        default:                                return "Unknown Phase";
        }
    }

    Turn(RenderSystem& renderer, GameRun* game_run);
    Turn(RenderSystem& renderer, GameRun* game_run, const Board& initial_board);

    //---------------------------------------------------------
    //modularized for FSM
    void handleInput(sf::Event& event);
        void handleInput_BeginPhase(sf::Event& event);

    void update(float deltaTime);
    void render(RenderSystem& renderer);
    //---------------------------------------------------------

    //------TURNS OWNS ITS BOARD ITSELF, IT WILL MANAGE IT INTRNALLY
    // 
    // each turn must contain every information to replicate the exact instance of the board i.e:
    // the disposition of the tiles at the start of the turn
    // the move made at the end of the turn
    // 
    // essential for go_back and future effects
    // 
    //---------------------------------------------------------
    //raw ptr back to owner GameRun (first pointer to gamerun, then board, problem with rng)
    GameRun* game_run;

    //internal board of the turn
    Board board;
    //useful for restoring the turn's state
    Board boardBegin;


    //internal currentmove made - None if not yet chosen
    Direction move = Direction::None;

    //current phase of the Turn in the FSM
    Phase currentPhase = Phase::Begin;

    //skips phase following the common order
    void nextPhase();

    //hard ends turn
    void end_turn();

    //requests shop to be handled by GameRun (possible change)
    void requestShop();

private:


    //trivial flag for input of move (to be depracated)
    bool inputReceived = 0;

    RenderSystem& renderer;


    //void itemSelected(saleItem& inventory_item);

};