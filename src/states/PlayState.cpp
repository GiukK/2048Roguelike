#include "states/PlayState.h"
#include <iostream>

PlayState::PlayState(StateManager& stateManager, sf::RenderWindow& window) :
    stateManager(stateManager),
    window(window),
    currentRun(std::make_unique<GameRun>(window))
{


    enter(); // Open game
}

void PlayState::enter() {
    std::cout << "Entering Play State - PlayState::enter()" << std::endl;

    currentRun->enter();

    std::cout << "Entering current GameRun" << std::endl;
}

void PlayState::exit() {
    std::cout << "exit() was called from PlayState - PlayState::exit()" << std::endl;

    stateManager.popState();
}

void PlayState::handleInput(sf::Event& event) {
    if (const auto* keyPressed = event.getIf<sf::Event::KeyReleased>()) {
        if (keyPressed->scancode == sf::Keyboard::Scancode::Enter) {
            std::cout << "Starting Game..." << std::endl;
            // TODO: Transition to PlayState
        }
        else if (keyPressed->scancode == sf::Keyboard::Scancode::Escape) {
            std::cout << "Exiting Game..." << std::endl;


            exit();  // Close game
            return;
        }
    }

    currentRun->handleInput(event);
}

void PlayState::update(float deltaTime) {

    currentRun->update(deltaTime);

}

void PlayState::render(sf::RenderWindow& window) {

    //Text UI deprecated
    currentRun->render(window);
}
