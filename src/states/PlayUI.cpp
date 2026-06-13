#include "states/PlayUI.h"
#include "states/ItemTooltip.h"
#include "core/Boss.h"
#include "core/GameRun.h"
#include "core/ItemRegistry.h"
#include "core/CardRegistry.h"
#include "rendering/RenderSystem.h"
#include "ui/UI.h"
#include "Debug.h"

#include <algorithm>
#include <string>

namespace {
// HUD layout, screen-space (drawn in the UI view). Hardcoded for now; moves into
// the data-driven UI layer later. The two side columns mirror each other:
// consumable items on the right, owned cards on the left; each column's row
// geometry is shared so its action buttons line up with the selected entry.
constexpr float TurnCounterX       = 350.f;   // top-left turn count
constexpr float CoinsCounterX      = 1380.f;  // top-right coin count
constexpr float ShopCountdownX     = 350.f;   // below the turn count
constexpr float ShopCountdownY     = 100.f;
constexpr float ShopCountdownScale = 7.f;
constexpr float AnteCountdownY     = 170.f;   // below the shop countdown: the boss clock
constexpr float InventoryX         = 1500.f;  // right-side inventory column
constexpr float InventoryTopY      = 350.f;   // first item's Y
constexpr float InventorySpacingY  = 200.f;   // vertical gap between items
constexpr float ActionButtonX      = 1350.f;  // use/discard column (left of items)
constexpr float ActionButtonGapY   = 55.f;    // offset of use/discard from item Y
constexpr float CardsX             = 180.f;   // left-side cards column
constexpr float CardsTopY          = 350.f;   // mirrors the inventory rows
constexpr float CardsSpacingY      = 200.f;
constexpr float CardMaxWidth       = 130.f;   // keeps the 31x41 card inside its row
constexpr float CardActionX        = 330.f;   // discard button, board-side of the card
// Themed frame around each column ("Items" / "Cards"): half-width and the
// vertical band covering the 3 rows plus headroom for the title.
constexpr float PanelHalfW         = 120.f;
constexpr float PanelTop           = 245.f;
constexpr float PanelHeight        = 630.f;
// Boss-fight banner: top-center, clear of the corner counters.
constexpr float BossBannerW        = 520.f;
constexpr float BossBannerH        = 96.f;
constexpr float BossBannerY        = 16.f;
// Debug toggle: bottom-left corner, below the cards panel's band.
constexpr float DebugToggleX       = 40.f;
constexpr float DebugToggleY       = 1000.f;
constexpr float DebugToggleW       = 190.f;
constexpr float DebugToggleH       = 48.f;
} // namespace

PlayUI::PlayUI(RenderSystem& renderer, GameRun& run)
    : renderer(renderer),
      run(run),
      backUI(renderer.getTextureManager().get("backUI")),
      debugToggleRect({DebugToggleX, DebugToggleY}, {DebugToggleW, DebugToggleH})
{
    renderer.resizeSprite("backUI", backUI);
}

