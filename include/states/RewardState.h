#pragma once

#include "states/GameState.h"
#include "rendering/UI_Button.h"

#include <SFML/Graphics.hpp>
#include <functional>
#include <vector>

class StateManager;
class RenderSystem;
class GameRun;

// The post-defeat reward picker — a modal overlay, the twin of ShopState. It
// draws the frozen play screen behind it (renderBehind) and shows the run's
// pending reward options; one click takes one. Picking grants via
// GameRun::pickReward and closes; ESC skips (GameRun::dismissReward). The world
// below is NOT updated while it is open, so it pauses the turn loop the same way
// the shop does.
class RewardState : public GameState {
public:
    using RenderBehind = std::function<void(RenderSystem&)>;

    RewardState(StateManager& stateManager, RenderSystem& renderer, GameRun* gameRun,
                RenderBehind renderBehind);

    void enter() override;
    void exit() override;
    void handleInput(sf::Event& event) override;
    void update(float deltaTime) override;
    void render(RenderSystem& renderer) override;

private:
    void buildButtons();
    void renderHoveredTooltip(RenderSystem& renderer);

    StateManager& stateManager;
    RenderSystem& renderer;
    GameRun* gameRun;
    RenderBehind renderBehind;

    // One icon button per offered option. Same deferred-pick machine as the
    // shop's deferred buy: a click records the index, applied in update() so the
    // button vector is never mutated mid-iteration.
    std::vector<UI_Button> optionButtons;
    int pendingPickIndex = -1;
};
