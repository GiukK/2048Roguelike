#include "states/MenuState.h"
#include "states/PlayState.h"
#include "states/StateManager.h"
#include "rendering/RenderSystem.h"

MenuState::MenuState(StateManager& stateManager, RenderSystem& renderer)
    : stateManager(stateManager),
      renderer(renderer),
      background(renderer.getTextureManager().get("background"))
{
    auto ws = renderer.getWindowSize();
    const float vShift = 100.f;

    buttons.emplace_back(renderer, "startb",
        sf::Vector2f{static_cast<float>(ws.x) / 2.f, -vShift + static_cast<float>(ws.y) / 2.f},
        [&stateManager, &renderer]() {
            stateManager.pushState(std::make_unique<PlayState>(stateManager, renderer));
        });

    buttons.emplace_back(renderer, "optionsb",
        sf::Vector2f{static_cast<float>(ws.x) / 2.f, vShift + static_cast<float>(ws.y) / 2.f},
        []() {});

    initVisuals();
    enter();
}

void MenuState::initVisuals() {
    renderer.resizeSprite("background", background);
}

void MenuState::enter() {}

void MenuState::exit() {
    renderer.close();
}

void MenuState::handleInput(sf::Event& event) {
    auto* key = event.getIf<sf::Event::KeyReleased>();
    if (!key) return;

    if (key->scancode == sf::Keyboard::Scancode::Escape) {
        exit();
    }
}

void MenuState::update(float deltaTime) {
    for (auto& btn : buttons) {
        btn.update(deltaTime);
    }
}

void MenuState::render(RenderSystem& renderer) {
    renderer.draw(background);
    for (auto& btn : buttons) {
        renderer.draw(btn.getSprite());
    }
}
