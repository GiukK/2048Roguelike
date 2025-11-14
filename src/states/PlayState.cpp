#include "states/PlayState.h"
#include <iostream>

PlayState::PlayState(StateManager& stateManager, RenderSystem& renderer) :
    stateManager(stateManager),
    renderer(renderer),
    currentRun(std::make_unique<GameRun>(renderer, this))
{

    UI_Button exitButton(renderer, "exit_button", [this]() {

        this->stateManager.requestPop();

        std::cout << "pop requested from exit_button" << std::endl;

        });


    //---- maybe it would be useful to build a function 
    sf::Sprite& exitButtonSprite = exitButton.getSprite();
    exitButtonSprite.setPosition({ 1800.f, 100.f });
    exitButtonSprite.setOrigin(exitButtonSprite.getLocalBounds().getCenter());
    //----

    renderer.resizeSprite("exit_button", exitButton.getSprite());

    buttons.emplace_back(std::move(exitButton));
    //


    enter(); // Open game
}

void PlayState::enter() {
    std::cout << "Entering Play State - PlayState::enter()" << std::endl;

    currentRun->enter();

    std::cout << "Entering current GameRun" << std::endl;
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

    //draw buttons
    for (auto& button : buttons) {

        button.update(deltaTime);

    }

    currentRun->update(deltaTime);

}

void PlayState::render(RenderSystem& renderer) {


    //Text UI deprecated
    currentRun->render(renderer);

    //draw buttons
    for (auto& button : buttons) {

        renderer.draw(button.getSprite());

    }
}
