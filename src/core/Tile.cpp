#include "core/Tile.h"
#include "core/Slot.h"
#include "rendering/RenderSystem.h"


//the entirety of this class has to be rethought as a more solid and less visual-oriented (sprites)

Tile::Tile(RenderSystem& renderer , Slot* slot, int value) :
value(value),
renderer(renderer),
tile(renderer.getTextureManager().get(std::to_string(value))),
slot(slot)
{
	fixVisualAssets();
}




void Tile::fixVisualAssets() {

	tile.setOrigin({ tile.getGlobalBounds().size.x / 2 , tile.getGlobalBounds().size.y / 2 });
	tile.setScale({ 2.f, 2.f });
	tile.setPosition(slot->getSlotSprite().getPosition());

}



void Tile::render( RenderSystem& renderer) {

	renderer.draw(tile);

}

//changes ownership of tile ALSO manages internal slot ownership (tile-><-slot) 
void Tile::changeSlot(Slot* a, Slot* b) {

	this->slot = b;

	//tile.setPosition(b->slot.getPosition());

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

//the in-merging tile is the one destroyed
void Tile::mergeIntoSlot(Slot* other) {

	std::cout << "Slot merged at: x = " << other->getCoord().x << " and y = " << other->getCoord().y << std::endl;

	int sum = value + other->tile->getValue();
	other->tile->setValue(sum);
	slot->removeTile();


	other->tile->changeSprite();
	other->triggerMergeEffects();

}


void Tile::changeSprite() {

	std::string val = std::to_string(value);

	tile.setTexture(renderer.getTextureManager().get(val));

}