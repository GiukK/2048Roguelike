#include "core/Board.h"
#include "core/Turn.h"
#include "core/GameRun.h"
#include "effects/ShopEffect.h"
#include "rendering/RenderSystem.h"
#include "rendering/Animation.h"

Board::Board(RenderSystem& renderer, Turn* turn, bool doInitialSetup)
    : renderer(renderer),
      boardSprite(renderer.getTextureManager().get("board")),
      turn(turn)
{
    if (doInitialSetup) {
        initVisuals();
        setupInitialBoard();
    }
}

Board Board::cloneFrom(const Board& other, Turn* turn) {
    Board b(other.renderer, turn, false);
    b.copyStateFrom(other);
    return b;
}

Board::Board(Board&& other) noexcept
    : turn(other.turn),
      renderer(other.renderer),
      boardSprite(std::move(other.boardSprite)),
      slots(std::move(other.slots)),
      movementQueue(std::move(other.movementQueue)),
      colRange(other.colRange),
      rowRange(other.rowRange),
      moveValidFlag(other.moveValidFlag),
      animationCallback(std::move(other.animationCallback)),
      hoveredTile(other.hoveredTile)
{
    // The slots were created pointing at the moved-from Board. Re-parent them
    // to this Board so Slot::board stays valid — SlotEffect::onMerge reaches the
    // owning Turn through it (e.g. ShopEffect requesting the shop).
    for (auto& [coord, slot] : slots) {
        if (slot) slot->board = this;
    }
}

void Board::copyStateFrom(const Board& other) {
    clear();

    boardSprite = other.boardSprite;
    colRange = other.colRange;
    rowRange = other.rowRange;
    moveValidFlag = false;
    hoveredTile = nullptr;

    slots.clear();
    for (const auto& [coord, slotPtr] : other.slots) {
        slots[coord] = std::make_unique<Slot>(*slotPtr, this);
    }

    for (const auto& [coord, slotPtr] : other.slots) {
        if (!slotPtr->isEmpty()) {
            Slot* mySlot = slots[coord].get();
            mySlot->setTile(std::make_unique<Tile>(renderer, mySlot, slotPtr->tile->getValue()));
        }
    }
}

void Board::initVisuals() {
    renderer.resizeSprite("board", boardSprite);
    boardSprite.setOrigin(boardSprite.getLocalBounds().getCenter());
    auto ws = renderer.getWindowSize();
    boardSprite.setPosition({static_cast<float>(ws.x) / 2.f,
                             static_cast<float>(ws.y) / 2.f});
}

void Board::setupInitialBoard() {
    for (int c = 0; c <= 3; ++c) {
        for (int r = 0; r <= 3; ++r) {
            slots[{c, r}] = std::make_unique<Slot>(c, r, this, renderer);
        }
    }

    slots[{-1, 2}] = std::make_unique<Slot>(-1, 2, this, renderer);
    slots[{-1, 2}]->addEffect(std::make_unique<ShopEffect>());
    slots[{-1, 2}]->canTileStepIn = false;
    slots[{-1, 2}]->canTileStepOut = false;
    slots[{-1, 2}]->setTile(std::make_unique<Tile>(renderer, slots[{-1, 2}].get(), 2));

    spawnTileInRandomEmptySlot();
}

// --- Input & Interaction ---

void Board::handleInput(sf::Event& event) {
    if (auto* released = event.getIf<sf::Event::MouseButtonReleased>()) {
        if (released->button == sf::Mouse::Button::Left) {
            sf::Vector2f worldPos = renderer.getWindow().mapPixelToCoords(
                {released->position.x, released->position.y});
            handleClick(worldPos);
        }
    }
}

void Board::updateHoverState() {
    sf::Vector2i mousePixel = sf::Mouse::getPosition(renderer.getWindow());
    sf::Vector2f mousePos = renderer.getWindow().mapPixelToCoords(mousePixel);
    bool mouseDown = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);

    Tile* hit = findTileAt(mousePos);

    if (hoveredTile && hoveredTile != hit) {
        hoveredTile->setVisual(Tile::Visual::Idle);
    }

    hoveredTile = hit;

    if (hoveredTile) {
        hoveredTile->setVisual(mouseDown ? Tile::Visual::Pressed : Tile::Visual::Hovered);
    }
}

void Board::handleClick(sf::Vector2f worldPos) {
    Tile* hit = findTileAt(worldPos);
    if (hit) {
        hit->setSelected(!hit->isSelected());
    }
}

Tile* Board::findTileAt(sf::Vector2f pos) const {
    for (const auto& [_, slot] : slots) {
        if (!slot->isEmpty() && slot->tile->getBounds().contains(pos)) {
            return slot->tile.get();
        }
    }
    return nullptr;
}

std::vector<Tile*> Board::getSelectedTiles() const {
    std::vector<Tile*> result;
    for (const auto& [_, slot] : slots) {
        if (!slot->isEmpty() && slot->tile->isSelected()) {
            result.push_back(slot->tile.get());
        }
    }
    return result;
}

