#include <SFML/Graphics.hpp>
#include "Game.h"
#include "states/MenuState.h"  // Start with the main menu

Game::Game() : window(sf::VideoMode::getFullscreenModes()[0], "<0>", sf::Style::None), //set to None when deployed
               renderer(window)
{
    window.setFramerateLimit(100); //limit framerate

    // Init layout and texture
    renderer.initialize(window.getSize());

    stateManager.pushState(std::make_unique<MenuState>(stateManager, renderer));  // Push initial state

}


//game loop
void Game::run() {
    while (window.isOpen()) {
        processEvents(); //input handliing
        update(clock.restart().asSeconds()); //clock restart and update deltaTime
        render(); //rendering
    }
}

void Game::processEvents() {
    std::optional<sf::Event> event;
    while (event = window.pollEvent()) { 
        if (event->is<sf::Event::Closed>())
            window.close();

        stateManager.handleInput(event.value());  // Delegate input handling to the current state
    }
}

void Game::update(float deltaTime) {
    stateManager.update(deltaTime);  // Update the active game state
}

void Game::render() {
    window.clear();
    stateManager.render(renderer);  // Render the current game state
    window.display();
}
