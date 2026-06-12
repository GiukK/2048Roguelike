#include "states/GameOverState.h"
#include "states/StateManager.h"
#include "core/GameRun.h"
#include "rendering/RenderSystem.h"

#include <string>

namespace {
// Layout in virtual-screen space (1920x1080), the same convention as every
// other screen's pixel constants.
constexpr unsigned int TitleCharSize = 140;
constexpr unsigned int LineCharSize = 40;
constexpr float TitleY = 300.f;
constexpr float FirstLineY = 500.f;
constexpr float LineSpacing = 64.f;
constexpr float ExitButtonY = 760.f;
} // namespace

GameOverState::GameOverState(StateManager& stateManager, RenderSystem& renderer,
                             GameRun* gameRun, RenderBehind renderBehind)
    : stateManager(stateManager),
      renderer(renderer),
      gameRun(gameRun),
      renderBehind(std::move(renderBehind)),
      // getTurnCount() includes the just-pushed dead turn (the one with no
      // legal move); the player completed one fewer.
      turnsSurvived(gameRun->getTurnCount() > 0 ? gameRun->getTurnCount() - 1 : 0),
      bestTile(gameRun->currentBoard().getMaxTileValue())
{
    auto ws = renderer.getWindowSize();
    buttons.emplace_back(renderer, "exit_button",
        sf::Vector2f{static_cast<float>(ws.x) / 2.f, ExitButtonY},
        [this]() { exit(); });

    enter();
}

void GameOverState::enter() {}

void GameOverState::exit() {
    // Two pops: this overlay and the dead PlayState (with its GameRun) beneath
    // it — back to the menu. requestPop defers to the StateManager's safe
    // point, so nothing is destroyed while a state method is still executing.
    stateManager.requestPop();
    stateManager.requestPop();
}

void GameOverState::handleInput(sf::Event& event) {
    auto* key = event.getIf<sf::Event::KeyReleased>();
    if (!key) return;

    if (key->scancode == sf::Keyboard::Scancode::Escape ||
        key->scancode == sf::Keyboard::Scancode::Enter) {
        exit();
    }
}

void GameOverState::update(float deltaTime) {
    // The world below is frozen (modal, like the shop): only this overlay's
    // widgets are updated.
    for (auto& btn : buttons) {
        btn.update(deltaTime);
    }
}

void GameOverState::render(RenderSystem& renderer) {
    // The final locked board, frozen behind a dark veil.
    if (renderBehind) renderBehind(renderer);

    renderer.useUIView();
    auto ws = renderer.getWindowSize();

    sf::RectangleShape veil({static_cast<float>(ws.x), static_cast<float>(ws.y)});
    veil.setFillColor(sf::Color(0, 0, 0, 200));
    renderer.draw(veil);

    auto centered = [&](const std::string& text, unsigned int charSize, float y,
                        sf::Color color) {
        sf::Vector2f m = renderer.measureText(text, charSize);
        renderer.drawText(text, {(static_cast<float>(ws.x) - m.x) / 2.f, y},
                          charSize, color);
    };

    centered("GAME OVER", TitleCharSize, TitleY, sf::Color(231, 76, 60));
    centered("The board is locked: no move is left.", LineCharSize, FirstLineY,
             sf::Color(220, 220, 220));
    centered("Turns survived: " + std::to_string(turnsSurvived), LineCharSize,
             FirstLineY + LineSpacing, sf::Color(220, 220, 220));
    centered("Best tile: " + std::to_string(bestTile), LineCharSize,
             FirstLineY + 2.f * LineSpacing, sf::Color(220, 220, 220));

    for (auto& btn : buttons) {
        renderer.draw(btn.getSprite());
    }
}
