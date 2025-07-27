#include "core/utils/saleItem.h"
#include "rendering/RenderSystem.h"


saleItem::saleItem(std::string name, RenderSystem& renderer) :
name(name),
sprite(renderer.getTextureManager().get(name))
{

	sprite.setOrigin(sprite.getGlobalBounds().getCenter());

	sf::Vector2u windowSize(renderer.getWindowSize());
	sprite.setPosition({ float(windowSize.x) / 2, float(windowSize.y) / 2 });

	renderer.resizeSprite("coin_bag", sprite);

}