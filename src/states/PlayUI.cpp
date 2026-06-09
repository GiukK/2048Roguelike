#include "states/PlayUI.h"
#include "states/ItemTooltip.h"
#include "core/GameRun.h"
#include "core/ItemRegistry.h"
#include "rendering/RenderSystem.h"
#include "ui/UI.h"

#include <algorithm>
#include <string>

namespace {
// HUD layout, screen-space (drawn in the UI view). Hardcoded for now; moves into
// the data-driven UI layer later. The inventory row geometry is shared so the
// use/discard action buttons line up with the selected item.
constexpr float TurnCounterX       = 350.f;   // top-left turn count
constexpr float CoinsCounterX      = 1380.f;  // top-right coin count
constexpr float ShopCountdownX     = 350.f;   // below the turn count
constexpr float ShopCountdownY     = 100.f;
constexpr float ShopCountdownScale = 7.f;
constexpr float InventoryX         = 1500.f;  // right-side inventory column
constexpr float InventoryTopY      = 350.f;   // first item's Y
constexpr float InventorySpacingY  = 200.f;   // vertical gap between items
constexpr float ActionButtonX      = 1350.f;  // use/discard column (left of items)
constexpr float ActionButtonGapY   = 55.f;    // offset of use/discard from item Y
} // namespace

PlayUI::PlayUI(RenderSystem& renderer, GameRun& run)
    : renderer(renderer),
      run(run),
      backUI(renderer.getTextureManager().get("backUI"))
{
    renderer.resizeSprite("backUI", backUI);
}

void PlayUI::update(float dt) {
    for (auto& btn : inventoryButtons) btn.update(dt);
    for (auto& btn : actionButtons) btn.update(dt);

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

    // Rebuild AFTER processing, so a model change this frame is reflected this
    // frame. Also catches inventory changes that happened while we weren't
    // updating (e.g. a shop purchase, since update is driven by PlayState).
    syncButtonsToModel();
}

void PlayUI::syncButtonsToModel() {
    int invSize = static_cast<int>(run.getInventoryItems().size());
    if (invSize != lastInventorySize) {
        rebuildInventoryButtons();
        lastInventorySize = invSize;
    }
    int sel = run.getSelectedIndex();
    if (sel != lastSelectedIndex) {
        rebuildActionButtons();
        lastSelectedIndex = sel;
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

void PlayUI::renderBackground(RenderSystem& r) {
    r.draw(backUI);
}

void PlayUI::renderForeground(RenderSystem& r) {
    drawDigitCounter(r, static_cast<unsigned int>(run.getTurnCount()), TurnCounterX);
    drawDigitCounter(r, static_cast<unsigned int>(run.getCoins()), CoinsCounterX);

    // Shop spawn countdown: stacked just below the turn counter (top-left),
    // smaller so it reads as a sub-counter and stays clear of the board and the
    // score readouts. Shows 0 while a shop is on the board; restarts at the
    // interval once the shop is consumed.
    drawDigitCounter(r, static_cast<unsigned int>(run.getShopCountdown()),
                     ShopCountdownX, ShopCountdownY, ShopCountdownScale);

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

    renderInventoryTooltip(r);
}

void PlayUI::renderInventoryTooltip(RenderSystem& r) {
    const auto& items = run.getInventoryItems();
    const sf::Vector2i mp = sf::Mouse::getPosition(r.getWindow());
    const sf::Vector2f mouse(mp);

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

bool PlayUI::isPointOverUI(sf::Vector2f screenPoint) const {
    for (const auto& btn : inventoryButtons) {
        if (btn.contains(screenPoint)) return true;
    }
    for (const auto& btn : actionButtons) {
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
