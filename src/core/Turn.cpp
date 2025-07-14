#include "core/Turn.h"
#include "core/GameRun.h"
#include "core/Board.h"

Turn::Turn(GameRun* game_run) :
    game_run(game_run),
    board(this)
{}


Turn::Turn(GameRun* game_run,const Board& initial_board) :
    game_run(game_run),
    board(initial_board, this)
{
}


void Turn::nextPhase() {

	std::cout << "Next Phase!" << std::endl;

}

void Turn::end_turn() {

    std::cout << "Turn ended" << std::endl;

    game_run->new_turn(this->board);
}

void Turn::requestShop() {

    game_run->openShop(); // delega a GameRun, to avoid multiple shops by consecutive merges. (possible feature?)
}


void Turn::update(float deltaTime) {

    //called every gameloop (fast) -> does nothing at the moment but will be used to animate board.

}

void Turn::handleInput(sf::Event& event) {

    if (const auto* keyPressed = event.getIf<sf::Event::KeyReleased>()) {

        if (keyPressed->scancode == sf::Keyboard::Scancode::X) {
            board.spawnTileInRandomEmptySlot();
        }
        else if (keyPressed->scancode == sf::Keyboard::Scancode::A) {

            if (board.moveLeft()) { board.spawnTileInRandomEmptySlot();

            move = Direction::Left;
            
            }
            else { std::cout << "move invalid" << std::endl; return;}
        }
        else if (keyPressed->scancode == sf::Keyboard::Scancode::D) {
            if (board.moveRight()) { board.spawnTileInRandomEmptySlot();
            
            move = Direction::Right;

            }
            else { std::cout << "move invalid" << std::endl; return; }
        }
        else if (keyPressed->scancode == sf::Keyboard::Scancode::W) {
            if (board.moveUp()) { board.spawnTileInRandomEmptySlot();
            
            move = Direction::Up;

            }
            else { std::cout << "move invalid" << std::endl; return;}
        }
        else if (keyPressed->scancode == sf::Keyboard::Scancode::S) {
            if (board.moveDown()) { board.spawnTileInRandomEmptySlot();
            
            move = Direction::Down;

            }
            else { std::cout << "move invalid" << std::endl; return;}
        }
        else if (keyPressed->scancode == sf::Keyboard::Scancode::Delete) {
            board.clear();
            board.spawnTileInRandomEmptySlot();
        }
        else if (keyPressed->scancode == sf::Keyboard::Scancode::B) {
            game_run->go_back();
            return;
        }


        end_turn();

    }

}