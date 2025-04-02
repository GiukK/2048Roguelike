#include "core/Board.h"

Board::Board() :
	boardTexture("assets/textures/board.png", false, sf::IntRect({ 0, 0 }, { 32, 32 })),
	board(boardTexture)
{

	board.setOrigin({ 16.f,16.f });
	board.setScale({ 4.f, 4.f });

	//------

	slots[{0, 0}] = std::make_shared<Slot>();
}



void Board::render(sf::RenderWindow& window) {

	window.draw(board);

}

bool Board::isAdjacent(const Coord& newCoord) const {
    for (const auto& offset : directions) {
        Coord adjacent{ newCoord.x + offset.x, newCoord.y + offset.y };
        if (slots.find(adjacent) != slots.end()) {
            return true;
        }
    }
    return false;
}

bool Board::addSlot(const Coord& newCoord) {
    if (slots.find(newCoord) != slots.end()) {
        std::cerr << "Slot già presente!\n";
        return false;
    }
    if (!isAdjacent(newCoord)) {
        std::cerr << "Posizione non valida!\n";
        return false;
    }
    slots[newCoord] = std::make_shared<Slot>();
    return true;
}

void Board::spawnTile(const Coord& pos, int value) {
    auto it = slots.find(pos);
    if (it != slots.end() && it->second->isEmpty()) {
        it->second->setTile(std::make_unique<Tile>(value));
    }
}

void Board::moveTiles(Coord direction) {
    std::map<Coord, std::shared_ptr<Slot>> newSlots;

    for (auto& [pos, slot] : slots) {
        if (!slot->isEmpty()) {
            Coord newPos = pos;

            while (slots.find({ newPos.x + direction.x, newPos.y + direction.y }) != slots.end()) {
                Coord nextPos = { newPos.x + direction.x, newPos.y + direction.y };

                if (slots[nextPos]->isEmpty()) {
                    slots[nextPos]->setTile(slot->removeTile());
                    newPos = nextPos;
                }
                else if (slots[nextPos]->getTile()->canMergeWith(*slot->getTile())) {
                    slots[nextPos]->getTile()->mergeWith(*slot->getTile());
                    slot->removeTile();
                    break;
                }
                else {
                    break;
                }
            }
        }
    }
}