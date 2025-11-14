#pragma once

#include <iostream>
#include <memory>
#include <stack>
#include <SFML/Graphics.hpp>

#include "rendering/RenderSystem.h"
#include <states/GameState.h>



class StateManager {
public:

    void pushState(std::unique_ptr<GameState> state);
    void popState();            // (kept, but prefer requestPop from states)
    //void changeState(std::unique_ptr<GameState> state);

    void requestPop();          // <— NEW: defer the pop


    void handleInput(sf::Event& event);
    void update(float deltaTime);
    void render(RenderSystem& renderer);

    bool isEmpty() const { return states.empty(); }

private:

    //new request feature
    enum class Op { Pop };
    std::vector<Op> pendingOps;

    void applyPending();        


    std::stack<std::unique_ptr<GameState>> states;
};
