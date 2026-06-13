#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include "rendering/UI_Button.h"

class RenderSystem;
class GameRun;
class Boss;

// The play-screen UI layer: full-screen backdrop, HUD counters, and the two
// side columns — consumable items on the RIGHT, owned cards on the LEFT — each
// framed by a themed panel, with selection + action buttons. It OWNS the
// screen-space widgets and reads/commands the GameRun MODEL (which keeps the
// run state). Owned by PlayState.
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

    // Screen-space hit-test over the inventory/card/action buttons (raw pixel
    // point). Lets PlayState give the UI priority over the board for pan/selection.
    bool isPointOverUI(sf::Vector2f screenPoint) const;

private:
    // Rebuild the buttons only when the model actually changed (cheap change
    // detection vs. an explicit GameRun->PlayUI notification, which would re-couple).
    void syncButtonsToModel();
    void rebuildInventoryButtons();
    void rebuildActionButtons();
    void rebuildCardButtons();
    void rebuildCardActionButtons();
    // Themed frame + title behind each side column (drawn before its buttons).
    void drawColumnPanel(RenderSystem& r, float centerX, const char* title);
    // Boss-fight banner (boss-design §10): name + HP bar, top-center, drawn
    // while a boss is on the acting board (read through GameRun, like the
    // countdown).
    void drawBossBanner(RenderSystem& r, const Boss& boss);
    // Tooltips for the item/card under the cursor (if any).
    void renderInventoryTooltip(RenderSystem& r);
    void renderCardsTooltip(RenderSystem& r);
    void drawDigitCounter(RenderSystem& r, unsigned int value, float xOffset,
                          float y = 18.f, float scale = 10.f);

    RenderSystem& renderer;
    GameRun& run;

    sf::Sprite backUI;
    std::vector<UI_Button> inventoryButtons;
    std::vector<UI_Button> actionButtons;

    // Cards column (left). cardModelIndex maps button i -> index into
    // GameRun::getOwnedCards(): engine-level cards without a registry def have
    // nothing to show and are skipped, so the two indexings can diverge.
    std::vector<UI_Button> cardButtons;
    std::vector<size_t> cardModelIndex;
    std::vector<UI_Button> cardActionButtons;  // discard only — cards are passive

    // Deferred actions — set by button callbacks, processed after the button
    // update loop to avoid invalidating the vectors mid-iteration.
    int pendingSelectIndex = -1;
    enum class PendingAction { None, Use, Discard };
    PendingAction pendingAction = PendingAction::None;
    int pendingCardSelect = -1;        // model index (via cardModelIndex)
    bool pendingCardDiscard = false;

    // Debug-mode toggle (debug builds only — never shown in release): flips
    // debug::active() so the real game/shop can be play-tested without a
    // rebuild, and back. A plain rect + text rather than a UI_Button because
    // it is label-drawn, not texture-backed; click semantics mirror
    // UI_Button's (fires on release only if the press started on it).
    sf::FloatRect debugToggleRect;
    bool debugTogglePressStarted = false;
    bool debugMouseWasDown = false;

    // Last observed model state, to detect when a rebuild is needed. Content
    // is tracked through GameRun's version counters — NOT by list size, which
    // misses a same-size replacement (an item consumed while a reactor grants
    // another in the same frame). Selection is tracked by the index itself.
    // 0 matches the model's "never mutated" state, so a fresh PlayUI starts
    // consistent (empty lists, empty buttons) without a forced first rebuild.
    unsigned int lastInventoryVersion = 0;
    int lastSelectedIndex = -1;
    unsigned int lastCardsVersion = 0;
    int lastSelectedCardIndex = -1;
};