void PlayUI::update(float dt) {
    for (auto& btn : inventoryButtons) btn.update(dt);
    for (auto& btn : actionButtons) btn.update(dt);
    for (auto& btn : cardButtons) btn.update(dt);
    for (auto& btn : cardActionButtons) btn.update(dt);

    // Debug-mode toggle: polled like UI_Button does (press must START on the
    // widget, fire on release over it) so a board pan dragged across it can
    // never flip the mode. Compiled out of release builds with debug::Enabled.
    if (debug::Enabled) {
        const bool down = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
        const sf::Vector2f p = renderer.mapPixelToUI(
            sf::Mouse::getPosition(renderer.getWindow()));
        const bool over = debugToggleRect.contains(p);

        if (down && !debugMouseWasDown) {
            debugTogglePressStarted = over;
        }
        if (!down && debugMouseWasDown) {
            if (debugTogglePressStarted && over) {
                debug::setActive(!debug::active());
            }
            debugTogglePressStarted = false;
        }
        debugMouseWasDown = down;
    }

    // Deferred selection toggle (set by an inventory button click).
    if (pendingSelectIndex >= 0) {
        run.toggleSelectedItem(pendingSelectIndex);
        pendingSelectIndex = -1;
    }

    // Deferred use/discard of the held item (set by an action button click).
    if (pendingAction != PendingAction::None) {
        if (pendingAction == PendingAction::Use) run.useHeldItem();
        else                                     run.discardHeldItem();
        pendingAction = PendingAction::None;
    }

    // Same deferred machine for the cards column.
    if (pendingCardSelect >= 0) {
        run.toggleSelectedCard(pendingCardSelect);
        pendingCardSelect = -1;
    }
    if (pendingCardDiscard) {
        run.discardSelectedCard();
        pendingCardDiscard = false;
    }

    // Rebuild AFTER processing, so a model change this frame is reflected this
    // frame. Also catches inventory changes that happened while we weren't
    // updating (e.g. a shop purchase, since update is driven by PlayState).
    syncButtonsToModel();
}

void PlayUI::syncButtonsToModel() {
    unsigned int invVersion = run.getInventoryVersion();
    if (invVersion != lastInventoryVersion) {
        rebuildInventoryButtons();
        lastInventoryVersion = invVersion;
    }
    int sel = run.getSelectedIndex();
    if (sel != lastSelectedIndex) {
        rebuildActionButtons();
        lastSelectedIndex = sel;
    }
    unsigned int cardsVersion = run.getCardsVersion();
    if (cardsVersion != lastCardsVersion) {
        rebuildCardButtons();
        lastCardsVersion = cardsVersion;
    }
    int cardSel = run.getSelectedCardIndex();
    if (cardSel != lastSelectedCardIndex) {
        rebuildCardActionButtons();
        lastSelectedCardIndex = cardSel;
    }
}

void PlayUI::rebuildInventoryButtons() {
    inventoryButtons.clear();
    const auto& items = run.getInventoryItems();
    for (size_t i = 0; i < items.size(); ++i) {
        const auto& def = run.getItemRegistry().get(items[i]);
        size_t idx = i;
        inventoryButtons.emplace_back(renderer, def.textureId,
            sf::Vector2f{InventoryX, InventoryTopY + InventorySpacingY * static_cast<float>(i)},
            [this, idx]() { pendingSelectIndex = static_cast<int>(idx); });
    }
}

void PlayUI::rebuildActionButtons() {
    actionButtons.clear();
    int sel = run.getSelectedIndex();
    if (sel < 0 || sel >= static_cast<int>(run.getInventoryItems().size())) return;

    float itemY = InventoryTopY + InventorySpacingY * static_cast<float>(sel);

    actionButtons.emplace_back(renderer, "use_button",
        sf::Vector2f{ActionButtonX, itemY - ActionButtonGapY},
        [this]() { pendingAction = PendingAction::Use; });

    actionButtons.emplace_back(renderer, "discard_button",
        sf::Vector2f{ActionButtonX, itemY + ActionButtonGapY},
        [this]() { pendingAction = PendingAction::Discard; });
}

void PlayUI::rebuildCardButtons() {
    cardButtons.clear();
    cardModelIndex.clear();

    const auto& owned = run.getOwnedCards();
    for (size_t i = 0; i < owned.size(); ++i) {
        // Engine-level cards (empty id, tests/debug only) have nothing to show.
        if (owned[i].defId.empty() || !run.getCardRegistry().has(owned[i].defId)) continue;
        const auto& def = run.getCardRegistry().get(owned[i].defId);

        size_t modelIdx = i;
        size_t row = cardButtons.size();
        cardButtons.emplace_back(renderer, def.textureId,
            sf::Vector2f{CardsX, CardsTopY + CardsSpacingY * static_cast<float>(row)},
            [this, modelIdx]() { pendingCardSelect = static_cast<int>(modelIdx); });
        cardModelIndex.push_back(modelIdx);

        // Clamp to the row height (the card art is taller than the item icons).
        sf::Sprite& sprite = cardButtons.back().getSprite();
        float width = sprite.getGlobalBounds().size.x;
        if (width > CardMaxWidth) {
            sprite.setScale(sprite.getScale() * (CardMaxWidth / width));
        }
    }
}

