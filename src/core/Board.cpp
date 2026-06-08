#include "core/Board.h"
#include "core/Turn.h"
#include "core/GameRun.h"
#include "effects/ShopEffect.h"
#include "rendering/RenderSystem.h"
#include "rendering/Animation.h"

#include <algorithm>
#include <array>
#include <set>

namespace {
// Returns the ShopEffect carried by a slot, or nullptr if the slot is not a
// shop. A slot is treated as a shop purely by virtue of holding a ShopEffect,
// which keeps the shop concept decoupled from Slot/Board internals.
ShopEffect* shopEffectOf(const Slot& slot) {
    for (const auto& effect : slot.effects) {
        if (auto* shop = dynamic_cast<ShopEffect*>(effect.get())) {
            return shop;
        }
    }
    return nullptr;
}
} // namespace

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
            // Tiles are rebuilt from value only, so per-tile state that must
            // persist across turns/undo has to be copied explicitly.
            if (slotPtr->tile->isBricked()) {
                mySlot->tile->setBricked(true);
            }
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
    // Plain 2048 start: a 4x4 grid with one spawned tile. The shop is no longer
    // placed here — it is spawned later by GameRun once the turn countdown
    // elapses (see GameRun::advanceShopState / Board::spawnShop).
    for (int c = 0; c <= 3; ++c) {
        for (int r = 0; r <= 3; ++r) {
            slots[{c, r}] = std::make_unique<Slot>(c, r, this, renderer);
        }
    }

    spawnTileInRandomEmptySlot();
}

// --- Input & Interaction ---

void Board::handleInput(sf::Event& event) {
    if (auto* released = event.getIf<sf::Event::MouseButtonReleased>()) {
        // Tile selection is on the RIGHT button: the left button is reserved for
        // camera pan, so the two never fight (no click-vs-drag disambiguation).
        if (released->button == sf::Mouse::Button::Right) {
            // Map through the board camera so picking follows zoom/pan.
            sf::Vector2f worldPos = renderer.mapPixelToBoard(
                {released->position.x, released->position.y});
            handleClick(worldPos);
        }
    }
}

void Board::updateHoverState() {
    sf::Vector2i mousePixel = sf::Mouse::getPosition(renderer.getWindow());
    sf::Vector2f mousePos = renderer.mapPixelToBoard(mousePixel);
    // "Pressed" feedback follows the selection button (right), so a left-drag pan
    // doesn't make the hovered tile flash pressed.
    bool mouseDown = sf::Mouse::isButtonPressed(sf::Mouse::Button::Right);

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
    // The shop is a protected objective: destruction effects (the bombs) must
    // never empty a shop slot, which would leave a broken, unmergeable shop that
    // also keeps the spawn countdown frozen forever. Guarding here covers every
    // destruction path at once.
    if (shopEffectOf(*tile->slot)) return;
    // hoveredTile is a raw back-pointer kept across frames by updateHoverState();
    // if we destroy the tile it points at, the next hover update would dereference
    // freed memory. Drop the reference before the tile dies.
    if (tile == hoveredTile) hoveredTile = nullptr;
    tile->slot->removeTile();
}

void Board::destroyAllTiles() {
    // destroyTile skips shop tiles and clears the hover back-pointer, so the
    // shop survives and nothing dangles. Erasing only the tile (not the slot)
    // leaves the map structure intact, so iterating it here is safe.
    for (auto& [_, slot] : slots) {
        if (!slot->isEmpty()) {
            destroyTile(slot->tile.get());
        }
    }
}

int Board::shuffleTiles() {
    // Release every non-shop tile, then reassign the tiles to the SAME set of
    // cells in a uniformly random order. Moving the Tile objects (not just their
    // values) preserves each tile's full state through the shuffle.
    std::vector<Slot*> cells;
    std::vector<std::unique_ptr<Tile>> tiles;
    for (auto& [_, slot] : slots) {
        if (!slot->isEmpty() && !shopEffectOf(*slot)) {
            cells.push_back(slot.get());
            tiles.push_back(slot->releaseTile());
        }
    }

    // Fisher-Yates: every permutation equally likely (statistically uniform).
    for (int i = static_cast<int>(tiles.size()) - 1; i > 0; --i) {
        int j = getRandomInt(0, i);
        std::swap(tiles[i], tiles[j]);
    }

    for (size_t i = 0; i < cells.size(); ++i) {
        Slot* cell = cells[i];
        Tile* moved = tiles[i].get();
        moved->slot = cell;                       // re-parent to its new cell
        cell->setTile(std::move(tiles[i]));
        moved->animateTo(cell->getSlotSprite().getPosition());  // slide to new home
    }

    return static_cast<int>(cells.size());
}

