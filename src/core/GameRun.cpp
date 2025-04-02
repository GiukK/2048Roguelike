#include "core/Gamerun.h"
#include "core/Tile.h"

#include <memory>
#include <iostream>

#include <random>

int getRandomNumber();



GameRun::GameRun(sf::RenderWindow& window) :
    window(window),
    boardTexture("assets/textures/board.png", false, sf::IntRect({ 0, 0 }, { 66, 66 })),
    board(boardTexture)
{

    board.setOrigin({ 33.f,33.f });
    board.setPosition({ window.getSize().x / 2.f , window.getSize().y / 2.f });

    board.setScale({ 10.f, 10.f });


}

void GameRun::enter() {

	std::cout << "currentRun entered." << std::endl;

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
        if (keyPressed->scancode == sf::Keyboard::Scancode::Z) {
            std::cout << "GameRun input triggered = create" << std::endl;

            std::unique_ptr<Tile> tile = std::make_unique<Tile>();

            tile->spawn(board, getRandomNumber(), getRandomNumber());

            tiles.push_back(std::move(tile));

        }
        else if (keyPressed->scancode == sf::Keyboard::Scancode::X) {
            std::cout << "GameRun input triggered = delete" << std::endl;

            if (!tiles.empty()) { tiles.pop_back(); }
        }
    }


}

void GameRun::update(float deltaTime) {}

void GameRun::render(sf::RenderWindow& window) {

    window.draw(board);

    for (const std::unique_ptr<Tile>& tile : tiles) {
        tile->render(window);
    }

}


int getRandomNumber() {
    static std::random_device rd;  // Seed for randomness
    static std::mt19937 gen(rd()); // Mersenne Twister PRNG
    std::uniform_int_distribution<int> distrib(1, 4); // Range [1, 4]

    return distrib(gen);
}