void PlayUI::rebuildCardActionButtons() {
    cardActionButtons.clear();
    int sel = run.getSelectedCardIndex();
    if (sel < 0 || sel >= static_cast<int>(run.getOwnedCards().size())) return;

    // The selected MODEL index maps back to its visual row to align the button.
    auto it = std::find(cardModelIndex.begin(), cardModelIndex.end(),
                        static_cast<size_t>(sel));
    if (it == cardModelIndex.end()) return;
    float rowY = CardsTopY + CardsSpacingY *
                 static_cast<float>(it - cardModelIndex.begin());

    // Cards are passive: the only action is unmounting (discard).
    cardActionButtons.emplace_back(renderer, "discard_button",
        sf::Vector2f{CardActionX, rowY},
        [this]() { pendingCardDiscard = true; });
}

void PlayUI::renderBackground(RenderSystem& r) {
    r.draw(backUI);
}

void PlayUI::drawColumnPanel(RenderSystem& r, float centerX, const char* title) {
    const ui::Theme& theme = ui::defaultTheme();
    const sf::FloatRect rect({centerX - PanelHalfW, PanelTop},
                             {PanelHalfW * 2.f, PanelHeight});
    r.drawPixelRoundedRect(rect, theme.radius, theme.panelFill,
                           theme.panelBorder, theme.borderThickness);

    const std::string text = title;
    sf::Vector2f size = r.measureText(text, theme.titleSize);
    r.drawText(text, {centerX - size.x / 2.f, PanelTop + theme.padding / 2.f},
               theme.titleSize, theme.accent);
}

void PlayUI::drawBossBanner(RenderSystem& r, const Boss& boss) {
    const ui::Theme& theme = ui::defaultTheme();
    const auto ws = r.getWindowSize();
    const float x = (static_cast<float>(ws.x) - BossBannerW) / 2.f;

    r.drawPixelRoundedRect({{x, BossBannerY}, {BossBannerW, BossBannerH}},
                           theme.radius, theme.panelFill,
                           sf::Color(231, 76, 60), theme.borderThickness);
    r.drawText(boss.getName(), {x + 24.f, BossBannerY + 8.f}, 30, sf::Color::White);

    // HP bar: trough + filled proportion + the visible number (hidden boss
    // health is explicitly rejected — boss-design §1). Clamped both ways so a
    // dying boss never draws a negative bar nor an over-full one.
    const float barX = x + 24.f;
    const float barY = BossBannerY + 56.f;
    const float barW = BossBannerW - 48.f;
    const float barH = 24.f;
    r.drawPixelRoundedRect({{barX, barY}, {barW, barH}}, 6.f, sf::Color(58, 58, 70));
    const float ratio = boss.getMaxHp() > 0
        ? std::clamp(static_cast<float>(boss.getHp()) /
                     static_cast<float>(boss.getMaxHp()), 0.f, 1.f)
        : 0.f;
    if (ratio > 0.f) {
        r.drawPixelRoundedRect({{barX, barY}, {barW * ratio, barH}}, 6.f,
                               sf::Color(231, 76, 60));
    }

    const std::string hp = std::to_string(std::max(0, boss.getHp())) + " / " +
                           std::to_string(boss.getMaxHp());
    const sf::Vector2f size = r.measureText(hp, 20);
    r.drawText(hp, {x + BossBannerW / 2.f - size.x / 2.f, barY + 1.f}, 20,
               sf::Color::White);

    // Phase line under the banner: a scrappy "what to expect" indicator
    // (the Sleeper's asleep/awake countdown), empty for a stateless boss like
    // the Brute. The boss aggregates it from its effects (Boss::statusText);
    // this whole strip is scrapped with the real fight UI.
    const std::string status = boss.statusText();
    if (!status.empty()) {
        const sf::Vector2f ssize = r.measureText(status, 18);
        r.drawText(status,
                   {x + BossBannerW / 2.f - ssize.x / 2.f, BossBannerY + BossBannerH + 6.f},
                   18, sf::Color(210, 210, 220));
    }
}

