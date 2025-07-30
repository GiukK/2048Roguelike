#include "states/MenuState.h"
#include "states/PlayState.h"
#include "rendering/RenderSystem.h"

#include <iostream>
#include <string>

MenuState::MenuState(StateManager& stateManager, RenderSystem& renderer) :
    stateManager(stateManager),
    renderer(renderer),
    background(renderer.getTextureManager().get("background")) //not movable
{
    //fixes button and assset size,position and origin of sprites.
    buttons.emplace_back(renderer, "startb", [&stateManager, &renderer]() {
        stateManager.pushState(std::make_unique<PlayState>(stateManager, renderer));
        });
    buttons.emplace_back(renderer, "optionsb", []() {
        std::cout << "Options clicked!\n";
        });

    fixVisualAssets();

    // Open game menu at start
    enter(); 
}

void MenuState::fixVisualAssets() {

    sf::Sprite& startbSprite = buttons[0].getSprite();
    sf::Sprite& optionsbSprite = buttons[1].getSprite();

    //asset resizing
    renderer.resizeSprite("background", background);
    renderer.resizeSprite("startb", startbSprite);
    renderer.resizeSprite("optionsb", optionsbSprite);

    //asset origin setting

    startbSprite.setOrigin(startbSprite.getLocalBounds().getCenter());
    optionsbSprite.setOrigin(optionsbSprite.getLocalBounds().getCenter());


    //asset positioning
    auto windowSize = renderer.getWindowSize();

    const float verticalShift = 100.f; //this is a visual shift needed to set up the correct position for the two buttons : START, OPTIONS

    startbSprite.setPosition({ float(windowSize.x) / 2, -verticalShift + float(windowSize.y) / 2 }); //  up shift (-) 
    optionsbSprite.setPosition({ float(windowSize.x) / 2, verticalShift + float(windowSize.y) / 2 }); // down shift

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
}

void MenuState::update(float deltaTime) {

    for (auto& button : buttons) {

        button.update(deltaTime);
    }
}

void MenuState::render(RenderSystem& renderer) {

    renderer.draw(background);

    for (auto& button : buttons) {

        renderer.draw(button.getSprite());
    }
}
