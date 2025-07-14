#include "states/ShopState.h"
#include <iostream>

ShopState::ShopState(StateManager& stateManager, sf::RenderWindow& window, GameRun* current_run) :
    stateManager(stateManager),
    window(window),
    currentRun(current_run),
    shopTexture("assets/textures/shop.png", false, sf::IntRect({ 0, 0 }, { 192, 108 })),
    shopSprite(shopTexture)
{
        //set sprite origin and scale
    shopSprite.setOrigin({ 0.f, 0.f });
    shopSprite.setScale({ 25.f, 25.f });
    shopSprite.setPosition({ 160.f, 0.f });

    enter(); // Open shop
}

void ShopState::enter() {
    std::cout << "Entering Shop ------------------------" << std::endl;
}

void ShopState::exit() {
    std::cout << "Exiting Shop ------------------------" << std::endl;

    stateManager.popState();
}

void ShopState::handleInput(sf::Event& event) {
    if (const auto* keyPressed = event.getIf<sf::Event::KeyReleased>()) {
        if (keyPressed->scancode == sf::Keyboard::Scancode::Escape) {
            std::cout << "Exiting shop..." << std::endl;

            // right now this field is public since the closing of the shop is currently handled by ShopState
            // in the future it will probably make sense to fully handle the UI internally in GameRun by creating public functions that modify private fields.
            currentRun->shopOpen = false;

            exit();  // Close shop
            return;
        }
    }
}

void ShopState::update(float deltaTime) {

}

void ShopState::render(sf::RenderWindow& window) {

    

    currentRun->render(window);

    window.draw(shopSprite);
}