void PlayUI::renderForeground(RenderSystem& r) {
    drawDigitCounter(r, static_cast<unsigned int>(run.getTurnCount()), TurnCounterX);
    drawDigitCounter(r, static_cast<unsigned int>(run.getCoins()), CoinsCounterX);

    // Boss-fight banner while a body is on the board. Until the ante machine
    // brings AntePhase::BossFight, boss-on-board IS the fight condition.
    if (const Boss* boss = run.getCurrentBoss()) {
        drawBossBanner(r, *boss);
    }

    // Shop spawn countdown: stacked just below the turn counter (top-left),
    // smaller so it reads as a sub-counter and stays clear of the board and the
    // score readouts. Shows 0 while a shop is on the board; restarts at the
    // interval once the shop is consumed.
    drawDigitCounter(r, static_cast<unsigned int>(run.getShopCountdown()),
                     ShopCountdownX, ShopCountdownY, ShopCountdownScale);

    // The boss clock: free-play turns left before the ante's fight. Frozen
    // (and reading 0) during the fight and the reward turn — the banner is
    // the fight indicator then. Proper fight UI is slice 5; this keeps the
    // doom visible meanwhile.
    if (run.getAntePhase() == GameRun::AntePhase::FreePlay) {
        drawDigitCounter(r, static_cast<unsigned int>(run.getAnteCountdown()),
                         ShopCountdownX, AnteCountdownY, ShopCountdownScale);
    }

    // Column frames first, so every widget draws on top of them.
    drawColumnPanel(r, InventoryX, "Items");
    drawColumnPanel(r, CardsX, "Cards");

    int sel = run.getSelectedIndex();
    for (size_t i = 0; i < inventoryButtons.size(); ++i) {
        auto& btn = inventoryButtons[i];
        bool held = (static_cast<int>(i) == sel);

        if (held) btn.getSprite().setColor(sf::Color::Red);
        r.draw(btn.getSprite());
        if (held) btn.getSprite().setColor(sf::Color::White);
    }

    for (auto& btn : actionButtons) {
        r.draw(btn.getSprite());
    }

    int cardSel = run.getSelectedCardIndex();
    for (size_t i = 0; i < cardButtons.size(); ++i) {
        auto& btn = cardButtons[i];
        bool held = (cardSel >= 0 && cardModelIndex[i] == static_cast<size_t>(cardSel));

        if (held) btn.getSprite().setColor(sf::Color::Red);
        r.draw(btn.getSprite());
        if (held) btn.getSprite().setColor(sf::Color::White);
    }

    for (auto& btn : cardActionButtons) {
        r.draw(btn.getSprite());
    }

    renderInventoryTooltip(r);
    renderCardsTooltip(r);

    // Debug-mode toggle, debug builds only: green = admin features on,
    // gray = the real game (prices, stock, no debug keys). Click to flip.
    if (debug::Enabled) {
        const bool on = debug::active();
        r.drawPixelRoundedRect(debugToggleRect, 8.f,
                               on ? sf::Color(36, 90, 48) : sf::Color(58, 58, 64),
                               on ? sf::Color(80, 200, 110) : sf::Color(120, 120, 128),
                               3.f);
        const std::string label = on ? "DEBUG: ON" : "DEBUG: OFF";
        const sf::Vector2f size = r.measureText(label, 24);
        r.drawText(label,
                   {debugToggleRect.position.x +
                        (debugToggleRect.size.x - size.x) / 2.f,
                    debugToggleRect.position.y +
                        (debugToggleRect.size.y - size.y) / 2.f},
                   24, sf::Color::White);
    }
}