bool Board::addRandomSlot() {
    auto candidates = getEmptyAdjacentCells();
    if (candidates.empty()) return false;

    int idx = getRandomInt(0, static_cast<int>(candidates.size()) - 1);
    Coord pos = candidates[idx];

    // A plain base slot: empty, unrestricted (canTileStepIn/Out default true),
    // no effects. It joins the playfield like any other cell.
    slots[pos] = std::make_unique<Slot>(pos.x, pos.y, this, renderer);
    return true;
}

bool Board::removeSlotUnder(Tile* tile) {
    if (!tile || !tile->slot) return false;

    Slot* slot = tile->slot;
    // The shop is a protected objective — never wrench it away.
    if (shopEffectOf(*slot)) return false;

    // Erasing the slot destroys its tile too; drop the hover back-pointer first
    // so the next hover update cannot dereference freed memory (cf. destroyTile).
    if (tile == hoveredTile) hoveredTile = nullptr;
    slots.erase(slot->getCoord());
    return true;
}

std::vector<Tile*> Board::getTilesInRadius(const Tile* center, int radius,
                                           bool includeCenter) const {
    std::vector<Tile*> result;
    if (!center || !center->slot) return result;

    Coord c = center->slot->getCoord();
    for (int dx = -radius; dx <= radius; ++dx) {
        for (int dy = -radius; dy <= radius; ++dy) {
            if (!includeCenter && dx == 0 && dy == 0) continue;

            auto it = slots.find({c.x + dx, c.y + dy});
            if (it == slots.end() || it->second->isEmpty()) continue;
            // Shops are immune to area effects (mirrors destroyTile), so they are
            // not offered as targets — keeps Bomb II's "up to 2" picks meaningful.
            if (shopEffectOf(*it->second)) continue;

            result.push_back(it->second->tile.get());
        }
    }
    return result;
}

