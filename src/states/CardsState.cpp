#include "states/CardsState.h"
#include "states/StateManager.h"
#include "states/ItemTooltip.h"
#include "core/GameRun.h"
#include "core/CardRegistry.h"
#include "rendering/RenderSystem.h"
#include "ui/UI.h"

#include <algorithm>

namespace {
// Panel geometry, fractions of the window so it centres at any resolution.
constexpr float PanelWidthFrac  = 0.6f;
constexpr float PanelHeightFrac = 0.5f;
constexpr float CardSpacing     = 250.f;
} // namespace

CardsState::CardsState(StateManager& stateManager, RenderSystem& renderer, GameRun* gameRun,
                       RenderBehind renderBehind)
    : stateManager(stateManager),
      renderer(renderer),
      gameRun(gameRun),
      renderBehind(std::move(renderBehind))
{
    enter();
}

void CardsState::enter() {
    rebuildCardButtons();
}

void CardsState::exit() {
    stateManager.requestPop();
}

void CardsState::rebuildCardButtons() {
    cardButtons.clear();

    // Owned cards with a registry def, in acquisition order (= dispatch order,
    // so the panel reads in the same order the cards fire). Engine-level cards
    // without a def (empty id) have nothing to show and are skipped.
    std::vector<const CardDef*> defs;
    for (const auto& owned : gameRun->getOwnedCards()) {
        if (!owned.defId.empty() && gameRun->getCardRegistry().has(owned.defId)) {
            defs.push_back(&gameRun->getCardRegistry().get(owned.defId));
        }
    }
    if (defs.empty()) return;

    auto ws = renderer.getWindowSize();
    float centerX = static_cast<float>(ws.x) / 2.f;
    float centerY = static_cast<float>(ws.y) / 2.f;

    const size_t count = defs.size();
    const float band = static_cast<float>(ws.x) * PanelWidthFrac * 0.9f;
    float spacing = std::min(CardSpacing, band / static_cast<float>(count));
    float startX = centerX - (static_cast<float>(count) - 1.f) / 2.f * spacing;
    const float maxIconWidth = spacing * 0.8f;

    for (size_t i = 0; i < count; ++i) {
        cardButtons.emplace_back(renderer, defs[i]->textureId,
            sf::Vector2f{startX + static_cast<float>(i) * spacing, centerY},
            []() {});  // owned cards are display-only for now

        sf::Sprite& sprite = cardButtons.back().getSprite();
        float width = sprite.getGlobalBounds().size.x;
        if (width > maxIconWidth) {
            sprite.setScale(sprite.getScale() * (maxIconWidth / width));
        }
    }
}

void CardsState::handleInput(sf::Event& event) {
    auto* key = event.getIf<sf::Event::KeyReleased>();
    if (key && key->scancode == sf::Keyboard::Scancode::Escape) {
        exit();
    }
}

void CardsState::update(float deltaTime) {
    // Modal: the world below is frozen (drawn, never updated) — same contract
    // as the shop overlay.
    for (auto& btn : cardButtons) {
        btn.update(deltaTime);
    }
}

void CardsState::render(RenderSystem& renderer) {
    if (renderBehind) renderBehind(renderer);

    renderer.useUIView();

    // Panel backdrop, themed like the tooltips so the UI reads as one family.
    const ui::Theme& theme = ui::defaultTheme();
    auto ws = renderer.getWindowSize();
    const float pw = static_cast<float>(ws.x) * PanelWidthFrac;
    const float ph = static_cast<float>(ws.y) * PanelHeightFrac;
    const sf::FloatRect panel({(static_cast<float>(ws.x) - pw) / 2.f,
                               (static_cast<float>(ws.y) - ph) / 2.f},
                              {pw, ph});
    renderer.drawPixelRoundedRect(panel, theme.radius, theme.panelFill,
                                  theme.panelBorder, theme.borderThickness);

    // Title, centred at the panel top.
    const std::string title = "Cards";
    sf::Vector2f titleSize = renderer.measureText(title, theme.titleSize);
    renderer.drawText(title,
                      {panel.position.x + (panel.size.x - titleSize.x) / 2.f,
                       panel.position.y + theme.padding},
                      theme.titleSize, theme.accent);

    if (cardButtons.empty()) {
        const std::string empty = "No cards yet - find them in the shop.";
        sf::Vector2f emptySize = renderer.measureText(empty, theme.bodySize);
        renderer.drawText(empty,
                          {panel.position.x + (panel.size.x - emptySize.x) / 2.f,
                           panel.position.y + panel.size.y / 2.f - emptySize.y / 2.f},
                          theme.bodySize, theme.textMuted);
    }

    for (auto& btn : cardButtons) {
        renderer.draw(btn.getSprite());
    }

    renderHoveredTooltip(renderer);
}

void CardsState::renderHoveredTooltip(RenderSystem& renderer) {
    const sf::Vector2i mp = sf::Mouse::getPosition(renderer.getWindow());
    const sf::Vector2f mouse(mp);

    // Buttons mirror the owned-with-def list rebuilt in rebuildCardButtons;
    // re-derive it the same way so index i maps to the same def.
    std::vector<const CardDef*> defs;
    for (const auto& owned : gameRun->getOwnedCards()) {
        if (!owned.defId.empty() && gameRun->getCardRegistry().has(owned.defId)) {
            defs.push_back(&gameRun->getCardRegistry().get(owned.defId));
        }
    }

    for (std::size_t i = 0; i < cardButtons.size() && i < defs.size(); ++i) {
        if (!cardButtons[i].contains(mouse)) continue;

        ui::UINode tip = buildCardTooltip(*defs[i], -1);  // owned: no price badge
        ui::measureNode(tip, renderer);

        const sf::FloatRect b = cardButtons[i].getSprite().getGlobalBounds();
        const auto ws = renderer.getWindowSize();
        float tx = std::clamp(b.position.x + b.size.x / 2.f - tip.computedW / 2.f,
                              10.f, static_cast<float>(ws.x) - tip.computedW - 10.f);
        float ty = b.position.y - tip.computedH - 16.f;
        if (ty < 10.f) ty = b.position.y + b.size.y + 16.f;

        ui::layoutNode(tip, tx, ty);
        ui::drawNode(tip, renderer);
        break;
    }
}
