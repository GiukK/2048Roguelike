#include "core/Board.h"
#include "core/Slot.h"

Board::Board() :
    boardTexture("assets/textures/board.png", false, sf::IntRect({ 0, 0 }, { 64, 32 })),
    board(boardTexture)
{
    //set sprite origin and scale
    board.setOrigin({ 0.f, 0.f });
    board.setScale({ 22.f, 22.f });
    board.setPosition({ 160.f,140.f });
    //------
    const float shift = 300.f;
    for (int i = -0; i <= 3; ++i) {
        for (int j = 0; j <= 3; ++j) {
            slots[{i, j}] = std::make_unique<Slot>(i, j);
        }
    }

    slots[{-1, 2}] = std::make_unique<Slot>(-1, 2);
    slots.erase({ 2, 2 });

}

Board::Board(const Board& other) :
    col_range(other.col_range),
    row_range(other.row_range),
    boardTexture("assets/textures/board.png", false, sf::IntRect({ 0, 0 }, { 64, 32 })),
    board(boardTexture),
    rng(other.rng)
{
    //set sprite origin and scale
    board.setOrigin({ 0.f, 0.f });
    board.setScale({ 22.f, 22.f });
    board.setPosition({ 160.f,140.f });
    //------

    // Primo passo: creare nuovi slot
    for (const auto& [coord, slotPtr] : other.slots) {
        slots[coord] = std::make_unique<Slot>(*slotPtr);
    }

    // Secondo passo: copiare le tile mantenendo riferimenti corretti agli slot
    for (const auto& [coord, slotPtr] : other.slots) {
        if (!slotPtr->isEmpty()) {
            Slot* mySlot = slots[coord].get();
            const Tile* originalTile = slotPtr->tile.get();
            mySlot->setTile(std::make_unique<Tile>(mySlot, originalTile->getValue()));
        }
    }
}

Board& Board::operator=(const Board& other) {
    if (this == &other) return *this;

    slots.clear();

    col_range = other.col_range;
    row_range = other.row_range;
    boardTexture = other.boardTexture;
    board = other.board;
    rng = other.rng;

    for (const auto& [coord, slotPtr] : other.slots) {
        slots[coord] = std::make_unique<Slot>(*slotPtr);
    }

    for (const auto& [coord, slotPtr] : other.slots) {
        if (!slotPtr->isEmpty()) {
            Slot* mySlot = slots[coord].get();
            const Tile* originalTile = slotPtr->tile.get();
            mySlot->setTile(std::make_unique<Tile>(mySlot, originalTile->getValue()));
        }
    }

    return *this;
}