void Board::swapTiles(Tile* a, Tile* b) {
    if (!a || !b || !a->slot || !b->slot) return;

    Slot* slotA = a->slot;
    Slot* slotB = b->slot;

    auto tileA = slotA->releaseTile();
    auto tileB = slotB->releaseTile();

    tileA->slot = slotB;
    tileB->slot = slotA;

    slotA->setTile(std::move(tileB));
    slotB->setTile(std::move(tileA));

    // Future: insert swap animation/effect here
    slotA->tile->animateTo(slotA->getSlotSprite().getPosition());
    slotB->tile->animateTo(slotB->getSlotSprite().getPosition());
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

// --- Shop mechanics ---

int Board::getMaxTileValue() const {
    int maxValue = 0;
    for (const auto& [_, slot] : slots) {
        if (!slot->isEmpty()) {
            maxValue = std::max(maxValue, slot->tile->getValue());
        }
    }
    return maxValue;
}

std::vector<Coord> Board::getEmptyAdjacentCells() const {
    // For every existing slot, every orthogonal neighbour that is NOT already a
    // slot is an admissible expansion cell. A std::set both de-duplicates cells
    // shared by multiple slots (e.g. a hole bordered by four slots counts once)
    // and yields a deterministic ordering so the uniform pick below is
    // reproducible for a given RNG state.
    static constexpr std::array<Coord, 4> offsets = {{
        {-1, 0}, {1, 0}, {0, -1}, {0, 1}
    }};

    std::set<Coord> candidates;
    for (const auto& [coord, _] : slots) {
        for (const auto& off : offsets) {
            Coord neighbour{coord.x + off.x, coord.y + off.y};
            if (slots.count(neighbour) == 0) {
                candidates.insert(neighbour);
            }
        }
    }
    return {candidates.begin(), candidates.end()};
}

bool Board::spawnShop(int tileValue) {
    auto candidates = getEmptyAdjacentCells();
    if (candidates.empty()) return false;

    int idx = getRandomInt(0, static_cast<int>(candidates.size()) - 1);
    Coord pos = candidates[idx];

    auto slot = std::make_unique<Slot>(pos.x, pos.y, this, renderer);
    slot->addEffect(std::make_unique<ShopEffect>());
    // Locked: tiles can neither slide into nor out of a shop. The only legal
    // interaction is merging a matching tile into the phantom tile, which fires
    // ShopEffect::onMerge (see Tile::mergeIntoSlot -> Slot::triggerMergeEffects).
    slot->canTileStepIn = false;
    slot->canTileStepOut = false;
    slot->setTile(std::make_unique<Tile>(renderer, slot.get(), tileValue));

    slots[pos] = std::move(slot);
    return true;
}

int Board::countActiveShops() const {
    int count = 0;
    for (const auto& [_, slot] : slots) {
        if (auto* shop = shopEffectOf(*slot); shop && !shop->isTriggered()) {
            ++count;
        }
    }
    return count;
}

int Board::removeConsumedShops() {
    std::vector<Coord> toRemove;
    for (const auto& [coord, slot] : slots) {
        if (auto* shop = shopEffectOf(*slot); shop && shop->isTriggered()) {
            toRemove.push_back(coord);
        }
    }

    for (const Coord& coord : toRemove) {
        // The slot (and its merged tile) is about to be destroyed; drop the
        // dangling-prone hover back-pointer if it targets this tile, mirroring
        // the guard in destroyTile().
        Slot* slot = slots[coord].get();
        if (!slot->isEmpty() && slot->tile.get() == hoveredTile) {
            hoveredTile = nullptr;
        }
        slots.erase(coord);
    }
    return static_cast<int>(toRemove.size());
}

// --- Movement ---

void Board::move(Direction dir) {
    // A move can merge tiles, which destroys the stationary tile of each pair.
    // hoveredTile is a raw pointer held across frames; if it points at a tile that
    // gets merged away, the next updateHoverState() would use-after-free it. Reset
    // the hover here (while every tile is still alive) and let it be re-detected
    // next frame. This is what made SWITCH crash: selecting tiles leaves the mouse
    // over the board, so hoveredTile is live right when a merge frees that tile.
    if (hoveredTile) {
        hoveredTile->setVisual(Tile::Visual::Idle);
        hoveredTile = nullptr;
    }

    initializeMovementQueue(dir);
    while (!movementQueue.empty()) {
        resolveNextTileMove(dir);
    }
}

void Board::initializeMovementQueue(Direction dir) {
    movementQueue.clear();
    if (slots.empty()) return;

    // Sweep bounds are derived from the live slot set rather than a fixed range,
    // so a shop spawned on *any* border (col -1, col 4, row -1, row 4, ...) is
    // covered automatically. This also keeps the board future-proof if abilities
    // ever grow or reshape the playfield.
    int minCol = slots.begin()->first.x, maxCol = minCol;
    int minRow = slots.begin()->first.y, maxRow = minRow;
    for (const auto& [coord, _] : slots) {
        minCol = std::min(minCol, coord.x);
        maxCol = std::max(maxCol, coord.x);
        minRow = std::min(minRow, coord.y);
        maxRow = std::max(maxRow, coord.y);
    }

    auto tryAdd = [&](int x, int y) {
        Coord coord{x, y};
        if (slots.count(coord) && !slots[coord]->isEmpty()) {
            movementQueue.pushBack(slots[coord].get());
        }
    };

    switch (dir) {
    case Direction::Left:
        for (int y = minRow; y <= maxRow; ++y)
            for (int x = minCol; x <= maxCol; ++x)
                tryAdd(x, y);
        break;
    case Direction::Right:
        for (int y = minRow; y <= maxRow; ++y)
            for (int x = maxCol; x >= minCol; --x)
                tryAdd(x, y);
        break;
    case Direction::Up:
        for (int x = minCol; x <= maxCol; ++x)
            for (int y = minRow; y <= maxRow; ++y)
                tryAdd(x, y);
        break;
    case Direction::Down:
        for (int x = minCol; x <= maxCol; ++x)
            for (int y = maxRow; y >= minRow; --y)
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

    // A bricked tile is immovable until a merge clears it: it neither slides nor
    // initiates a merge. It can still be a merge *target* — when another tile
    // sweeps into it, mergeIntoSlot destroys this tile and the brick goes with
    // it. The same immobility applies to a tile frozen for the rest of this turn
    // by such a merge, so it cannot move again before the turn ends.
    // See Tile::isBricked / Tile::isFrozenThisTurn / the "brick" item.
    if (tile->isBricked() || tile->isFrozenThisTurn()) return;

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
    // All tiles are about to be destroyed; drop the dangling-prone hover pointer.
    hoveredTile = nullptr;
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

sf::Vector2f Board::getContentCenter() {
    if (slots.empty()) return {0.f, 0.f};

    bool first = true;
    float minX = 0.f, maxX = 0.f, minY = 0.f, maxY = 0.f;
    for (auto& [_, slot] : slots) {
        sf::Vector2f p = slot->getSlotSprite().getPosition();
        if (first) {
            minX = maxX = p.x;
            minY = maxY = p.y;
            first = false;
        } else {
            minX = std::min(minX, p.x);
            maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y);
            maxY = std::max(maxY, p.y);
        }
    }
    return {(minX + maxX) / 2.f, (minY + maxY) / 2.f};
}

void Board::setAnimationCallback(AnimationCallback callback) {
    animationCallback = std::move(callback);
}

int Board::getRandomInt(int min, int max) {
    return turn->gameRun->getRandomInt(min, max);
}
