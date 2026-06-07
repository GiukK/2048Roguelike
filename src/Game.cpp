#include "Game.h"
#include "states/MenuState.h"

Game::Game()
    : window(sf::VideoMode::getFullscreenModes()[0], "<0>", sf::Style::None),
      renderer(window)
{
    window.setFramerateLimit(100);
    renderer.initialize(window.getSize());
    stateManager.pushState(std::make_unique<MenuState>(stateManager, renderer));
}

void Game::run() {
    while (window.isOpen()) {
        processEvents();
        update(clock.restart().asSeconds());
        render();
    }
}

void Game::processEvents() {
    while (auto event = window.pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            window.close();
            return;
        }
        stateManager.handleInput(event.value());
    }
}

void Game::update(float deltaTime) {
    stateManager.update(deltaTime);
}

void Game::render() {
    window.clear();
    stateManager.render(renderer);
    window.display();
}
