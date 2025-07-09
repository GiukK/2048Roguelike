#include "core/Gamerun.h"

#include <memory>
#include <iostream>

GameRun::GameRun(sf::RenderWindow& window) :
    window(window)
{
    auto rd = std::random_device{};
    randomSeed = rd();
    rng.seed(randomSeed);

    gameStarted = true;

    run_turns.push_back(std::make_unique<Turn>(this));
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

    run_turns.back()->handleInput(event);

}

void GameRun::update(float deltaTime) {

    //called every gameloop (fast) -> does nothing at the moment but will be used to animate board.
    
}

void GameRun::render(sf::RenderWindow& window) {

    run_turns.back()->board.render(window);

}
int GameRun::getRandomInt(int min, int max) {
    std::uniform_int_distribution<int> dist(min, max);
    return dist(rng);
}

float GameRun::getRandomFloat(float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(rng);
}

