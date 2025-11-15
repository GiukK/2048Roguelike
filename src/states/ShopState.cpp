#include "states/ShopState.h"
#include "rendering/UI_Button.h"
#include "rendering/Animation.h"

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

    for (int i = 1; i <= 14; i++) {
        
        //ani
        Animation anim(renderer, "coin_animation", { 32, 32 }, 8, { 120.f * i, 900.f });
    
        animations.emplace_back(std::move(anim));

    }

    //......
    //asset resizing
    renderer.resizeSprite("shop", shopSprite);
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
    
    std::cout << "shop generated - empty" << std::endl;

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

    for (auto& ani : animations) {

        ani.update(deltaTime);
    }

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

    //render animation
    for (auto& ani : animations) {

        renderer.draw(ani.getSprite());
    }
}

void ShopState::handleClick(sf::Vector2f worldPos) {

    

}
//TODO
void ShopState::buyItem(const std::string & itemButton) {


}





