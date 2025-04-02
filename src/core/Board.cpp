#include "core/Board.h"

Board::Board() :
	boardTexture("assets/textures/board.png", false, sf::IntRect({ 0, 0 }, { 32, 32 })),
	board(boardTexture)
{

	board.setOrigin({ 16.f,16.f });
	board.setScale({ 4.f, 4.f });

	//------
}



void Board::render(sf::RenderWindow& window) {

	window.draw(board);

}
