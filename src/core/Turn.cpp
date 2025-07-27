#include "core/Turn.h"
#include "core/GameRun.h"
#include "core/Board.h"
#include "core/utils/Direction.h"
#include "core/utils/saleItem.h"



#include "rendering/RenderSystem.h"


Turn::Turn(RenderSystem& renderer , GameRun* game_run) :
    renderer(renderer),
    game_run(game_run),
    board(renderer , this),
    boardBegin(Board::cloneFrom(board, this)),


    use_button(renderer.getTextureManager().get("use_button")) ////<------------------------------------- FIX IN THE NEXT VERSION

{
    std::cout << " -------------------------------  Turn 1  -------------------------------" << std::endl;
}


Turn::Turn(RenderSystem& renderer , GameRun* game_run,const Board& initial_board) :
    renderer(renderer),
    game_run(game_run),
    board(Board::cloneFrom(initial_board, this)),
    boardBegin(Board::cloneFrom(initial_board, this)),


    use_button(renderer.getTextureManager().get("use_button")) ////<------------------------------------- FIX IN THE NEXT VERSION

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

    std::cout << "Turn ended with move: " << dirToString(move) << std::endl;

    game_run->new_turn(this->board);

    currentPhase = Phase::Begin;

    move = Direction::None; //backs to default
    inputReceived = false; //backs to default for future possible go_backs
                            //moved from update()->case Movement -> after move.

    board.copyStateFrom(boardBegin); //useless reversing internals because of this(?)

}

void Turn::requestShop() {

    game_run->openShop(); // managed by GameRun, to avoid multiple shops by consecutive merges. (possible feature?)
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
            nextPhase();
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

        end_turn();
        break;

    default:
        break;
    }
}

void Turn::render(RenderSystem& renderer) {

    board.render(renderer);

    if (itemWasSelected) { renderer.draw(use_button); }
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

    
    //in the future it will be nice to have down-up button animation when pushed in sync with onRelease and isPressed
    if (const auto* mouseReleased = event.getIf<sf::Event::MouseButtonReleased>())
    {
        if (mouseReleased->button == sf::Mouse::Button::Left) {
            auto& window = renderer.getWindow();

            sf::Vector2i mousePos = sf::Mouse::getPosition(window);  // da PlayState o GameRun
            sf::Vector2f worldPos = window.mapPixelToCoords(mousePos);

            //board.handleClick(worldPos); //works fine but its just a test

            //manage inventory effects (temporary) -----------------------------------------------------------------


            for (auto& item : game_run->getInventory()) {
                sf::FloatRect bounds = item.sprite.getGlobalBounds();

                if (bounds.contains(worldPos)) {

                    //temporary UI for fast test
                    if (!itemWasSelected) {
                        // Applica filtro rosso
                        item.sprite.setColor(sf::Color(255, 100, 100));  // leggermente trasparente rosso

                        //currentPhase = Phase::SelectingConsumables
                        //itemSelected(item);
                        // 
                        //temporary item management
                        use_button.setPosition({ item.sprite.getPosition().x + 100.f, item.sprite.getPosition().y });
                        use_button.setScale({ 2.f, 2.f });
                        itemWasSelected = true;


                        std::cout << "item set to red\n";


                        return;
                    }
                    else {

                        item.sprite.setColor(sf::Color(255, 255, 255));
                        itemWasSelected = false;

                        std::cout << "item set to white\n";


                        return;
                    }

                }
                else {

                    item.sprite.setColor(sf::Color(255, 255, 255));
                }
            }

            sf::FloatRect bbounds = use_button.getGlobalBounds();  //<<<

            if (itemWasSelected and bbounds.contains(worldPos)) {

                std::cout << "item used\n";



            }
            else if (itemWasSelected and not bbounds.contains(worldPos)) {

                itemWasSelected = false;

                std::cout << "item selected but outside button -> d\n";

            }


            //-------------------------------------------------------------------------------------------------------
        }
    }
}
