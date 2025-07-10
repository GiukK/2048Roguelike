#include "core/Turn.h"
#include "core/GameRun.h"

Turn::Turn(GameRun* game_run, Board initial_board):
    game_run(game_run),
    board(initial_board)
{

}

void Turn::announce_event() {

	std::cout << "EVENT!" << std::endl;

}

void Turn::end_turn() {

    std::cout << "Turn ended" << std::endl;

    game_run->new_turn(this->board);
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
            board.moveLeft();
            board.spawnTileInRandomEmptySlot();
        }
        else if (keyPressed->scancode == sf::Keyboard::Scancode::D) {
            board.moveRight();
            board.spawnTileInRandomEmptySlot();
        }
        else if (keyPressed->scancode == sf::Keyboard::Scancode::W) {
            board.moveUp();
            board.spawnTileInRandomEmptySlot();
        }
        else if (keyPressed->scancode == sf::Keyboard::Scancode::S) {
            board.moveDown();
            board.spawnTileInRandomEmptySlot();
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