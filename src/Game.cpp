#include <SFML/Graphics.hpp>
#include "Game.h"
#include "states/MenuState.h"  // Start with the main menu

Game::Game() : window(sf::VideoMode::getFullscreenModes()[0], "My 2D Game", sf::Style::None) //set to None when deployed
{
    window.setFramerateLimit(100); //limit framerate
    stateManager.pushState(std::make_unique<MenuState>(stateManager, window));  // Push initial state
}

void Game::run() {
    while (window.isOpen()) {
        processEvents();
        update(clock.restart().asSeconds());
        render();
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
    stateManager.render(window);  // Render the current game state
    window.display();
}
