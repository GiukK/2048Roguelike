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
    //asset positioning
    auto windowSize = renderer.getWindowSize();
    const float verticalShift = 100.f; //this is a visual shift: START, OPTIONS

    UI_Button startb(renderer, "startb", { float(windowSize.x) / 2, -verticalShift + float(windowSize.y) / 2 }, [&stateManager, &renderer]() {
        stateManager.pushState(std::make_unique<PlayState>(stateManager, renderer));
        });
    UI_Button optionsb(renderer, "optionsb", { float(windowSize.x) / 2, verticalShift + float(windowSize.y) / 2 }, []() {
        std::cout << "Options clicked!\n";
        });

    buttons.emplace_back(startb);
    buttons.emplace_back(optionsb);

    fixVisualAssets();

    // Open game menu at start
    enter(); 
}

void MenuState::fixVisualAssets() {

    renderer.resizeSprite("background", background);

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
