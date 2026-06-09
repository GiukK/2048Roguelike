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
    // The shop is a modal overlay. Since only the top state ticks/renders,
    // ShopState keeps the play screen behind it live via these callbacks
    // (provided by PlayState, which owns the world + HUD).
    using UpdateBehind = std::function<void(float)>;
    using RenderBehind = std::function<void(RenderSystem&)>;

    ShopState(StateManager& stateManager, RenderSystem& renderer, GameRun* gameRun,
              UpdateBehind updateBehind, RenderBehind renderBehind);

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

    StateManager& stateManager;
    RenderSystem& renderer;
    GameRun* gameRun;
    UpdateBehind updateBehind;
    RenderBehind renderBehind;

    sf::Sprite shopSprite;
    std::vector<Animation> decorAnimations;

    std::vector<std::string> shopItemIds;
    std::vector<UI_Button> shopButtons;
    int pendingBuyIndex = -1;

    static constexpr int SHOP_SLOTS = 3;
};
