#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include "rendering/UI_Button.h"

class RenderSystem;
class GameRun;

// The play-screen UI layer: full-screen backdrop, HUD counters, and the
// inventory / action buttons. It OWNS the screen-space widgets and reads/commands
// the GameRun MODEL (which keeps the run state). Owned by PlayState.
//
// This is the seam the future data-driven UI framework will grow into: the
// UI_Button widgets here become UINodes, and isPointOverUI is subsumed by the
// framework's input pass — without GameRun ever knowing about any of it.
class PlayUI {
public:
    PlayUI(RenderSystem& renderer, GameRun& run);

    void update(float dt);                  // widget update + deferred select/use/discard
    void renderBackground(RenderSystem& r); // full-screen UI backdrop (UI view)
    void renderForeground(RenderSystem& r); // HUD counters + buttons + tooltip (UI view)

    // Screen-space hit-test over the inventory/action buttons (raw pixel point).
    // Lets PlayState give the UI priority over the board for pan/selection.
    bool isPointOverUI(sf::Vector2f screenPoint) const;

private:
    // Rebuild the buttons only when the model actually changed (cheap change
    // detection vs. an explicit GameRun->PlayUI notification, which would re-couple).
    void syncButtonsToModel();
    void rebuildInventoryButtons();
    void rebuildActionButtons();
    // Draws the tooltip for the inventory item currently under the cursor (if any).
    void renderInventoryTooltip(RenderSystem& r);
    void drawDigitCounter(RenderSystem& r, unsigned int value, float xOffset,
                          float y = 18.f, float scale = 10.f);

    RenderSystem& renderer;
    GameRun& run;

    sf::Sprite backUI;
    std::vector<UI_Button> inventoryButtons;
    std::vector<UI_Button> actionButtons;

    // Deferred actions — set by button callbacks, processed after the button
    // update loop to avoid invalidating the vectors mid-iteration.
    int pendingSelectIndex = -1;
    enum class PendingAction { None, Use, Discard };
    PendingAction pendingAction = PendingAction::None;

    // Last observed model state, to detect when a rebuild is needed.
    int lastInventorySize = -1;
    int lastSelectedIndex = -1;
};
