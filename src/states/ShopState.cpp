#include "states/ShopState.h"
#include <iostream>

ShopState::ShopState(StateManager& stateManager, RenderSystem& renderer, GameRun* current_run) :
    stateManager(stateManager),
    renderer(renderer),
    currentRun(current_run),
    shopSprite(renderer.getTextureManager().get("shop"))
{

    fixVisualAssets();


    enter(); // Open shop
}


void ShopState::fixVisualAssets() {
    //asset resizing
    renderer.resizeSprite("shop", shopSprite);

    //asset origin setting

    shopSprite.setOrigin(shopSprite.getLocalBounds().getCenter());

    //asset positioning
    auto windowSize = renderer.getWindowSize();

    shopSprite.setPosition({ float(windowSize.x) / 2, float(windowSize.y) / 2 }); //  up shift (-) 

    std::cout << "ShopState visual assets: ready" << std::endl;

}



void ShopState::enter() {
    std::cout << "Entering Shop ------------------------" << std::endl;

    generateShop();

}

void ShopState::generateShop() {
    

    //right now the shop generates only a coin bag
    saleItem coin_bag("coin_bag", renderer);
    itemsForSale.push_back(coin_bag);

}

void ShopState::exit() {

    std::cout << "Exiting Shop ------------------------" << std::endl;

    itemsForSale.clear();

    currentRun->shopOpen = false;

    stateManager.popState();
}

void ShopState::handleInput(sf::Event& event) {
    if (const auto* keyPressed = event.getIf<sf::Event::KeyReleased>()) {
        if (keyPressed->scancode == sf::Keyboard::Scancode::Escape) {
            std::cout << "Exiting shop..." << std::endl;

            // right now this field is public since the closing of the shop is currently handled by ShopState
            // in the future it will probably make sense to fully handle the UI internally in GameRun by creating public functions that modify private fields.

            exit();  // Close shop
            return;
        }

    }
        //in the future it will be nice to have down-up button animation when pushed in sync with onRelease and isPressed
    if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left))
    {
        auto& window = renderer.getWindow();

        sf::Vector2i mousePos = sf::Mouse::getPosition(window);  // da PlayState o GameRun
        sf::Vector2f worldPos = window.mapPixelToCoords(mousePos);

        handleClick(worldPos); //works fine but its just a test
    }
}

void ShopState::update(float deltaTime) {

}

void ShopState::render(RenderSystem& renderer) {

    

    currentRun->render(renderer);

    renderer.draw(shopSprite);


    //render items for sale
    for (auto item : itemsForSale) {

        renderer.draw(item.sprite);

    }


}

void ShopState::handleClick(sf::Vector2f worldPos) {

    for (auto& item : itemsForSale) {
        sf::FloatRect bounds = item.sprite.getGlobalBounds();

        if (bounds.contains(worldPos) and not item.sold) { //buy more items ? (remove item.sold)

            buyItem(item);

            return;

        }
    }

}

void ShopState::buyItem(saleItem& item) {

    if (item.price > currentRun->getCoins()) { //item cannot be bought (possible feature to go in debt?)
        
        std::cout << "ITEM : " << item.name << " cannot be bought: it costs " <<
            item.price << " and you have " << currentRun->getCoins() << std::endl;
        return;
    }
    if (currentRun->isInventoryFull()) {

        std::cout << "Inventory is full!\n" << std::endl;
        return;

    }

    item.sprite.setColor(sf::Color(255, 100, 100));  // apply filter (atm)

    item.sold = true;

    std::cout << "ITEM BOUGHT : " << item.name << " has been added to the inventory!\n";

    currentRun->addCoins(-item.price); // in the future it will be useful to add "subtract coins etc..."

    currentRun->addItem(item.name);

}





