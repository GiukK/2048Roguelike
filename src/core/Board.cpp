#include "core/Board.h"
#include "core/Slot.h"
#include "core/ShopEffect.h"
#include "core/Turn.h"

Board::Board(Turn* turn) :
    turn(turn),
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
            slots[{i, j}] = std::make_unique<Slot>(i, j, this);
        }
    }

    slots[{-1, 2}] = std::make_unique<Slot>(-1, 2, this);
    slots[{-1, 2}]->addEffect(std::make_unique<ShopEffect>());
    slots[{-1, 2}]->canTileStepIn = 0;
    slots[{-1, 2}]->canTileStepOut = 0;
    slots[{-1, 2}]->setTile(std::make_unique<Tile>(slots[{-1, 2}].get(), 2));

    slots.erase({ 2, 2 });

}

Board::Board(const Board& other, Turn* turn) :
    turn(turn),
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
        slots[coord] = std::make_unique<Slot>(*slotPtr, this);
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
        int randomIndex = getRandomInt(0, int(emptySlots.size()) - 1);
        Slot* randomSlot = emptySlots[randomIndex];

        //90% 2 - 10% 4 spawn rate
        int probValue = getRandomInt(1, 10);

        int tileValue{2};

        if (probValue <= 9) {
            tileValue = 2;
        }
        else {
            tileValue = 4;
        }

        //spawn random tile at random tile
        randomSlot->setTile(std::make_unique<Tile>(randomSlot,tileValue));

        std::cout << "Spawned tile at (row=" << randomSlot->row << ", col=" << randomSlot->col << ") with value " << tileValue << std::endl ;
        std::cout << "the value of spawn rate was " << probValue << std::endl;
    }
    else {
        std::cout << "No empty slots available to spawn a tile.\n";
    }
}


bool Board::moveLeft() {

    bool someTileHasMoved{ 0 };

    for (int y = row_range.first; y <= row_range.second; ++y) {
        // Start by checking each row from left to right
        for (int x = col_range.first; x <= col_range.second; ++x) {

                                                      // (.count returs 1 if the container has an element with key k
                                                      //   or 0 otherwise )
            if (!slots.count({ x, y })) { continue; } // Skip if the slot isn't there (has been removed since it is inside .first and .second) 
            

            if (!slots[{ x, y}]->isEmpty()) { //Skip if the slot is not empty (does not contain a tile)
                int currentX = x;

                // Move the tile as far left as possible
                while (

                    currentX > col_range.first and // this is more of a hard check to be inside the board
                    slots.count({ currentX - 1, y }) and // tiles at the edge of the board dont move
                    slots[{ currentX - 1, y }]->isEmpty() and // tiles whose adjacent slot is already full do not move
                    slots[{ currentX - 1, y }]->canTileStepIn and // if the adj slot is out-locked
                    slots[{ currentX, y }]->canTileStepOut // if the current slot is in-locked

                    //in the future it may be useful to properly inplement a structure which contains the useful fields or a helper function
                    //that computes the status of access of a slot
                    
                    )
                {

                    //if everything went right the current slot changes tile with the next
                    slots[{ currentX, y}]->tile->changeSlot(slots[{ currentX, y}].get(), slots[{ currentX - 1, y }].get());

                    currentX--; // Move the considered slot one step to the left to "chase" the tile until it hits a wall


                    std::cout << "tile moved!\n";

                    someTileHasMoved = true;
                }

                
                // Check for merging tiles if the left tile is not empty and has the same value
                if (currentX > col_range.first && slots.count({ currentX - 1, y }) && !slots[{ currentX - 1, y }]->isEmpty() &&
                    slots[{ currentX - 1, y }]->tile->getValue() == slots[{ currentX, y}]->tile->getValue()) {
                    // Merge tiles and move one step left
                    // The current tile slot becomes 
                    slots[{ currentX, y}]->tile->mergeIntoSlot(slots[{ currentX - 1, y }].get());

                    std::cout << "tiles merged!\n";

                    someTileHasMoved = true;
                }
            }
        }
    }

    //if no tile has moved then the move is considered invalid and the turn should not proceed.
    return someTileHasMoved;
}



