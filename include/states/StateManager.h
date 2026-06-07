#pragma once

#include <memory>
#include <stack>
#include <vector>

#include <SFML/Graphics.hpp>
#include "states/GameState.h"

class RenderSystem;

class StateManager {
public:
    void pushState(std::unique_ptr<GameState> state);
    void popState();
    void requestPop();

    void handleInput(sf::Event& event);
    void update(float deltaTime);
    void render(RenderSystem& renderer);

    bool isEmpty() const { return states.empty(); }

private:
    void applyPending();

    enum class PendingOp { Pop };
    std::vector<PendingOp> pendingOps;
    std::stack<std::unique_ptr<GameState>> states;
};
