#include "states/MenuState.h"
#include "states/PlayState.h"

#include <iostream>

MenuState::MenuState(StateManager& stateManager, sf::RenderWindow& window) :
    stateManager(stateManager),
    window(window),

    titleText(font),

    backgroundTexture("assets/textures/background.png", false, sf::IntRect({ 0, 0 }, { 192, 108 })),
    startTexture("assets/textures/startb.png", false, sf::IntRect({ 0, 0 }, { 32, 16 })),
    optionsTexture("assets/textures/optionsb.png", false, sf::IntRect({ 0, 0 }, { 32, 16 })),

    background(backgroundTexture),
    startButton(startTexture),
    optionsButton(optionsTexture),

    audioBuffer1("assets/sounds/hover.wav"),
    audioBuffer2("assets/sounds/click.wav"),
    audioClick(audioBuffer1),
    audioHover(audioBuffer2)

{

    //Text UI deprecated
    /*
    if (!font.openFromFile("assets/fonts/default.ttf"))
    {
        std::cerr << "Error loading font!" << std::endl;
    }

    titleText.setString("Main Menu\nPress ENTER to Start\nPress ESC to Quit");
    titleText.setCharacterSize(30);
    titleText.setFillColor(sf::Color::White);
    titleText.setPosition({ window.getSize().x / 2.f - 100, window.getSize().y / 3.f });
    */

    //Button UI

    // BACKGROUND

    float scaleFactor = 10.f;

    background.setScale({ scaleFactor, scaleFactor });
    background.setOrigin({ 192 / 2, 108 / 2 });
    background.setPosition({ window.getSize().x / 2.f , window.getSize().y / 2.f });

    // START
    startButton.setScale({ scaleFactor,scaleFactor });
    startButton.setOrigin({ 16,8 });
    startButton.setPosition({ window.getSize().x / 2.f , window.getSize().y / 2.f - 100});

    //OPTIONS
    optionsButton.setScale({ scaleFactor,scaleFactor });
    optionsButton.setOrigin({ 16,8 });
    optionsButton.setPosition({ window.getSize().x / 2.f , window.getSize().y / 2.f + 100 });


    enter(); // Open game
}

void MenuState::enter() {
    std::cout << "Entering Menu State" << std::endl;
}

void MenuState::exit() {
    std::cout << "exit() was called from MenuState" << std::endl;
    window.close();
}

void MenuState::handleInput(sf::Event& event) {
    if (const auto* keyPressed = event.getIf<sf::Event::KeyReleased>()) {
        if (keyPressed->scancode == sf::Keyboard::Scancode::Enter) {
            std::cout << "Starting Game..." << std::endl;
            // TODO: Transition to PlayState
        }
        else if (keyPressed->scancode == sf::Keyboard::Scancode::Escape) {
            std::cout << "Exiting Game..." << std::endl;


            exit();  // Close game
        }
    }
    if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {

        //HANDLE MOUSE POSITIONS
        sf::Vector2i mousePos_i = sf::Mouse::getPosition(window);
        // Convert to world coordinates (float)
        sf::Vector2f mousePos_f = window.mapPixelToCoords(mousePos_i);
        //----------------------------------------------------------------------

        if (startButton.getGlobalBounds().contains(mousePos_f)) {

            std::cout << "START pressed" << std::endl;
            audioClick.play();

            stateManager.pushState(std::make_unique<PlayState>(stateManager, window));
        }

        if (optionsButton.getGlobalBounds().contains(mousePos_f)) {

            std::cout << "OPTIONS pressed" << std::endl;
            audioClick.play();
        }


    }
}

void MenuState::update(float deltaTime) {

    updateButton(startButton);
    updateButton(optionsButton);
}

void MenuState::render(sf::RenderWindow& window) {

    //Text UI deprecated
    //window.draw(titleText);

    window.draw(background);

    window.draw(startButton);

    window.draw(optionsButton);
}

void MenuState::updateButton(sf::Sprite& button) {


    sf::Vector2i mousePos = sf::Mouse::getPosition(window);

    if (button.getGlobalBounds().contains(static_cast<sf::Vector2f>(mousePos)) and button.getColor() == sf::Color::White) {

        button.setColor(sf::Color::Red);
        audioHover.play();

    }
    else if (!button.getGlobalBounds().contains(static_cast<sf::Vector2f>(mousePos))) { button.setColor(sf::Color::White); }

}
