#include "states/StateManager.h"
#include "rendering/RenderSystem.h"

void StateManager::pushState(std::unique_ptr<GameState> state) {
    states.push(std::move(state));
}

void StateManager::popState() {
    if (!states.empty()) {
        states.pop();
    }
}

void StateManager::requestPop() {
    pendingOps.push_back(PendingOp::Pop);
}

void StateManager::applyPending() {
    for (auto op : pendingOps) {
        if (op == PendingOp::Pop) {
            popState();
        }
    }
    pendingOps.clear();
}

void StateManager::handleInput(sf::Event& event) {
    if (!states.empty()) {
        states.top()->handleInput(event);
    }
}

void StateManager::update(float deltaTime) {
    if (!states.empty()) {
        states.top()->update(deltaTime);
    }
    applyPending();
}

void StateManager::render(RenderSystem& renderer) {
    if (!states.empty()) {
        states.top()->render(renderer);
    }
}
