#include "states/PlayState.h"
#include <iostream>
#include <algorithm>

PlayState::PlayState(StateManager& stateManager, RenderSystem& renderer) :
    stateManager(stateManager),
    renderer(renderer),
    currentRun(std::make_unique<GameRun>(renderer, this))
{

    UI_Button exitButton(renderer, "exit_button", {1800.f, 100.f}, [this]() {

        this->stateManager.requestPop();

        std::cout << "pop requested from exit_button" << std::endl;

        });


    buttons.emplace_back(std::move(exitButton));

    enter(); // Open game
}

void PlayState::enter() {
    std::cout << "Entering Play State - PlayState::enter()" << std::endl;

    currentRun->enter();
}

void PlayState::exit() {
    std::cout << "exit() was called from PlayState - PlayState::exit()" << std::endl;

    stateManager.popState();
}

void PlayState::handleInput(sf::Event& event) {
    if (const auto* keyPressed = event.getIf<sf::Event::KeyReleased>()) {
        if (keyPressed->scancode == sf::Keyboard::Scancode::Enter) {
            std::cout << "Starting Game..." << std::endl;
            // TODO: Transition to PlayState
        }
        else if (keyPressed->scancode == sf::Keyboard::Scancode::Escape) {
            std::cout << "Exiting Game..." << std::endl;


            exit();  // Close game
            return;
        }
    }

    currentRun->handleInput(event);
}

void PlayState::update(float deltaTime) {

    currentRun->update(deltaTime);

    //draw buttons
    for (auto& button : buttons) {

        button.update(deltaTime);

    }

    for (auto& ani : animations) {

        ani->update(deltaTime);
    }

    animations.erase(
        std::remove_if(animations.begin(), animations.end(),
            [](const std::unique_ptr<Animation>& ani) {
                return ani->isEnded();
            }),
        animations.end());

}

void PlayState::render(RenderSystem& renderer) {


    //Text UI deprecated
    currentRun->render(renderer);

    //draw buttons
    for (auto& button : buttons) {

        renderer.draw(button.getSprite());

    }

    //render animation
    for (auto& ani : animations) {

        renderer.draw(ani->getSprite());
    }
}

void PlayState::addAnimation(std::unique_ptr<Animation> ani) {
    if (!ani) return;
    animations.push_back(std::move(ani));  
}
