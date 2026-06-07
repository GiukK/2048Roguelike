#include "states/PlayState.h"
#include "states/StateManager.h"
#include "states/ShopState.h"
#include "rendering/RenderSystem.h"

#include <algorithm>

PlayState::PlayState(StateManager& stateManager, RenderSystem& renderer)
    : stateManager(stateManager),
      renderer(renderer)
{
    auto animCallback = [this](std::unique_ptr<Animation> anim) {
        addAnimation(std::move(anim));
    };

    auto shopCallback = [this](GameRun* run) {
        this->stateManager.pushState(
            std::make_unique<ShopState>(this->stateManager, this->renderer, run));
    };

    currentRun = std::make_unique<GameRun>(renderer, animCallback, shopCallback);
    currentRun->setAnimationsActiveQuery([this]() { return hasActiveAnimations(); });

    buttons.emplace_back(renderer, "exit_button", sf::Vector2f{1800.f, 100.f},
        [this]() { this->stateManager.requestPop(); });

    enter();
}

void PlayState::enter() {
    currentRun->enter();
}

void PlayState::exit() {
    stateManager.requestPop();
}

void PlayState::handleInput(sf::Event& event) {
    auto* key = event.getIf<sf::Event::KeyReleased>();
    if (key && key->scancode == sf::Keyboard::Scancode::Escape) {
        exit();
        return;
    }
    currentRun->handleInput(event);
}

void PlayState::update(float deltaTime) {
    currentRun->update(deltaTime);

    for (auto& btn : buttons) {
        btn.update(deltaTime);
    }

    for (auto& anim : animations) {
        anim->update(deltaTime);
    }

    animations.erase(
        std::remove_if(animations.begin(), animations.end(),
            [](const std::unique_ptr<Animation>& a) { return a->isFinished(); }),
        animations.end());
}

void PlayState::render(RenderSystem& renderer) {
    currentRun->render(renderer);

    for (auto& btn : buttons) {
        renderer.draw(btn.getSprite());
    }

    for (auto& anim : animations) {
        renderer.draw(anim->getSprite());
    }
}

void PlayState::addAnimation(std::unique_ptr<Animation> anim) {
    if (anim) {
        animations.push_back(std::move(anim));
    }
}
