#include "core/Tile.h"
#include "core/Slot.h"

Tile::Tile(std::shared_ptr<Slot> slot) :
slot(slot),
tileTexture("assets/textures/2sprite.png", false, sf::IntRect({ 0, 0 }, { 32, 32 })),
tile(tileTexture)
{

	tile.setOrigin({ 16.f,16.f });
	tile.setScale({ 2.f, 2.f });
	tile.setPosition(slot->slot.getPosition());
}



void Tile::render(sf::RenderWindow& window) {

	window.draw(tile);

}

//changes ownership of tile ALSO manages internal slot ownership (tile-><-slot) 
void Tile::changeSlot(std::shared_ptr<Slot> a, std::shared_ptr<Slot> b) {

	this->slot = b;

	tile.setPosition(b->slot.getPosition());

	b->setTile(a->releaseTile());

}

sf::Vector2f Tile::getPosition() {

	return tile.getPosition();

}