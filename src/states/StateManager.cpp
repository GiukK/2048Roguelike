#include "states/StateManager.h"
#include "rendering/RenderSystem.h"

void StateManager::pushState(std::unique_ptr<GameState> state) {
    states.push(std::move(state));

    std::cout << "size of stack is " << states.size() << std::endl;

}

void StateManager::popState() {
    if (!states.empty()) {

        //states.top()->exit();

        std::cout << "size of stack before pop is " << states.size() << std::endl;


        states.pop(); //careful in handling exit in states:   1) state.exit() -> 2) stateManager.popState() to avoid recursion

        std::cout << "size of stack after pop is " << states.size() << std::endl;

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

void StateManager::requestPop() {
    pendingOps.push_back(Op::Pop);
}

void StateManager::applyPending() {
    if (pendingOps.empty()) return;
    for (auto op : pendingOps) {
        if (op == Op::Pop) popState(); // actually perform it now, safely
    }
    pendingOps.clear();
}



//-----

void StateManager::handleInput(sf::Event& event) {
    if (!states.empty()) {
        states.top()->handleInput(event);
    }
}

void StateManager::update(float deltaTime) {
    if (!states.empty()) {
        states.top()->update(deltaTime);
    }

    applyPending(); //can pop state at the end, without risking to access freed memory
}

void StateManager::render(RenderSystem& renderer) {
    if (!states.empty()) {
        states.top()->render(renderer);
    }
}
