#include "states/ShopState.h"
#include "rendering/UI_Button.h"

#include <iostream>

ShopState::ShopState(StateManager& stateManager, RenderSystem& renderer, GameRun* current_run) :
    stateManager(stateManager),
    renderer(renderer),
    currentRun(current_run),
    shopSprite(renderer.getTextureManager().get("shop"))
{
    enter(); // Open shop

    fixVisualAssets();

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



    //button -> no risize
    itemsForSale[0].getSprite().setOrigin(itemsForSale[0].getSprite().getLocalBounds().getCenter());
    itemsForSale[0].getSprite().setScale({ 2.f , 2.f });
    itemsForSale[0].getSprite().setPosition({ float(windowSize.x) / 2, float(windowSize.y) / 2 });

}



void ShopState::enter() {
    std::cout << "Entering Shop ------------------------" << std::endl;

    generateShop();

}

void ShopState::generateShop() {
    
    //right now the shop generates only a coin bag
    itemsForSale.emplace_back(renderer, "coin_bag", [this]() {
        this->buyItem("coin_bag");
        });

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
}

void ShopState::update(float deltaTime) {

    currentRun->update(deltaTime);

    for (auto& itemButton : itemsForSale) {

        itemButton.update(deltaTime);
    }

}

void ShopState::render(RenderSystem& renderer) {

    

    currentRun->render(renderer);

    renderer.draw(shopSprite);


    //render items for sale
    for (auto& itemButton : itemsForSale) {

        renderer.draw(itemButton.getSprite());
    }


}

void ShopState::handleClick(sf::Vector2f worldPos) {

    

}
//TODO
void ShopState::buyItem(const std::string & itemButton) {

    if (1 > currentRun->getCoins()) { //item cannot be bought (possible feature to go in debt?)
        
        std::cout << "ITEM : cannot be bought: it costs " <<
            1 << " and you have " << currentRun->getCoins() << std::endl;
        return;
    }
    if (currentRun->isInventoryFull()) {

        std::cout << "Inventory is full!\n" << std::endl;
        return;

    }

    itemsForSale[0].disabled = true; //to be fixed

    std::cout << "ITEM BOUGHT :  has been added to the inventory!\n";

    currentRun->addCoins(-1); // in the future it will be useful to add "subtract coins etc..."

    currentRun->addItem("coin_bag");

}





