#include "states/MenuState.h"
#include "states/PlayState.h"
#include "rendering/RenderSystem.h"

#include <iostream>
#include <string>

MenuState::MenuState(StateManager& stateManager, RenderSystem& renderer) :
    stateManager(stateManager),
    renderer(renderer),
    background(renderer.getTextureManager().get("background")),
    startButton(renderer.getTextureManager().get("startb")),
    optionsButton(renderer.getTextureManager().get("optionsb"))
{
    //fixes size,position and origin of sprites.
    fixVisualAssets();

    // Open game menu at start
    enter(); 
}

void MenuState::fixVisualAssets() {

    //asset resizing
    renderer.resizeSprite("background", background);
    renderer.resizeSprite("startb", startButton);
    renderer.resizeSprite("optionsb", optionsButton);

    //asset origin setting

    startButton.setOrigin(startButton.getLocalBounds().getCenter());
    optionsButton.setOrigin(optionsButton.getLocalBounds().getCenter());


    //asset positioning
    auto windowSize = renderer.getWindowSize();

    const float verticalShift = 100.f; //this is a visual shift needed to set up the correct position for the two buttons : START, OPTIONS

    startButton.setPosition({ float(windowSize.x) / 2, -verticalShift + float(windowSize.y) / 2 }); //  up shift (-) 
    optionsButton.setPosition({ float(windowSize.x) / 2, verticalShift + float(windowSize.y) / 2 }); // down shift

    std::cout << "MenuState visual assets: ready" << std::endl;
}

void MenuState::enter() {
    std::cout << "Entering Menu State" << std::endl;
}

void MenuState::exit() {
    std::cout << "exit() was called from MenuState" << std::endl;
    renderer.close();
}

void MenuState::handleInput(sf::Event& event) {
    if (const auto* keyPressed = event.getIf<sf::Event::KeyReleased>()) {
        if (keyPressed->scancode == sf::Keyboard::Scancode::Enter) {
            std::cout << "Starting Game..." << std::endl;
            //enter??
        }
        else if (keyPressed->scancode == sf::Keyboard::Scancode::Escape) {
            std::cout << "Exiting Game..." << std::endl;


            exit();  // Close game
        }
    }
    if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {

        auto& window = renderer.getWindow();

        //HANDLE MOUSE POSITIONS
        sf::Vector2i mousePos_i = sf::Mouse::getPosition(window);
        sf::Vector2f mousePos_f = window.mapPixelToCoords(mousePos_i);  // Convert to world coordinates (float)

        //----------------------------------------------------------------------

        if (startButton.getGlobalBounds().contains(mousePos_f)) {

            std::cout << "START pressed" << std::endl;
            stateManager.pushState(std::make_unique<PlayState>(stateManager, renderer));
        }

        if (optionsButton.getGlobalBounds().contains(mousePos_f)) {
            std::cout << "OPTIONS pressed" << std::endl;
        }
    }
}

void MenuState::update(float deltaTime) {

    updateButton(startButton);
    updateButton(optionsButton);
}

void MenuState::render(RenderSystem& renderer) {

    //Text UI deprecated
    //window.draw(titleText);

    renderer.draw(background);

    renderer.draw(startButton);

    renderer.draw(optionsButton);
}

void MenuState::updateButton(sf::Sprite& button) {


    sf::Vector2i mousePos = sf::Mouse::getPosition(renderer.getWindow());

    if (button.getGlobalBounds().contains(static_cast<sf::Vector2f>(mousePos)) and button.getColor() == sf::Color::White) {

        button.setColor(sf::Color::Red);
        //audioHover.play();

    }
    else if (!button.getGlobalBounds().contains(static_cast<sf::Vector2f>(mousePos))) { button.setColor(sf::Color::White); }

}
