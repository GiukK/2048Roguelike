#include "core/Gamerun.h"
#include "core/Tile.h"

#include <memory>
#include <iostream>

GameRun::GameRun(sf::RenderWindow& window) :
    window(window)
{
    auto rd = std::random_device{};
    randomSeed = rd();
    rng.seed(randomSeed);

    board.setRng(rng);

    board.start();

    gameStarted = true;

}

void GameRun::enter() {

	std::cout << "currentRun entered" << std::endl;

}

void GameRun::exit() {

    std::cout << "currentRun exited" << std::endl;

}

void GameRun::addScore(int score) {
    score += score;
    std::cout << "Score added: " << score << " | Total Score: " << score << std::endl;
}

void GameRun::subtractScore(int score) {
    score -= score;
    if (score < 0) {
        score = 0; // Ensuring score doesn't go negative
    }
    std::cout << "Score subtracted: " << score << " | Total Score: " << score << std::endl;
}

void GameRun::handleInput(sf::Event& event) {

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
    }

}

void GameRun::update(float deltaTime) {

    //called every gameloop (fast) -> does nothing at the moment but will be used to animate board.
    
}

void GameRun::render(sf::RenderWindow& window) {

    board.render(window);

}
int GameRun::getRandomInt(int min, int max) {
    std::uniform_int_distribution<int> dist(min, max);
    return dist(rng);
}

float GameRun::getRandomFloat(float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(rng);
}