bool Board::moveRight() {

    bool someTileHasMoved{ 0 };

    for (int y = row_range.first; y <= row_range.second; ++y) {
        // Start by checking each row from right to left
        for (int x = col_range.second; x >= col_range.first; --x) {

            if (!slots.count({ x, y })) { continue; } // Skip if the slot isn't there (has been removed)

            if (!slots[{ x, y}]->isEmpty()) {
                int currentX = x;

                // Move the tile as far right as possible
                while (

                    currentX < col_range.second and 
                    slots.count({ currentX + 1, y }) and 
                    slots[{ currentX + 1, y }]->isEmpty() and
                    slots[{ currentX + 1, y }]->canTileStepIn and
                    slots[{ currentX, y }]->canTileStepOut


                    )
                
                {
                    slots[{ currentX, y}]->tile->changeSlot(slots[{ currentX, y}].get(), slots[{ currentX + 1, y }].get());
                    currentX++; // Move the tile one step to the right
                    std::cout << "tile moved!\n";
                    someTileHasMoved = true;
                }

                // Check for merging tiles if the right tile is not empty and has the same value
                if (currentX < col_range.second && slots.count({ currentX + 1, y }) && !slots[{ currentX + 1, y }]->isEmpty() &&
                    slots[{ currentX + 1, y }]->tile->getValue() == slots[{ currentX, y}]->tile->getValue()) {
                    // Merge tiles and move one step right
                    slots[{ currentX, y}]->tile->mergeIntoSlot(slots[{ currentX + 1, y }].get());

                    std::cout << "tiles merged!\n";
                    someTileHasMoved = true;
                }
            }
        }
    }

    return someTileHasMoved;
}


bool Board::moveUp() {

    bool someTileHasMoved{ 0 };

    for (int x = col_range.first; x <= col_range.second; ++x) {
        // Start by checking each column from top to bottom
        for (int y = row_range.first + 1; y <= row_range.second; ++y) {  // Start from 1 to allow the previous row to move tiles
            if (!slots.count({ x, y })) { continue; } // Skip if the slot isn't there (has been removed)

            if (!slots[{ x, y}]->isEmpty()) {
                int currentY = y;

                // Move the tile as far up as possible
                while (

                    currentY > row_range.first and
                    slots.count({ x, currentY - 1 }) and
                    slots[{ x, currentY - 1 }]->isEmpty() and
                    slots[{ x ,currentY - 1 }]->canTileStepIn and
                    slots[{ x, currentY }]->canTileStepOut

                    
                    ) {
                    slots[{ x, currentY}]->tile->changeSlot(slots[{ x, currentY}].get(), slots[{ x, currentY - 1 }].get());
                    currentY--; // Move the tile one step up
                    std::cout << "tile moved!\n";

                    someTileHasMoved = true;
                }

                // Check for merging tiles if the top tile is not empty and has the same value
                if (currentY > row_range.first && slots.count({ x, currentY - 1 }) && !slots[{ x, currentY - 1 }]->isEmpty() &&
                    slots[{ x, currentY - 1 }]->tile->getValue() == slots[{ x, currentY}]->tile->getValue()) {
                    // Merge tiles and move one step up
                    slots[{ x, currentY}]->tile->mergeIntoSlot(slots[{ x, currentY - 1 }].get());

                    std::cout << "tiles merged!\n";

                    someTileHasMoved = true;
                }
            }
        }
    }
    return someTileHasMoved;
}


bool Board::moveDown() {

    bool someTileHasMoved{ 0 };

    for (int x = col_range.first; x <= col_range.second; ++x) {
        // Start by checking each column from bottom to top
        for (int y = row_range.second; y >= row_range.first; --y) {  // Start from second to last to allow the bottom tile to move
            if (!slots.count({ x, y })) { continue; } // Skip if the slot isn't there (has been removed)

            if (!slots[{ x, y}]->isEmpty()) {
                int currentY = y;

                // Move the tile as far down as possible
                while (
                    
                    currentY < row_range.second and
                    slots.count({ x, currentY + 1 }) and
                    slots[{ x, currentY + 1 }]->isEmpty() and
                    slots[{ x, currentY + 1 }]->canTileStepIn and
                    slots[{ x, currentY }]->canTileStepOut


                    
                    ) {
                    slots[{ x, currentY}]->tile->changeSlot(slots[{ x, currentY}].get(), slots[{ x, currentY + 1 }].get());
                    currentY++; // Move the tile one step down
                    std::cout << "tile moved!\n";
                    someTileHasMoved = true;
                }

                // Check for merging tiles if the bottom tile is not empty and has the same value
                if (currentY < row_range.second && slots.count({ x, currentY + 1 }) && !slots[{ x, currentY + 1 }]->isEmpty() &&
                    slots[{ x, currentY + 1 }]->tile->getValue() == slots[{ x, currentY}]->tile->getValue()) {
                    // Merge tiles and move one step down
                    slots[{ x, currentY}]->tile->mergeIntoSlot(slots[{ x, currentY + 1 }].get());

                    std::cout << "tiles merged!\n";
                    someTileHasMoved = true;
                }
            }
        }
    }
    return someTileHasMoved;
}


void Board::clear() {

    for (const auto& entry : slots) {
        if (!entry.second->isEmpty()) {
            entry.second->removeTile();

            std::cout << "removed tile from col=" << entry.first.x << " row=" << entry.first.y << std::endl;
        }
    }
}
