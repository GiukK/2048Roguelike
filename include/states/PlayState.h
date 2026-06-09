#pragma once

#include "states/GameState.h"
#include "states/PlayUI.h"
#include "core/GameRun.h"
#include "rendering/UI_Button.h"
#include "rendering/Animation.h"

#include <SFML/Graphics.hpp>
#include <memory>
#include <vector>

class StateManager;
class RenderSystem;

class PlayState : public GameState {
public:
    PlayState(StateManager& stateManager, RenderSystem& renderer);

    void enter() override;
    void exit() override;
    void handleInput(sf::Event& event) override;
    void update(float deltaTime) override;
    void render(RenderSystem& renderer) override;

    void addAnimation(std::unique_ptr<Animation> anim);
    bool hasActiveAnimations() const { return !animations.empty(); }

    StateManager& stateManager;

private:
    // True if `pixel` is over any play-screen UI widget (exit button + PlayUI's
    // inventory/action buttons). The UI takes priority over the board there: a
    // press on it won't start a pan, and a right-click on it won't select a tile.
    bool isPointOverUI(sf::Vector2i pixel) const;

    // Update / render the world (board) + HUD. Used by PlayState itself and,
    // through callbacks, by ShopState so the play screen stays live behind the
    // shop overlay (ShopState is the top state, so PlayState isn't ticked then).
    void updateWorldAndHud(float dt);
    void renderWorldAndHud(RenderSystem& r);

    RenderSystem& renderer;
    std::unique_ptr<GameRun> currentRun;
    std::unique_ptr<PlayUI> playUI;
    std::vector<UI_Button> buttons;
    std::vector<std::unique_ptr<Animation>> animations;

    // Left-drag camera pan. Tile selection is on the RIGHT button, so the left
    // button is purely the pan handle (no click-vs-select disambiguation). The
    // threshold only stops a shaky click from producing a tiny pan.
    static constexpr int DragThresholdPixels = 6;
    bool leftButtonDown = false;
    bool isDraggingBoard = false;
    sf::Vector2i pressPixel{};
    sf::Vector2i lastDragPixel{};
};
