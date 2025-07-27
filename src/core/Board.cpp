#include "core/Board.h"
#include "core/utils/Coord.h"
#include "core/Slot.h"
#include "effects/ShopEffect.h"
#include "core/Turn.h"
#include "rendering/RenderSystem.h"


Board::Board(RenderSystem& renderer, Turn* turn, bool doInitialSetup) :
    renderer(renderer),
    board(renderer.getTextureManager().get("board")),
    turn(turn)
{
    //only for the first board
    if (doInitialSetup) {
        fixVisualAssets();
        firstBoardSetUp();
    }
}

Board Board::cloneFrom(const Board& other, Turn* turn) {
    Board b(other.renderer, turn, false);
    b.copyStateFrom(other);
    return b;
}

void Board::copyStateFrom(const Board& other) {
    this->clear();

    board = other.board;
    col_range = other.col_range;
    row_range = other.row_range;
    rng = other.rng;

    //restore flags at the end of the round
    isResolvingMovementFlag = false;
    moveIsPermittedFlag = false;

    slots.clear();
    for (const auto& [coord, slotPtr] : other.slots) {
        slots[coord] = std::make_unique<Slot>(*slotPtr, this);
    }

    for (const auto& [coord, slotPtr] : other.slots) {
        if (!slotPtr->isEmpty()) {
            Slot* mySlot = slots[coord].get();
            const Tile* originalTile = slotPtr->tile.get();
            mySlot->setTile(std::make_unique<Tile>(renderer, mySlot, originalTile->getValue()));
        }
    }
}



void Board::fixVisualAssets() {

    //asset resizing
    renderer.resizeSprite("board", board);
    //asset origin setting

    board.setOrigin(board.getLocalBounds().getCenter());

    //asset positioning
    auto windowSize = renderer.getWindowSize();

    board.setPosition({ float(windowSize.x) / 2, float(windowSize.y) / 2 }); //  up shift (-) 

    std::cout << "Board visual assets: ready" << std::endl;
}



void Board::firstBoardSetUp() {

    //grid
    for (int i = -0; i <= 3; ++i) {
        for (int j = 0; j <= 3; ++j) {
            slots[{i, j}] = std::make_unique<Slot>(i, j, this, renderer);
        }
    }
    //shop initial slot
    slots[{-1, 2}] = std::make_unique<Slot>(-1, 2, this, renderer);

    //shop effect add->tile and fields (to be created a specific function)
    slots[{-1, 2}]->addEffect(std::make_unique<ShopEffect>());
    slots[{-1, 2}]->canTileStepIn = 0;
    slots[{-1, 2}]->canTileStepOut = 0;
    slots[{-1, 2}]->setTile(std::make_unique<Tile>(renderer, slots[{-1, 2}].get(), 2));

    //boss

    /*
    slots[{3, 3}]->getSlotSprite().setTexture(renderer.getTextureManager().get("monstro"));
    slots[{3, 3}]->getSlotSprite().setTextureRect(sf::IntRect({ 0,0 }, { 42, 42 }));
    slots[{3, 3}]->getSlotSprite().setOrigin({ 42 / 2 , 42 / 2 });
    renderer.resizeSprite("monstro" , slots[{3, 3}]->getSlotSprite());
    */

    //hole
    
    //slots.erase({ 2, 2 });
    
    //first tile
    spawnTileInRandomEmptySlot();
    
}


void Board::render(RenderSystem& renderer) {

    //renderer.draw(board);

    for (const auto& [coord, slotPtr] : slots) {

        slotPtr->render(renderer);

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
        randomSlot->setTile(std::make_unique<Tile>(renderer,randomSlot,tileValue));

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

        //this should all be part of a function that computes and groups conditions and combinations of effect
        //locked, passives, effects...etc

        if (!neighbor->isEmpty() and                                //the neighbor slot must be empty
            neighbor->tile->getValue() == tile->getValue() and      //it must have the same value
            slots[current]->canTileStepOut)                         //the current slot must not be locked
        {

            tile->mergeIntoSlot(neighbor);

            moveIsPermittedFlag = true;
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

//----test

void Board::handleClick(sf::Vector2f worldPos) {


    return;

    //this function has to be thought again as after the render refactor the slot's sprite is not owned.(it actually is, luckily)

    /*
    for (const auto& [coord, slotPtr] : slots) {
        sf::FloatRect bounds = slotPtr->getSlotSprite().getGlobalBounds();

        if (bounds.contains(worldPos)) {

            // Applica filtro rosso
            slotPtr->getSlotSprite().setColor(sf::Color(255, 100, 100));  // leggermente trasparente rosso

            return;
        }
    }

    std::cout << "Clicked on empty space.\n";
    */
}