void Board::render(sf::RenderWindow& window) {

    window.draw(board);

    // Iterate over the map using a range-based for loop
    for (const auto& pair : slots) {
        //const Coord& coord = pair.first;  // The key (Coord)
        const std::unique_ptr<Slot>& slot = pair.second;  // The value (shared_ptr<Slot>)

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
    std::vector<Slot*> emptySlots;

    for (const auto& entry : slots) {
        if (entry.second->isEmpty()) {
            emptySlots.push_back(entry.second.get());
        }
    }

    if (!emptySlots.empty()) {
        int randomIndex = getRandomInt(0, emptySlots.size() - 1);
        Slot* randomSlot = emptySlots[randomIndex];

        randomSlot->setTile(std::make_unique<Tile>(randomSlot,4));

        std::cout << "Spawned tile at (row=" << randomSlot->row << ", col=" << randomSlot->col << ")\n";
    }
    else {
        std::cout << "No empty slots available to spawn a tile.\n";
    }
}


void Board::moveLeft() {
    for (int y = row_range.first; y <= row_range.second; ++y) {
        // Start by checking each row from left to right
        for (int x = col_range.first; x <= col_range.second; ++x) {
            if (!slots.count({ x, y })) { continue; } // Skip if the slot isn't there (has been removed)

            if (!slots[{ x, y}]->isEmpty()) {
                int currentX = x;

                // Move the tile as far left as possible
                while (currentX > col_range.first and slots.count({ currentX - 1, y }) and slots[{ currentX - 1, y }]->isEmpty()) {
                    slots[{ currentX, y}]->tile->changeSlot(slots[{ currentX, y}].get(), slots[{ currentX - 1, y }].get());
                    currentX--; // Move the tile one step to the left
                    std::cout << "tile moved!\n";
                }

                
                // Check for merging tiles if the left tile is not empty and has the same value
                if (currentX > col_range.first && slots.count({ currentX - 1, y }) && !slots[{ currentX - 1, y }]->isEmpty() &&
                    slots[{ currentX - 1, y }]->tile->getValue() == slots[{ currentX, y}]->tile->getValue()) {
                    // Merge tiles and move one step left
                    // The current tile slot becomes 
                    slots[{ currentX, y}]->tile->mergeIntoSlot(slots[{ currentX - 1, y }].get());

                    std::cout << "tiles merged!\n";
                }
            }
        }
    }
}



void Board::moveRight() {
    for (int y = row_range.first; y <= row_range.second; ++y) {
        // Start by checking each row from right to left
        for (int x = col_range.second; x >= col_range.first; --x) {

            if (!slots.count({ x, y })) { continue; } // Skip if the slot isn't there (has been removed)

            if (!slots[{ x, y}]->isEmpty()) {
                int currentX = x;

                // Move the tile as far right as possible
                while (currentX < col_range.second && slots.count({ currentX + 1, y }) && slots[{ currentX + 1, y }]->isEmpty()) {
                    slots[{ currentX, y}]->tile->changeSlot(slots[{ currentX, y}].get(), slots[{ currentX + 1, y }].get());
                    currentX++; // Move the tile one step to the right
                    std::cout << "tile moved!\n";
                }

                // Check for merging tiles if the right tile is not empty and has the same value
                if (currentX < col_range.second && slots.count({ currentX + 1, y }) && !slots[{ currentX + 1, y }]->isEmpty() &&
                    slots[{ currentX + 1, y }]->tile->getValue() == slots[{ currentX, y}]->tile->getValue()) {
                    // Merge tiles and move one step right
                    slots[{ currentX, y}]->tile->mergeIntoSlot(slots[{ currentX + 1, y }].get());

                    std::cout << "tiles merged!\n";
                }
            }
        }
    }
}


void Board::moveUp() {
    for (int x = col_range.first; x <= col_range.second; ++x) {
        // Start by checking each column from top to bottom
        for (int y = row_range.first + 1; y <= row_range.second; ++y) {  // Start from 1 to allow the previous row to move tiles
            if (!slots.count({ x, y })) { continue; } // Skip if the slot isn't there (has been removed)

            if (!slots[{ x, y}]->isEmpty()) {
                int currentY = y;

                // Move the tile as far up as possible
                while (currentY > row_range.first && slots.count({ x, currentY - 1 }) && slots[{ x, currentY - 1 }]->isEmpty()) {
                    slots[{ x, currentY}]->tile->changeSlot(slots[{ x, currentY}].get(), slots[{ x, currentY - 1 }].get());
                    currentY--; // Move the tile one step up
                    std::cout << "tile moved!\n";
                }

                // Check for merging tiles if the top tile is not empty and has the same value
                if (currentY > row_range.first && slots.count({ x, currentY - 1 }) && !slots[{ x, currentY - 1 }]->isEmpty() &&
                    slots[{ x, currentY - 1 }]->tile->getValue() == slots[{ x, currentY}]->tile->getValue()) {
                    // Merge tiles and move one step up
                    slots[{ x, currentY}]->tile->mergeIntoSlot(slots[{ x, currentY - 1 }].get());

                    std::cout << "tiles merged!\n";
                }
            }
        }
    }
}


void Board::moveDown() {
    for (int x = col_range.first; x <= col_range.second; ++x) {
        // Start by checking each column from bottom to top
        for (int y = row_range.second; y >= row_range.first; --y) {  // Start from second to last to allow the bottom tile to move
            if (!slots.count({ x, y })) { continue; } // Skip if the slot isn't there (has been removed)

            if (!slots[{ x, y}]->isEmpty()) {
                int currentY = y;

                // Move the tile as far down as possible
                while (currentY < row_range.second && slots.count({ x, currentY + 1 }) && slots[{ x, currentY + 1 }]->isEmpty()) {
                    slots[{ x, currentY}]->tile->changeSlot(slots[{ x, currentY}].get(), slots[{ x, currentY + 1 }].get());
                    currentY++; // Move the tile one step down
                    std::cout << "tile moved!\n";
                }

                // Check for merging tiles if the bottom tile is not empty and has the same value
                if (currentY < row_range.second && slots.count({ x, currentY + 1 }) && !slots[{ x, currentY + 1 }]->isEmpty() &&
                    slots[{ x, currentY + 1 }]->tile->getValue() == slots[{ x, currentY}]->tile->getValue()) {
                    // Merge tiles and move one step down
                    slots[{ x, currentY}]->tile->mergeIntoSlot(slots[{ x, currentY + 1 }].get());

                    std::cout << "tiles merged!\n";
                }
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
