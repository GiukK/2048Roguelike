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

    //------

    GameRun(RenderSystem& renderer, PlayState* playState);

    void enter() ;
    void exit() ;

    void new_turn(const Board& initial_board);
    void go_back();

    void openShop();

    void handleInput(sf::Event& event) ;
    void update(float deltaTime) ;
    void render(RenderSystem& renderer);


    // right now this field is public since the closing of the shop is currently handled by ShopState
    // in the future it will probably make sense to fully handle the UI internally by creating public functions that modify private fields.

    bool shopOpen{ 0 };

    void addCoins(int count);
    void addItem(std::string item_name);

    //getters
    bool isInventoryFull();
    void popInventory(); //temporary to make items disappear

    const RenderSystem& getRenderer() const;
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

    sf::Sprite backUI;

    std::vector<UI_Button> inventoryButtons;



    //------

    int coins{100};

    unsigned short int maxInventorySize{ 3 } ;

    PlayState* playState;
    std::stack<std::unique_ptr<Turn>> run_turns;

    //------



};