#include <memory>
#include <iostream>
#include "core/Gamerun.h"
#include "states/PlayState.h"
#include "states/ShopState.h"

GameRun::GameRun(sf::RenderWindow& window, PlayState* playState) :
    window(window),
    playState(playState)
{
    auto rd = std::random_device{};
    randomSeed = rd();
    rng.seed(randomSeed);

    gameStarted = true;

    run_turns.push(std::make_unique<Turn>(this));

}

void GameRun::enter() {

	std::cout << "currentRun entered" << std::endl;

}

void GameRun::exit() {

    std::cout << "currentRun exited" << std::endl;

}


void GameRun::new_turn(const Board& initial_board) {
    run_turns.push(std::make_unique<Turn>(this, initial_board));

    std::cout << "Turn" << run_turns.size() << std::endl;

}

void GameRun::go_back() {
    if (run_turns.size() <= 1) {
        std::cout << "Nessun turno precedente disponibile." << std::endl;
        return;
    }

    // Rimuove l'ultimo turno
    run_turns.pop();
    std::cout << "Turno Rimosso - Tornato al Turno : " << run_turns.size()  << std::endl;

}

void GameRun::openShop() {
    std::cout << "Opening shop...\n";
    playState->stateManager.pushState(std::make_unique<ShopState>(playState->stateManager, playState->window, this));
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


    if (!run_turns.empty()) {
        run_turns.top()->handleInput(event);
    }

}

void GameRun::update(float deltaTime) {

    //called every gameloop (fast) -> does nothing at the moment but will be used to animate board.
    
}

void GameRun::render(sf::RenderWindow& window) {

    if (!run_turns.empty()) {
        run_turns.top()->board.render(window);
    }

}
int GameRun::getRandomInt(int min, int max) {
    std::uniform_int_distribution<int> dist(min, max);
    return dist(rng);
}

float GameRun::getRandomFloat(float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(rng);
}

