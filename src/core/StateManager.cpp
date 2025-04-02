#include "core/StateManager.h"

void StateManager::pushState(std::unique_ptr<GameState> state) {
    states.push(std::move(state));
}

void StateManager::popState() {
    if (!states.empty()) {
        //states.top()->exit();
        states.pop(); //careful in handling exit in states:   1) state.exit() -> 2) stateManager.popState() to avoid recursion
    }
}

/*
void StateManager::changeState(std::unique_ptr<GameState> state) { //?? weird and probably unsafe to use
    while (!states.empty()) {
        states.top()->exit();
        states.pop();
    }
    pushState(std::move(state));
}
*/

void StateManager::handleInput(sf::Event& event) {
    if (!states.empty()) {
        states.top()->handleInput(event);
    }
}

void StateManager::update(float deltaTime) {
    if (!states.empty()) {
        states.top()->update(deltaTime);
    }
}

void StateManager::render(sf::RenderWindow& window) {
    if (!states.empty()) {
        states.top()->render(window);
    }
}
