#include "core/Turn.h"
#include "core/GameRun.h"
#include "core/Board.h"
#include "core/Direction.h"

Turn::Turn(GameRun* game_run) :
    game_run(game_run),
    board(this),
    boardBegin(board , this)
{}


Turn::Turn(GameRun* game_run,const Board& initial_board) :
    game_run(game_run),
    board(initial_board, this),
    boardBegin(board, this)
{
}

void Turn::nextPhase() {

    std::cout << "[FSM] Transitioning from " << toString(currentPhase);

    switch (currentPhase) {
    case Phase::Begin:
        currentPhase = Phase::Movement;
        break;
    case Phase::Movement:
        currentPhase = Phase::BoardResolution;
        break;
    case Phase::BoardResolution:
        currentPhase = Phase::End;
        break;
    case Phase::End:
        
        end_turn();

        break;
    default:
        break;
    }

    std::cout << " to " << toString(currentPhase) << std::endl;
}


//this function has to reverse every possible thing that might create bugs in the future if the player comes back to this TURN
void Turn::end_turn() {

    std::cout << "Turn ended" << std::endl;

    game_run->new_turn(this->board);

    currentPhase = Phase::Begin;
    board = Board(boardBegin, this);

}

void Turn::requestShop() {

    game_run->openShop(); // delega a GameRun, to avoid multiple shops by consecutive merges. (possible feature?)
}


/*
Begin,
Movement,
BoardResolution,
End
*/
void Turn::update(float delta) {
    switch (currentPhase) {
    case Phase::Begin:

        if (inputReceived) {
            nextPhase();
        }

        break;
    case Phase::Movement:

        //this part has to be fixed in the future since board.move() has an internal while loop and starts and ends with Resolving as a flag
        //so I think a single thread will just proceed in order and therefore the flag MAY be useless + animation will need to be handled carfully
        //since update() will handle frame skip and render() will draw.

        //I feel like move() should be called once and then the animation will have to handle the transition or something

        //moves just 1 time
        if (inputReceived) {
            board.move(move);
            move = Direction::None; //backs to default
            inputReceived = false; //backs to default for future possible go_backs (?)
                                    //this would be better handled by end_turn()
        }

        board.update(delta); //does nothing now


        //the movement has to be processed to understand if the move is valid so this check has to be made here.
        //in case the move fails the phase goes back to Begin
        if (board.moveIsPermitted()) {
            nextPhase();
        }
        else { 
            currentPhase = Phase::Begin;
            std::cout << "[FSM] Move invalid: Transitioning from Movement to Begin" << std::endl;
        }

        break;
    case Phase::BoardResolution:

        board.spawnTileInRandomEmptySlot();
        nextPhase();

        break;
    case Phase::End:

        end_turn();
        break;

    default:
        break;
    }
}

void Turn::render(sf::RenderWindow& window) {

    //render
}

void Turn::handleInput(sf::Event& event) {

    switch (currentPhase) {
    case Phase::Begin:

        handleInput_BeginPhase(event);

        break;
    case Phase::Movement:

        break;
    case Phase::BoardResolution:

        break;
    case Phase::End:
        break;

    default:
        break;
    }
}

void Turn::handleInput_BeginPhase(sf::Event& event) {
    if (const auto* keyPressed = event.getIf<sf::Event::KeyReleased>()) {

        switch (keyPressed->scancode) {
        case sf::Keyboard::Scancode::A: move = Direction::Left; inputReceived = true; break;
        case sf::Keyboard::Scancode::D: move = Direction::Right; inputReceived = true; break;
        case sf::Keyboard::Scancode::W: move = Direction::Up; inputReceived = true; break;
        case sf::Keyboard::Scancode::S: move = Direction::Down; inputReceived = true; break;
        case sf::Keyboard::Scancode::X:
            board.spawnTileInRandomEmptySlot();
            return;
        case sf::Keyboard::Scancode::Delete:
            board.clear();
            board.spawnTileInRandomEmptySlot();
            return;
        case sf::Keyboard::Scancode::B:
            game_run->go_back();
            return;
        default:
            return;
        }
        

       
    }
}

