#pragma once

#include "states/GameState.h"
#include "rendering/UI_Button.h"

#include <SFML/Graphics.hpp>
#include <functional>
#include <vector>

class StateManager;
class RenderSystem;
class GameRun;

// Terminal modal shown when the run is defeated (GameRun's defeat callback —
// the board locked with no legal move, docs/boss-design.md §8). Like the shop
// it draws the play screen behind itself as a frozen backdrop, so the player
// sees the final position that ended the run. UNLIKE the shop it is one-way:
// the only exit returns to the main menu, popping the dead PlayState too —
// no rewind, no item rescue, the run is over.
class GameOverState : public GameState {
public:
    using RenderBehind = std::function<void(RenderSystem&)>;

    GameOverState(StateManager& stateManager, RenderSystem& renderer, GameRun* gameRun,
                  RenderBehind renderBehind);

    void enter() override;
    void exit() override;  // back to the menu: pops this overlay AND PlayState
    void handleInput(sf::Event& event) override;
    void update(float deltaTime) override;
    void render(RenderSystem& renderer) override;

private:
    StateManager& stateManager;
    RenderSystem& renderer;
    // Kept for run stats today and a future "new run" button; the screen never
    // mutates the run.
    GameRun* gameRun;
    RenderBehind renderBehind;

    // Run epitaph, captured at entry (the world behind is frozen, but capturing
    // documents that these are THIS death's numbers, not live queries).
    size_t turnsSurvived;
    int bestTile;

    std::vector<UI_Button> buttons;
};
