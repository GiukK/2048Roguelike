#include "core/Slot.h"
#include "core/Board.h"



Slot::Slot (int col, int row, Board* board) :
    col(col),
    row(row),
    board(board),
    slotTexture("assets/textures/slot.png", false, sf::IntRect({ 0, 0 }, { 32, 32 })),
    slot(slotTexture)
{ 

    slot.setOrigin({ 16.f,16.f });
    slot.setScale({ 4.f, 4.f });


    float shift = 300.f;
    slot.setPosition({ 128.f * col + shift, 128.f * row + shift });
}

Slot::Slot(const Slot& other, Board* newBoard) :
    board(newBoard),
    col(other.col),
    row(other.row),
    canTileStepIn(other.canTileStepIn),
    canTileStepOut(other.canTileStepOut),
    slotTexture(other.slotTexture),
    slot(slotTexture)
{
    // Copia visiva dello sprite
    slot.setOrigin(other.slot.getOrigin());
    slot.setScale(other.slot.getScale());
    slot.setPosition(other.slot.getPosition());

    // Copia profonda degli effetti
    for (const auto& effect : other.effects) {
        effects.push_back(effect->clone());
    }

    // Nota: la tile non viene copiata qui, viene gestita separatamente dalla Board
}



void Slot::render(sf::RenderWindow& window) {
    window.draw(slot);
    if (!isEmpty()) {
        tile->render(window);
    }
}

bool Slot::isEmpty() const {
    return tile == nullptr;
}

Coord Slot::getCoord() const {
    return { col, row };
}

void Slot::setTile(std::unique_ptr<Tile> newTile) {
    tile = std::move(newTile);  // Takes ownership using move semantics
}

std::unique_ptr<Tile> Slot::releaseTile() {
    auto t = std::move(tile);
    removeTile();
    return t;
}


void Slot::removeTile() {
    tile.reset();  // Properly destroys the tile
}

void Slot::addEffect(std::unique_ptr<SlotEffect> effect) {
    effects.push_back(std::move(effect));


    //right now the effect is trivially the shop but in the future this will be constructed with string concatenation
    sf::Texture loader("assets/textures/shopslot.png", false, sf::IntRect({ 0, 0 }, { 32, 32 }));

    slotTexture = loader;

    sf::Sprite spriteload(slotTexture);

    slot = spriteload;

    slot.setOrigin({ 16.f,16.f });
    slot.setScale({ 4.f, 4.f });


    float shift = 300.f;

    //in the future the position will hopefully be more insightful to read
    slot.setPosition({ 128.f * col + shift, 128.f * row + shift });
}

void Slot::triggerMergeEffects() {
    for (auto& effect : effects) {
        effect->onMerge(this);
    }
}