void PlayUI::renderInventoryTooltip(RenderSystem& r) {
    const auto& items = run.getInventoryItems();
    const sf::Vector2f mouse = r.mapPixelToUI(sf::Mouse::getPosition(r.getWindow()));

    for (std::size_t i = 0; i < inventoryButtons.size() && i < items.size(); ++i) {
        if (!inventoryButtons[i].contains(mouse)) continue;

        // Built each frame on hover (small tree, cheap). cost = -1 → no price
        // badge, since you already own inventory items.
        ui::UINode tip = buildItemTooltip(run.getItemRegistry().get(items[i]), -1);
        ui::measureNode(tip, r);

        // Inventory hugs the right edge, so place the tooltip to the LEFT of the
        // hovered item, vertically centred, clamped to the screen.
        const sf::FloatRect b = inventoryButtons[i].getSprite().getGlobalBounds();
        const auto ws = r.getWindowSize();
        float tx = std::max(10.f, b.position.x - tip.computedW - 24.f);
        float ty = std::clamp(b.position.y + b.size.y / 2.f - tip.computedH / 2.f,
                              10.f, static_cast<float>(ws.y) - tip.computedH - 10.f);

        ui::layoutNode(tip, tx, ty);
        ui::drawNode(tip, r);
        break;  // one tooltip at a time
    }
}

void PlayUI::renderCardsTooltip(RenderSystem& r) {
    const sf::Vector2f mouse = r.mapPixelToUI(sf::Mouse::getPosition(r.getWindow()));

    const auto& owned = run.getOwnedCards();
    for (std::size_t i = 0; i < cardButtons.size(); ++i) {
        if (!cardButtons[i].contains(mouse)) continue;

        const auto& def = run.getCardRegistry().get(owned[cardModelIndex[i]].defId);
        ui::UINode tip = buildCardTooltip(def, -1);  // owned: no price badge
        ui::measureNode(tip, r);

        // Cards hug the left edge, so the tooltip goes to their RIGHT (the
        // mirror of the inventory tooltip), vertically centred and clamped.
        const sf::FloatRect b = cardButtons[i].getSprite().getGlobalBounds();
        const auto ws = r.getWindowSize();
        float tx = std::min(b.position.x + b.size.x + 24.f,
                            static_cast<float>(ws.x) - tip.computedW - 10.f);
        float ty = std::clamp(b.position.y + b.size.y / 2.f - tip.computedH / 2.f,
                              10.f, static_cast<float>(ws.y) - tip.computedH - 10.f);

        ui::layoutNode(tip, tx, ty);
        ui::drawNode(tip, r);
        break;  // one tooltip at a time
    }
}

bool PlayUI::isPointOverUI(sf::Vector2f screenPoint) const {
    // The debug toggle owns its rect (debug builds): no pan may start there
    // and no right-click selection may fall through it.
    if (debug::Enabled && debugToggleRect.contains(screenPoint)) return true;

    for (const auto& btn : inventoryButtons) {
        if (btn.contains(screenPoint)) return true;
    }
    for (const auto& btn : actionButtons) {
        if (btn.contains(screenPoint)) return true;
    }
    for (const auto& btn : cardButtons) {
        if (btn.contains(screenPoint)) return true;
    }
    for (const auto& btn : cardActionButtons) {
        if (btn.contains(screenPoint)) return true;
    }
    return false;
}

void PlayUI::drawDigitCounter(RenderSystem& r, unsigned int value, float xOffset,
                              float y, float scale) {
    std::string text = std::to_string(value);
    float totalWidth = static_cast<float>(text.size()) * 5.f * scale;
    r.drawNumber(value, {xOffset + totalWidth / 2.f, y}, scale);
}
