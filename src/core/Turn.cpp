#include "core/Turn.h"
#include "core/GameRun.h"
#include "core/Board.h"
#include "core/utils/Direction.h"
#include "states/PlayState.h"


#include "rendering/RenderSystem.h"


Turn::Turn(RenderSystem& renderer , GameRun* game_run) :
    renderer(renderer),
    game_run(game_run),
    board(renderer , this),
    boardBegin(Board::cloneFrom(board, this))

{
    std::cout << "Turn 1 ready" << std::endl;
}


Turn::Turn(RenderSystem& renderer , GameRun* game_run,const Board& initial_board) :
    renderer(renderer),
    game_run(game_run),
    board(Board::cloneFrom(initial_board, this)),
    boardBegin(Board::cloneFrom(initial_board, this))
{

}

void Turn::nextPhase() {

    std::cout << "[FSM] Transitioning from " << toString(currentPhase) << std::endl;

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
}


//this function has to reverse every possible thing that might create bugs in the future if the player comes back to this TURN
void Turn::end_turn() {

    std::cout << "Turn ended with move: " << dirToString(move) << std::endl;

    game_run->new_turn(this->board);

    currentPhase = Phase::Begin;

    move = Direction::None; //backs to default
    inputReceived = false; //backs to default for future possible go_backs
                            //moved from update()->case Movement -> after move.

    board.copyStateFrom(boardBegin); //useless reversing internals because of this(?)

}

void Turn::requestShop() {

    shopRequested = 1;

    std::cout << "SHOP HAS BEEN REQUESTED" << std::endl;

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

        board.generateCoins();

        board.update(delta);


        if (inputReceived) {
            nextPhase();
        }

        break;
    case Phase::Movement:

        //this part has to be fixed in the future since board.move() has an internal while loop and starts and ends with Resolving as a flag
        //so I think a single thread will just proceed in order and therefore the flag MAY be useless + animation will need to be handled carfully
        //since update() will handle frame skip and render() will draw.

                //re-reading code, I probably was referring to the fact that i wanted to implement multi-thread for update() and render() but im not sure.
                //either way, looking back there will probably need to be a moment in the turn steps (Resolving board?) where the animations are handled
                //and the state of the board is transitioned from init to end.

        //I feel like move() should be called once and then the animation will have to handle the transition or something

        //moves just 1 time
        if (inputReceived) {
            board.move(move);               
        }

        board.update(delta); //does nothing now


        //the movement has to be processed to understand if the move is valid so this check has to be made here.
        //in case the move fails the phase goes back to Begin
        if (board.moveIsPermitted()) {

            if (board.animationFinished()) {
                nextPhase(); // Proceed to Phase::BoardResolution
            }
        }
        else { 
            currentPhase = Phase::Begin;
            move = Direction::None; //backs to default (hardset)
            inputReceived = false; //backs to default (hardset)

            std::cout << "[FSM] Move invalid: Transitioning from Movement to Begin" << std::endl;
        }

        break;
    case Phase::BoardResolution:

        board.spawnTileInRandomEmptySlot();
        nextPhase();

        break;
    case Phase::End:

        if (not shopRequested) { nextPhase(); }                                     //if shop is not requested you can directly skip turn
        else if (not game_run->getPlayState()->isAnimationEmpty()) { break; }       //if it is but ani is still going stay in turn but loop
        else {                                                                      
            game_run->openShop();
            shopRequested = 0;                                                      //else just open shop
        }
        if (not game_run->shopOpen) {nextPhase();}                                  //and skip turn only when it closes.
        break;

    default:
        break;
    }
}

void Turn::render(RenderSystem& renderer) {

    board.render(renderer);
}

void Turn::handleInput(sf::Event& event) {

    switch (currentPhase) {
    case Phase::Begin:

        handleInput_BeginPhase(event);

        break;
    case Phase::SelectingConsumables:

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

        switch (keyPressed->scancode) { //sets inputReceived true-> update() -> calls nextPhase()
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
