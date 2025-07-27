#include <memory>
#include <iostream>
#include "core/Gamerun.h"
#include "states/PlayState.h"
#include "states/ShopState.h"
#include "rendering/RenderSystem.h"
#include "rendering/TextureManager.h"
//#include "core/utils/saleItem.h"


GameRun::GameRun(RenderSystem& renderer, PlayState* playState) :
    renderer(renderer),
    playState(playState),
    backUI(renderer.getTextureManager().get("backUI")),
    coin_animation(renderer.getTextureManager().get("coin_animation"))
{

    //future fixVisualAssets as the others
    renderer.resizeSprite("backUI", backUI);

    renderer.resizeSprite("coin_animation", coin_animation);

    coin_animation.setPosition({ 20.f, 850.f });
    //animation starts at 0, avoids full sprite artifact
    coin_animation.setTextureRect(sf::IntRect({ int(coin_ani_frame * 32), 0 }, { 32, 32 })); 


    //to fix this using local time
    auto rd = std::random_device{};
    randomSeed = rd();
    rng.seed(randomSeed);

    gameStarted = true;

    run_turns.push(std::make_unique<Turn>(renderer , this));

}

void GameRun::enter() {

	std::cout << "currentRun entered" << std::endl;

}

void GameRun::exit() {

    std::cout << "currentRun exited" << std::endl;

}


void GameRun::new_turn(const Board& initial_board) {
    run_turns.push(std::make_unique<Turn>(renderer , this, initial_board));

    std::cout << " -------------------------------" << "Turn " << run_turns.size() << " -------------------------------" << std::endl;

}

void GameRun::go_back() {
    if (run_turns.size() <= 1) {
        std::cout << "Nessun turno precedente disponibile." << std::endl;
        return;
    }

    // Rimuove l'ultimo turno
    run_turns.pop();
    std::cout << "Turno Rimosso - Tornato al Turno : " << run_turns.size()  << std::endl;

}

void GameRun::openShop() {

    if (shopOpen) { 
        std::cout << "Shop will not open since it is already been opened\n";
        return;
    }

    std::cout << "Opening shop...\n";
    shopOpen = true;
    playState->stateManager.pushState(std::make_unique<ShopState>(playState->stateManager, renderer, this));

}

void GameRun::handleInput(sf::Event& event) {

    if (!run_turns.empty()) {
        run_turns.top()->handleInput(event);
    }

}

void GameRun::update(float deltaTime) {

    run_turns.top()->update(deltaTime);

    coin_ani_elapsed += deltaTime;


    //all this CAN and WILL be centralized, possibly in the renderer and with a centralized deltatime, fps and animation handling.

    if (coin_ani_elapsed > 1.f / 12.f) {

        coin_ani_frame += 1;

        if (coin_ani_frame >= 8) {
            coin_ani_frame = 0;
        }
        coin_animation.setTextureRect(sf::IntRect({ int(coin_ani_frame * 32), 0 }, { 32, 32 })); //animation starts at 0
        
        coin_ani_elapsed = 0;


        
    }
}

//writes a counter with the number of the turn-  to be deprecated
void GameRun::drawCounter(sf::RenderWindow& window, unsigned int count, std::string asset) {
    std::string countStr = std::to_string(count);
    float digitScale = 10.f;
    float digitWidth = 5.f;
    float digitHeight = 7.f;

    const float YSHIFT = 18.f;
    float XSHIFT{};

    if (asset == "turns") {

        XSHIFT = 340.f;
    }
    else if (asset == "coins") {

        XSHIFT = 1000.f;
    }

    for (std::size_t i = 0; i < countStr.size(); ++i) {
        int digit = countStr[i] - '0';

        sf::Texture digitTexture("assets/textures/digits.png", false,
            sf::IntRect({ int(digit * digitWidth), 0 }, { int(digitWidth), int(digitHeight) }));

        sf::Sprite digitSprite(digitTexture);
        digitSprite.setOrigin({ 0.f, 0.f });
        digitSprite.setScale({ digitScale, digitScale });
        digitSprite.setPosition({ i * digitWidth * digitScale + XSHIFT, YSHIFT });

        window.draw(digitSprite);
    }
}



void GameRun::render(RenderSystem& renderer) {

    renderer.draw(backUI);

    renderer.draw(coin_animation);

    run_turns.top()->render(renderer);


    //in the future this function will not exist, all the ui will be centralized
    drawCounter(renderer.getWindow(), run_turns.size(), "turns");
    drawCounter(renderer.getWindow(), coins , "coins");

    //draw inventory
    for (auto& item : inventory) {

        renderer.draw(item.sprite);

    }


}



int GameRun::getRandomInt(int min, int max) {
    std::uniform_int_distribution<int> dist(min, max);
    return dist(rng);
}

float GameRun::getRandomFloat(float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(rng);
}

const RenderSystem& GameRun::getRenderer() const {

    return renderer;

}

void GameRun::addCoins(int count) {

    coins += count;

    return;

}

const int GameRun::getCoins() const  {

    return coins;

}

std::vector<saleItem>& GameRun::getInventory() {

    return inventory;

}

bool GameRun::isInventoryFull() {

    return inventory.size() == maxInventorySize;

}

void GameRun::addItem(std::string item_name) {

    saleItem item(item_name, renderer);

    //hard positioning for gamerun ui -> to be deprecated and centralized into an UI manager

    item.sprite.setPosition({ 1500.f, 400.f + 100.f*inventory.size()});
    item.sprite.setScale({ 1.f, 1.f });
    //

    //in the future this will be a better structured container and class/struct
    inventory.push_back(item);

}