void Board::destroyTile(Tile* tile) {
    if (!tile || !tile->slot) return;
    tile->slot->removeTile();
}

void Board::clearSelection() {
    for (auto& [_, slot] : slots) {
        if (!slot->isEmpty()) {
            slot->tile->setSelected(false);
        }
    }
}

// --- Render & Update ---

void Board::render(RenderSystem& renderer) {
    for (const auto& [_, slot] : slots) {
        slot->render(renderer);
    }
}

void Board::update(float deltaTime) {
    updateHoverState();

    for (auto& [_, slot] : slots) {
        slot->update(deltaTime);
    }
}

// --- Spawning ---

void Board::spawnTileInRandomEmptySlot() {
    std::vector<Slot*> empty;
    for (const auto& [_, slot] : slots) {
        if (slot->isEmpty()) {
            empty.push_back(slot.get());
        }
    }
    if (empty.empty()) return;

    int idx = getRandomInt(0, static_cast<int>(empty.size()) - 1);
    int val = (getRandomInt(1, 10) <= 9) ? 2 : 4;
    empty[idx]->setTile(std::make_unique<Tile>(renderer, empty[idx], val));
}

// --- Movement ---

void Board::move(Direction dir) {
    initializeMovementQueue(dir);
    while (!movementQueue.empty()) {
        resolveNextTileMove(dir);
    }
}

void Board::initializeMovementQueue(Direction dir) {
    movementQueue.clear();

    auto tryAdd = [&](int x, int y) {
        Coord coord{x, y};
        if (slots.count(coord) && !slots[coord]->isEmpty()) {
            movementQueue.pushBack(slots[coord].get());
        }
    };

    switch (dir) {
    case Direction::Left:
        for (int y = rowRange.first; y <= rowRange.second; ++y)
            for (int x = colRange.first; x <= colRange.second; ++x)
                tryAdd(x, y);
        break;
    case Direction::Right:
        for (int y = rowRange.first; y <= rowRange.second; ++y)
            for (int x = colRange.second; x >= colRange.first; --x)
                tryAdd(x, y);
        break;
    case Direction::Up:
        for (int x = colRange.first; x <= colRange.second; ++x)
            for (int y = rowRange.first; y <= rowRange.second; ++y)
                tryAdd(x, y);
        break;
    case Direction::Down:
        for (int x = colRange.first; x <= colRange.second; ++x)
            for (int y = rowRange.second; y >= rowRange.first; --y)
                tryAdd(x, y);
        break;
    default:
        break;
    }
}

void Board::resolveNextTileMove(Direction dir) {
    Slot* origin = movementQueue.popFront();
    if (!origin || origin->isEmpty()) return;

    Tile* tile = origin->tile.get();
    Coord current = origin->getCoord();

    while (true) {
        Coord next = getNextCoord(current, dir);
        if (!slots.count(next)) break;

        Slot* target = slots[next].get();
        if (!target->isEmpty()) break;
        if (!target->canTileStepIn || !slots[current]->canTileStepOut) break;

        tile->changeSlot(slots[current].get(), target);
        moveValidFlag = true;
        current = next;
    }

    if (tile->mergedThisSweep) return;

    Coord mergeCoord = getNextCoord(current, dir);
    if (!slots.count(mergeCoord)) return;

    Slot* neighbor = slots[mergeCoord].get();
    if (neighbor->isEmpty()) return;
    if (neighbor->tile->getValue() != tile->getValue()) return;
    if (!slots[current]->canTileStepOut) return;
    if (tile->mergedThisSweep || neighbor->tile->mergedThisSweep) return;

    tile->mergeIntoSlot(neighbor);

    if (animationCallback) {
        auto anim = std::make_unique<Animation>(
            renderer, "merge_animation", sf::Vector2i(23, 23), 5,
            neighbor->getSlotSprite().getPosition());
        anim->looping = false;
        animationCallback(std::move(anim));
    }

    moveValidFlag = true;
}

Coord Board::getNextCoord(Coord from, Direction dir) {
    switch (dir) {
    case Direction::Left:  return {from.x - 1, from.y};
    case Direction::Right: return {from.x + 1, from.y};
    case Direction::Up:    return {from.x, from.y - 1};
    case Direction::Down:  return {from.x, from.y + 1};
    default:               return from;
    }
}

void Board::clear() {
    for (const auto& [_, slot] : slots) {
        if (!slot->isEmpty()) {
            slot->removeTile();
        }
    }
}

bool Board::allAnimationsFinished() const {
    for (const auto& [_, slot] : slots) {
        if (!slot->isEmpty() && slot->tile->isAnimating()) {
            return false;
        }
    }
    return true;
}

bool Board::moveWasValid() const {
    return moveValidFlag;
}

void Board::setAnimationCallback(AnimationCallback callback) {
    animationCallback = std::move(callback);
}

int Board::getRandomInt(int min, int max) {
    return turn->gameRun->getRandomInt(min, max);
}
