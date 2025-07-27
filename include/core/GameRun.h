#pragma once

#include <iostream>
#include <random>
#include <stack>

#include <SFML/Graphics.hpp>
#include "core/Turn.h"
#include "core/utils/saleItem.h"

class PlayState;
class RenderSystem;
class saleItem;

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

    std::vector<saleItem>& getInventory();

    const RenderSystem& getRenderer() const;
    const int getCoins() const;

private:

    unsigned int randomSeed;
    std::mt19937 rng;

    bool gameStarted{ 0 };


    //------

    //this function will later be part of a more broad UI.h
    void drawCounter(sf::RenderWindow& window, unsigned int count, std::string asset);

    RenderSystem& renderer;

    sf::Sprite backUI;

    sf::Sprite coin_animation;

    char coin_ani_frame{ 0 };
    float coin_ani_elapsed{ 0 };

    //------

    int coins{0};

    unsigned short int maxInventorySize{ 6 } ;
    std::vector<saleItem> inventory;

    PlayState* playState;
    std::stack<std::unique_ptr<Turn>> run_turns;

    //------


    int getRandomInt(int min, int max);
    float getRandomFloat(float min, float max);

};