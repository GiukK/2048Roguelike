#include "core/Tile.h"
#include "core/Slot.h"

Tile::Tile(Slot* slot, int value) :
value(value),
slot(slot),
tileTexture("assets/textures/" + std::to_string(value) + ".png", false, sf::IntRect({ 0, 0 }, { 32, 32 })),
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
void Tile::changeSlot(Slot* a, Slot* b) {

	this->slot = b;

	tile.setPosition(b->slot.getPosition());

	b->setTile(a->releaseTile());

}

sf::Vector2f Tile::getPosition() {

	return tile.getPosition();

}


int Tile::getValue() const {

	return value;

}

void Tile::setValue(int x) {

	value = x;

}

void Tile::mergeIntoSlot(Slot* other) {

	int sum = value + other->tile->getValue();

	other->tile->setValue(sum);

	slot->removeTile();


	other->tile->changeSprite();

	other->triggerMergeEffects();

}

void Tile::changeSprite() {

	std::cerr << "changed!1" << std::endl;

	std::string val = std::to_string(value);

	if (!tileTexture.loadFromFile("assets/textures/" + val + ".png")) {
		std::cerr << "Error loading tile texture!" << std::endl;
		return;
	}

	tile.setTexture(tileTexture);

	std::cerr << "changed!2" << std::endl;
}