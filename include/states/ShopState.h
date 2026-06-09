#pragma once

#include "states/GameState.h"
#include "rendering/UI_Button.h"
#include "rendering/Animation.h"

#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <functional>

class StateManager;
class RenderSystem;
class GameRun;

class ShopState : public GameState {
public:
    // The shop is a modal overlay. Only the top state renders, so PlayState gives
    // ShopState a callback to draw the play screen behind it as a FROZEN backdrop.
    // The world below is intentionally NOT updated while the shop is open.
    using RenderBehind = std::function<void(RenderSystem&)>;

    ShopState(StateManager& stateManager, RenderSystem& renderer, GameRun* gameRun,
              RenderBehind renderBehind);

    void enter() override;
    void exit() override;
    void handleInput(sf::Event& event) override;
    void update(float deltaTime) override;
    void render(RenderSystem& renderer) override;

private:
    void initVisuals();
    void generateShop();
    void buyItem(size_t index);
    void rebuildShopButtons();
    // Tooltip (name + description + price) for the shop item under the cursor.
    void renderHoveredTooltip(RenderSystem& renderer);

    StateManager& stateManager;
    RenderSystem& renderer;
    GameRun* gameRun;
    RenderBehind renderBehind;

    sf::Sprite shopSprite;
    std::vector<Animation> decorAnimations;

    std::vector<std::string> shopItemIds;
    std::vector<UI_Button> shopButtons;
    int pendingBuyIndex = -1;

    static constexpr int SHOP_SLOTS = 3;
};
