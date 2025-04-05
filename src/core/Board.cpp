#include "core/Board.h"
#include "core/Slot.h"

Board::Board() :
    boardTexture("assets/textures/board.png", false, sf::IntRect({ 0, 0 }, { 32, 32 })),
    board(boardTexture)
{
    //set sprite origin and scale
    board.setOrigin({ 16.f,16.f });
    board.setScale({ 4.f, 4.f });
    //------
    const float shift = 300.f;
    for (int i = 0; i <= 3; ++i) {
        for (int j = 0; j <= 3; ++j) {
            slots[{i, j}] = std::make_shared<Slot>(i, j);
        }
    }

}



void Board::render(sf::RenderWindow& window) {
    // Iterate over the map using a range-based for loop
    for (const auto& pair : slots) {
        //const Coord& coord = pair.first;  // The key (Coord)
        const std::shared_ptr<Slot>& slot = pair.second;  // The value (shared_ptr<Slot>)

        slot->render(window);
    }
}
void Board::start() {
    std::cout << "size is ---> " << slots.size() << std::endl;
}
void Board::setRng(std::mt19937 rng) {
    this->rng = rng;
}
int Board::getRandomInt(int min, int max) {
    std::uniform_int_distribution<int> dist(min, max);
    return dist(rng);
}
float Board::getRandomFloat(float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(rng);
}

// Function to spawn a tile in a random empty slot
void Board::spawnTileInRandomEmptySlot() {
    std::vector<std::shared_ptr<Slot>> emptySlots;

    for (const auto& entry : slots) {
        if (entry.second->isEmpty()) {
            emptySlots.push_back(entry.second);
        }
    }

    if (!emptySlots.empty()) {
        int randomIndex = getRandomInt(0, emptySlots.size() - 1);
        auto randomSlot = emptySlots[randomIndex];

        randomSlot->setTile(std::make_unique<Tile>(randomSlot));

        std::cout << "Spawned tile at (row=" << randomSlot->row << ", col=" << randomSlot->col << ")\n";
    }
    else {
        std::cout << "No empty slots available to spawn a tile.\n";
    }
}


void Board::moveLeft() {
    for (int y = row_range.first; y <= row_range.second; ++y) {

        for (int x = col_range.first; x <= col_range.second; ++x) {

            if (!slots.count({ x,y })) { continue; } // break if the slot isnt there (has been removed)
            
            if (!slots[{ x, y}]->isEmpty() and slots.count({ x - 1, y }) and slots[{ x - 1, y }]->isEmpty()) {


                slots[{ x, y}]->tile->changeSlot(slots[{ x, y}], slots[{ x-1, y}]);

                std::cout << "tile moved!\n";
            }
        }
    }
}


void Board::moveRight() {
    for (int y = row_range.first; y <= row_range.second; ++y) {

        for (int x = col_range.second; x >= col_range.first; --x) {

            if (!slots.count({ x,y })) { continue; } // break if the slot isnt there (has been removed)

            if (!slots[{ x, y}]->isEmpty() and slots.count({ x + 1, y }) and slots[{ x + 1, y }]->isEmpty()) {


                slots[{ x, y}]->tile->changeSlot(slots[{ x, y}], slots[{ x + 1, y}]);

                std::cout << "tile moved!\n";
            }
        }
    }
}

void Board::moveUp() {
    for (int x = col_range.first; x <= col_range.second; ++x) {

        for (int y = row_range.first; y <= row_range.second; ++y) {

            if (!slots.count({ x,y })) { continue; } // break if the slot isnt there (has been removed)

            if (!slots[{ x, y}]->isEmpty() and slots.count({ x , y - 1 }) and slots[{ x , y - 1}]->isEmpty()) {


                slots[{ x, y}]->tile->changeSlot(slots[{ x, y}], slots[{ x , y - 1}]);

                std::cout << "tile moved!\n";
            }  
        }
    }
}

void Board::moveDown() {
    for (int x = col_range.first; x <= col_range.second; ++x) {

        for (int y = row_range.second; y >= row_range.first; --y) {

            if (!slots.count({ x,y })) { continue; } // break if the slot isnt there (has been removed)

            if (!slots[{ x, y}]->isEmpty() and slots.count({ x , y + 1 }) and slots[{ x, y + 1}]->isEmpty()) {


                slots[{ x, y}]->tile->changeSlot(slots[{ x, y}], slots[{ x, y + 1}]);

                std::cout << "tile moved!\n";
            }
        }
    }
}

void Board::clear() {

    for (const auto& entry : slots) {
        if (!entry.second->isEmpty()) {
            entry.second->removeTile();

            std::cout << "removed tile from col=" << entry.first.x << " row=" << entry.first.y << std::endl;
        }
    }
}
