#include "states/RewardState.h"
#include "states/StateManager.h"
#include "states/ItemTooltip.h"
#include "core/GameRun.h"
#include "rendering/RenderSystem.h"
#include "ui/UI.h"

#include <algorithm>

RewardState::RewardState(StateManager& stateManager, RenderSystem& renderer, GameRun* gameRun,
                         RenderBehind renderBehind)
    : stateManager(stateManager),
      renderer(renderer),
      gameRun(gameRun),
      renderBehind(std::move(renderBehind))
{
    enter();
}

void RewardState::enter() {
    buildButtons();
}

void RewardState::exit() {
    // The skip / teardown path: clear the owed-reward flags WITHOUT a grant,
    // then close. (The pick path closes itself in update — see pickReward.)
    gameRun->dismissReward();
    stateManager.requestPop();
}

void RewardState::buildButtons() {
    optionButtons.clear();
    const auto& offer = gameRun->getRewardOffer();
    if (offer.empty()) return;

    const auto ws = renderer.getWindowSize();
    const float centerX = static_cast<float>(ws.x) / 2.f;
    const float centerY = static_cast<float>(ws.y) / 2.f;

    // A simple centred row, like the shop's item row.
    const size_t count = offer.size();
    const float spacing = 320.f;
    const float startX = centerX - (static_cast<float>(count) - 1.f) / 2.f * spacing;

    for (size_t i = 0; i < count; ++i) {
        const float x = startX + static_cast<float>(i) * spacing;
        const size_t idx = i;
        optionButtons.emplace_back(renderer, offer[i].textureId,
            sf::Vector2f{x, centerY},
            [this, idx]() { pendingPickIndex = static_cast<int>(idx); });
    }
}

void RewardState::handleInput(sf::Event& event) {
    auto* key = event.getIf<sf::Event::KeyReleased>();
    if (key && key->scancode == sf::Keyboard::Scancode::Escape) {
        exit();  // skip the reward
    }
}

void RewardState::update(float deltaTime) {
    // The world below is frozen (modal) — only this picker updates.
    for (auto& btn : optionButtons) {
        btn.update(deltaTime);
    }

    if (pendingPickIndex >= 0) {
        gameRun->pickReward(static_cast<size_t>(pendingPickIndex));
        pendingPickIndex = -1;
        stateManager.requestPop();  // reward taken — close
    }
}

void RewardState::render(RenderSystem& renderer) {
    if (renderBehind) renderBehind(renderer);

    renderer.useUIView();

    const auto ws = renderer.getWindowSize();
    const float w = static_cast<float>(ws.x);
    const float h = static_cast<float>(ws.y);

    // A centred panel behind the options, plus a title and a skip hint.
    const float panelW = 1100.f, panelH = 520.f;
    const sf::FloatRect panel({w / 2.f - panelW / 2.f, h / 2.f - panelH / 2.f},
                              {panelW, panelH});
    renderer.drawPixelRoundedRect(panel, 18.f, sf::Color(24, 24, 34, 235),
                                  sf::Color(210, 180, 90), 4.f);

    const std::string title = "CHOOSE A REWARD";
    const sf::Vector2f tsize = renderer.measureText(title, 48);
    renderer.drawText(title, {w / 2.f - tsize.x / 2.f, panel.position.y + 36.f},
                      48, sf::Color(235, 225, 200));

    for (auto& btn : optionButtons) {
        renderer.draw(btn.getSprite());
    }

    const std::string hint = "ESC to skip";
    const sf::Vector2f hsize = renderer.measureText(hint, 22);
    renderer.drawText(hint, {w / 2.f - hsize.x / 2.f, panel.position.y + panelH - 48.f},
                      22, sf::Color(150, 150, 160));

    renderHoveredTooltip(renderer);
}

void RewardState::renderHoveredTooltip(RenderSystem& renderer) {
    const sf::Vector2f mouse =
        renderer.mapPixelToUI(sf::Mouse::getPosition(renderer.getWindow()));
    const auto& offer = gameRun->getRewardOffer();

    for (std::size_t i = 0; i < optionButtons.size() && i < offer.size(); ++i) {
        if (!optionButtons[i].contains(mouse)) continue;

        // A reward is free, so omit the price badge (cost < 0). Reuses the
        // generic info-card builder shared by item/card tooltips.
        ui::UINode tip = buildInfoTooltip(offer[i].textureId, offer[i].name,
                                          offer[i].description, -1);
        ui::measureNode(tip, renderer);

        const sf::FloatRect b = optionButtons[i].getSprite().getGlobalBounds();
        const auto ws = renderer.getWindowSize();
        float tx = std::clamp(b.position.x + b.size.x / 2.f - tip.computedW / 2.f,
                              10.f, static_cast<float>(ws.x) - tip.computedW - 10.f);
        float ty = b.position.y - tip.computedH - 16.f;
        if (ty < 10.f) ty = b.position.y + b.size.y + 16.f;

        ui::layoutNode(tip, tx, ty);
        ui::drawNode(tip, renderer);
        return;
    }
}
