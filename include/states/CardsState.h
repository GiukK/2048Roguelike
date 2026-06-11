#pragma once

#include "states/GameState.h"
#include "rendering/UI_Button.h"

#include <SFML/Graphics.hpp>
#include <functional>
#include <vector>

class StateManager;
class RenderSystem;
class GameRun;

// The Cards panel: a read-only modal overlay listing the player's owned cards
// (run-scoped passives), opened from the play screen's cards button. Same modal
// contract as ShopState: the play screen is drawn behind as a frozen backdrop
// and the world below is NOT updated while the panel is open.
class CardsState : public GameState {
public:
    using RenderBehind = std::function<void(RenderSystem&)>;

    CardsState(StateManager& stateManager, RenderSystem& renderer, GameRun* gameRun,
               RenderBehind renderBehind);

    void enter() override;
    void exit() override;
    void handleInput(sf::Event& event) override;
    void update(float deltaTime) override;
    void render(RenderSystem& renderer) override;

private:
    void rebuildCardButtons();
    // Tooltip (name + description, no price — already owned) on hover.
    void renderHoveredTooltip(RenderSystem& renderer);

    StateManager& stateManager;
    RenderSystem& renderer;
    GameRun* gameRun;
    RenderBehind renderBehind;

    // UI_Button gives hover feedback + the contains() hit-test the tooltip
    // needs; clicking an owned card does nothing (yet).
    std::vector<UI_Button> cardButtons;
};
