#pragma once

#include <iostream>
#include <random>
#include <stack>

#include <SFML/Graphics.hpp>
#include "rendering/UI_Button.h"

#include "core/Turn.h"

class PlayState;
class RenderSystem;

class GameRun {

public:

    //------

    GameRun(RenderSystem& renderer, PlayState* playState);

    void enter() ;
    void exit() ;

    void new_turn(const Board& initial_board);
    void go_back();

    bool shopOpen{ 0 };
    void openShop();

    void handleInput(sf::Event& event) ;
    void update(float deltaTime) ;
    void render(RenderSystem& renderer);


    void addCoins(int count);
    void addItem(std::string item_name);

    //getters
    bool isInventoryFull();
    void popInventory(); //temporary to make items disappear

    const RenderSystem& getRenderer() const;
    PlayState* getPlayState();

    const int getCoins() const;

    int getRandomInt(int min, int max);
    float getRandomFloat(float min, float max);



private:

    unsigned int randomSeed;
    std::mt19937 rng;

    bool gameStarted{ 0 };


    //------

    //this function will later be part of a more broad UI.h
    void drawCounter(sf::RenderWindow& window, unsigned int count, std::string asset);

    RenderSystem& renderer;
    PlayState* playState;

    sf::Sprite backUI;

    std::vector<UI_Button> inventoryButtons;



    //------

    int coins{100};

    unsigned short int maxInventorySize{ 3 } ;

    std::stack<std::unique_ptr<Turn>> run_turns;

    //------



};