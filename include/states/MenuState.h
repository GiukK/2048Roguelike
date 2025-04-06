#pragma once

#include "core/GameState.h"
#include "core/StateManager.h"

#include "SFML/Audio.hpp"

class MenuState : public GameState {
public:
    MenuState(StateManager& stateManager, sf::RenderWindow& window);

    void enter() override;
    void exit() override;

    void handleInput(sf::Event& event) override;
    void update(float deltaTime) override;
    void render(sf::RenderWindow& window) override;

private:

    void updateButton(sf::Sprite& button);

    StateManager& stateManager;
    sf::RenderWindow& window;


    sf::Font font;
    sf::Text titleText;

    //UI
    sf::Texture backgroundTexture;
    sf::Texture startTexture;
    sf::Texture optionsTexture;

    sf::Sprite background;
    sf::Sprite startButton;
    sf::Sprite optionsButton;

    //AUDIO
    sf::SoundBuffer audioBuffer1, audioBuffer2;
    sf::Sound audioHover, audioClick;

    bool musicOn = false;
    sf::Music music;


};
