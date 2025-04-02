#include "core/Tile.h"

Tile::Tile(): 
tileTexture("assets/textures/2sprite.png", false, sf::IntRect({ 0, 0 }, { 32, 32 })),
tile(tileTexture)
{

	tile.setOrigin({ 16.f,16.f });
	tile.setScale({ 4.f, 4.f });
}



void Tile::render(sf::RenderWindow& window) {

	window.draw(tile);

}


void Tile::spawn(sf::Sprite board, int x, int y) {

	float xshift{};
	float yshift{};

	float halfTile = board.getGlobalBounds().size.x / 8.f;

	if (x == 1) { xshift -= halfTile * 3; }
	if (x == 2) { xshift -= halfTile; }
	if (x == 3) { xshift += halfTile; }
	if (x == 4) { xshift += halfTile * 3; }

	if (y == 1) { yshift -= halfTile * 3; }
	if (y == 2) { yshift -= halfTile; }
	if (y == 3) { yshift += halfTile; }
	if (y == 4) { yshift += halfTile * 3; }

	sf::Vector2f boardPos = board.getPosition();

	sf::Vector2f pos = boardPos + sf::Vector2f({xshift, yshift});

	tile.setPosition(pos);
}