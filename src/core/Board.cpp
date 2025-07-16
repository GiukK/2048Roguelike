#include "core/Board.h"
#include "core/Coord.h"
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

    spawnTileInRandomEmptySlot();

}

Board::Board(const Board& other, Turn* turn) :
    turn(turn),
    col_range(other.col_range),
    row_range(other.row_range),
    boardTexture(other.boardTexture),
    board(other.boardTexture),
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

void Board::update(float deltaTime) {

    // to be written
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

// Function to spawn a tile in a random empty slot - this will be modular in the future
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

void Board::move(Direction dir) {

    isResolvingMovementFlag = true;

    initializeMovementQueue(dir);
    while (!movementQueue.empty()) {
        resolveNextTileMove(dir);
    }

    isResolvingMovementFlag = false;

}

void Board::initializeMovementQueue(Direction dir) {
    movementQueue.clear();

    switch (dir) {
    case Direction::Left:
        for (int y = row_range.first; y <= row_range.second; ++y) {
            for (int x = col_range.first; x <= col_range.second; ++x) {
                Coord coord = { x, y };
                if (slots.count(coord) && !slots[coord]->isEmpty()) {
                    movementQueue.pushBack(slots[coord].get());
                }
            }
        }
        break;

    case Direction::Right:
        for (int y = row_range.first; y <= row_range.second; ++y) {
            for (int x = col_range.second; x >= col_range.first; --x) {
                Coord coord = { x, y };
                if (slots.count(coord) && !slots[coord]->isEmpty()) {
                    movementQueue.pushBack(slots[coord].get());
                }
            }
        }
        break;

    case Direction::Up:
        for (int x = col_range.first; x <= col_range.second; ++x) {
            for (int y = row_range.first; y <= row_range.second; ++y) {
                Coord coord = { x, y };
                if (slots.count(coord) && !slots[coord]->isEmpty()) {
                    movementQueue.pushBack(slots[coord].get());
                }
            }
        }
        break;

    case Direction::Down:
        for (int x = col_range.first; x <= col_range.second; ++x) {
            for (int y = row_range.second; y >= row_range.first; --y) {
                Coord coord = { x, y };
                if (slots.count(coord) && !slots[coord]->isEmpty()) {
                    movementQueue.pushBack(slots[coord].get());
                }
            }
        }
        break;

    default:
        break;
    }
}

void Board::resolveNextTileMove(Direction moveDirection) {
    if (movementQueue.empty()) return;

    Slot* origin = movementQueue.popFront();
    if (!origin || origin->isEmpty()) return;

    Tile* tile = origin->tile.get();
    Coord current = origin->getCoord();

    // Step 1: insegui finché possibile
    while (true) {
        Coord next = getNextCoord(current, moveDirection);

        if (!slots.count(next)) break;

        Slot* target = slots[next].get();

        if (!target->isEmpty()) break;
        if (!target->canTileStepIn || !slots[current]->canTileStepOut) break;

        // Sposta la tile di uno slot
        tile->changeSlot(slots[current].get(), target);

        //signals that the move is good
        moveIsPermittedFlag = true;

        current = next; // Avanza la posizione
    }

    // Step 2: prova merge con la tile successiva
    Coord mergeCoord = getNextCoord(current, moveDirection);

    if (slots.count(mergeCoord)) {
        Slot* neighbor = slots[mergeCoord].get();

        if (!neighbor->isEmpty() &&
            neighbor->tile->getValue() == tile->getValue()) {

            tile->mergeIntoSlot(neighbor);
        }
    }
}


Coord Board::getNextCoord(Coord from, Direction dir) {
    switch (dir) {
    case Direction::Left:  return { from.x - 1, from.y };
    case Direction::Right: return { from.x + 1, from.y };
    case Direction::Up:    return { from.x, from.y - 1 };
    case Direction::Down:  return { from.x, from.y + 1 };
    default:               return from;
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

//getters

bool Board::isResolvingMovement() const {
    return isResolvingMovementFlag;
}

bool Board::moveIsPermitted() const {
    return moveIsPermittedFlag;
}

