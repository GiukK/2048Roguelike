#include "core/Board.h"
#include "core/Boss.h"
#include "core/Turn.h"
#include "core/GameRun.h"
#include "effects/AttackContext.h"
#include "effects/EffectContext.h"
#include "effects/ShopEffect.h"
#include "rendering/RenderSystem.h"
#include "rendering/Animation.h"
#include "Debug.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <set>

Board::Board(RenderSystem& renderer, Turn* turn, bool doInitialSetup)
    : renderer(renderer),
      turn(turn)
{
    if (doInitialSetup) {
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
      slots(std::move(other.slots)),
      boss(std::move(other.boss)),
      defeatPending(other.defeatPending),
      movementQueue(std::move(other.movementQueue)),
      moveValidFlag(other.moveValidFlag),
      animationCallback(std::move(other.animationCallback)),
      hoveredTile(other.hoveredTile)
{
    // The slots were created pointing at the moved-from Board. Re-parent them
    // to this Board so Slot::board stays valid — Effect::onMergeResolving reaches
    // the owning Turn through it (e.g. ShopEffect requesting the shop).
    for (auto& [coord, slot] : slots) {
        if (slot) slot->board = this;
    }
}

void Board::copyStateFrom(const Board& other) {
    clear();

    moveValidFlag = false;
    defeatPending = false;
    hoveredTile = nullptr;

    // Boss state is board state (boss-design §7): the body — HP included —
    // clones with the turn, so undo rewinds boss damage like any world change.
    boss = other.boss ? std::make_unique<Boss>(*other.boss) : nullptr;

    slots.clear();
    for (const auto& [coord, slotPtr] : other.slots) {
        slots[coord] = std::make_unique<Slot>(*slotPtr, this);
    }

    for (const auto& [coord, slotPtr] : other.slots) {
        if (!slotPtr->isEmpty()) {
            Slot* mySlot = slots[coord].get();
            mySlot->setTile(std::make_unique<Tile>(renderer, mySlot, slotPtr->tile->getValue()));
            // Tiles are rebuilt from value; their tags decide for themselves
            // whether they survive the clone (Effect::isPersistent) — brick does,
            // frozen doesn't. No per-flag special cases here, ever again.
            for (const auto& effect : slotPtr->tile->effects) {
                if (effect->isPersistent()) {
                    mySlot->tile->addEffect(effect->clone());
                }
            }
        }
    }
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
    // A protected slot (the shop today) must never be emptied by destruction
    // effects — a broken, unmergeable shop would also keep the spawn countdown
    // frozen forever. Guarding here covers every destruction path at once.
    if (tile->slot->isProtected()) return;
    // hoveredTile is a raw back-pointer kept across frames by updateHoverState();
    // if we destroy the tile it points at, the next hover update would dereference
    // freed memory. Drop the reference before the tile dies.
    if (tile == hoveredTile) hoveredTile = nullptr;

    // Capture before the tile is freed; log it as an effect-driven destruction in
    // the acting turn (bombs, black hole — each destroyed tile emits one event).
    if (turn) turn->log().push(TurnEvent::tileDestroyed(tile->getValue(), tile->slot->getCoord()));

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
        if (!slot->isEmpty() && !slot->isProtected()) {
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
    // Protected slots (the shop) can never be wrenched away.
    if (slot->isProtected()) return false;

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
            // Protected slots are immune to area effects (mirrors destroyTile), so
            // they aren't offered as targets — keeps Bomb II's picks meaningful.
            if (it->second->isProtected()) continue;

            result.push_back(it->second->tile.get());
        }
    }
    return result;
}

void Board::swapTiles(Tile* a, Tile* b, bool allowProtected) {
    // Self-swap would releaseTile() the same slot twice and null-deref below.
    if (!a || !b || a == b || !a->slot || !b->slot) return;
    // Default policy: protected slots (the shop) keep their tile, consistent
    // with destroy / wrench / shuffle / area targeting. This is a POLICY, not
    // an invariant — moving the shop's phantom tile is a legal mechanic for
    // callers that opt in explicitly via allowProtected (the Switch item does
    // exactly this), instead of the rule being unhookable.
    if (!allowProtected && (a->slot->isProtected() || b->slot->isProtected())) return;

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

    // The boss body: per-def texture over the footprint, drawn in the board
    // layer AFTER the slots so it overlays them (boss-design §10). The visual
    // lives entirely in this render pass — the Boss entity itself is headless
    // (core boundary rule) — so the sprite is built from the texture each
    // frame; one sprite per fight is cheap and keeps the entity clone-pure.
    if (boss) {
        auto it = slots.find(boss->getFootprint().front());
        if (it != slots.end()) {
            sf::Sprite body(renderer.getTextureManager().get(boss->getTextureId()));
            body.setOrigin(body.getLocalBounds().getCenter());

            const sf::FloatRect cell = it->second->getSlotSprite().getGlobalBounds();
            const sf::FloatRect raw = body.getLocalBounds();
            const float scale = std::min(cell.size.x / raw.size.x,
                                         cell.size.y / raw.size.y);
            body.setScale({scale, scale});
            body.setPosition(it->second->getSlotSprite().getPosition());
            renderer.draw(body);
        }
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
    for (const auto& [coord, slot] : slots) {
        // Boss-occupied cells hold no tile but are NOT free: excluded here (not
        // just refused in spawnTileAt) so a fight never silently eats the
        // turn's spawn by drawing an unusable cell.
        if (slot->isEmpty() && !isBossAt(coord)) {
            empty.push_back(slot.get());
        }
    }
    if (empty.empty()) return;

    int idx = getRandomInt(0, static_cast<int>(empty.size()) - 1);
    int val = (getRandomInt(1, 10) <= 9) ? 2 : 4;
    spawnTileAt(empty[idx]->getCoord(), val);
}

Tile* Board::spawnTileAt(Coord c, int value) {
    // Unbacked values are refused like a taken cell (nullptr): effects can pass
    // arbitrary ints here, and a Tile without artwork would throw deep in the
    // texture lookup. Guarded at this primitive so every spawn path inherits it.
    if (!Tile::isValidValue(value)) {
        if (debug::Enabled) {
            std::cerr << "[spawn] refused unbacked tile value " << value << '\n';
        }
        return nullptr;
    }

    auto it = slots.find(c);
    if (it == slots.end() || !it->second->isEmpty()) return nullptr;
    // A boss-occupied cell is taken even though it holds no tile.
    if (isBossAt(c)) return nullptr;

    Slot* slot = it->second.get();
    slot->setTile(std::make_unique<Tile>(renderer, slot, value));

    // Log the spawn in the acting turn (the genesis spawn during board setup is
    // logged too, then cleared with the first endTurn — harmless and accurate).
    if (turn) turn->log().push(TurnEvent::tileSpawned(value, c));
    return slot->tile.get();
}

Slot* Board::slotAt(Coord c) const {
    auto it = slots.find(c);
    return it == slots.end() ? nullptr : it->second.get();
}

std::vector<Tile*> Board::getAllTiles() const {
    std::vector<Tile*> result;
    for (const auto& [_, slot] : slots) {
        if (!slot->isEmpty()) {
            result.push_back(slot->tile.get());
        }
    }
    return result;
}

std::vector<Effect*> Board::collectBoardEffects() const {
    // Scope-major (all tile effects, then all slot effects), coord-ordered
    // within each scope — see the declaration for the dispatch-order contract.
    std::vector<Effect*> result;
    for (const auto& [_, slot] : slots) {
        if (!slot->isEmpty()) {
            for (const auto& effect : slot->tile->effects) {
                result.push_back(effect.get());
            }
        }
    }
    for (const auto& [_, slot] : slots) {
        for (const auto& effect : slot->effects) {
            result.push_back(effect.get());
        }
    }
    return result;
}

Slot* Board::findOwnerSlot(const Effect* effect) const {
    // Pointer-identity scan over the LIVE board (never trusting the snapshot's
    // idea of where the effect was): tiles move mid-flush (a reactor may swap
    // them), so the owner is wherever the effect lives NOW.
    for (const auto& [_, slot] : slots) {
        if (!slot->isEmpty()) {
            for (const auto& e : slot->tile->effects) {
                if (e.get() == effect) return slot.get();
            }
        }
        for (const auto& e : slot->effects) {
            if (e.get() == effect) return slot.get();
        }
    }
    return nullptr;
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
    // ShopEffect::onMergeResolving (see Tile::mergeIntoSlot -> Slot::resolveMerge).
    slot->canTileStepIn = false;
    slot->canTileStepOut = false;
    slot->setTile(std::make_unique<Tile>(renderer, slot.get(), tileValue));

    slots[pos] = std::move(slot);

    // Logged like any other board change so future reactors ("when a shop
    // appears...") don't need a side channel. Fired at end-of-turn via
    // GameRun::advanceShopState, so it belongs to the finishing turn.
    if (turn) turn->log().push(TurnEvent::shopSpawned(tileValue, pos));
    return true;
}

int Board::countActiveShops() const {
    // Lifecycle code legitimately needs the concrete effect (its triggered
    // state), so it goes through the typed findEffect — unlike the protection /
    // immobility checks, which are capability queries.
    int count = 0;
    for (const auto& [_, slot] : slots) {
        if (auto* shop = slot->findEffect<ShopEffect>(); shop && !shop->isTriggered()) {
            ++count;
        }
    }
    return count;
}

int Board::removeConsumedShops() {
    std::vector<Coord> toRemove;
    for (const auto& [coord, slot] : slots) {
        if (auto* shop = slot->findEffect<ShopEffect>(); shop && shop->isTriggered()) {
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

// --- Boss occupancy & the attack interaction (boss-design §2/§3/§4) ---

bool Board::isBossAt(Coord c) const {
    return boss && boss->occupies(c);
}

Boss* Board::spawnBoss(const BossDef& def, Coord c) {
    // Spawn-primitive contract (cf. spawnTileAt): refuse instead of crash —
    // one boss at a time, anchored on an existing, tile-empty, unprotected
    // slot (a boss squatting the shop would deadlock the spawn countdown).
    if (boss) return nullptr;
    auto it = slots.find(c);
    if (it == slots.end() || !it->second->isEmpty() || it->second->isProtected()) {
        return nullptr;
    }

    boss = std::make_unique<Boss>(def, c);
    if (turn) turn->log().push(TurnEvent::bossSpawned(boss->getMaxHp(), c));
    return boss.get();
}

Boss* Board::spawnBossInRandomEmptySlot(const BossDef& def) {
    std::vector<Coord> candidates;
    for (const auto& [coord, slot] : slots) {
        if (slot->isEmpty() && !slot->isProtected()) {
            candidates.push_back(coord);
        }
    }
    if (candidates.empty()) return nullptr;

    int idx = getRandomInt(0, static_cast<int>(candidates.size()) - 1);
    return spawnBoss(def, candidates[idx]);
}

void Board::resolveBossAttack(Tile* attacker, Coord at) {
    if (defeatPending) return;  // already dead this sweep — no extra hits

    // Mirror the merge's step-out gate: a tile locked into its slot can no
    // more initiate an attack than a merge. (Immobilized attackers never get
    // here — resolveNextTileMove returned already.)
    if (!attacker->slot->canTileStepOut) return;

    const IncomingResolution incoming = boss->resolveIncoming(*attacker);
    if (incoming.kind == IncomingResolution::Kind::Block) return;  // a wall: the tile stays

    // The attack pipeline, mirroring the merge's (build → modify → apply →
    // log): in-scope modifiers act on the mutable outcome before anything
    // lands. Scope today: the attacker's tile effects, then the run cards
    // (boss-design §3); slot scopes join with the chip mounting layer.
    AttackContext attack{boss.get(), attacker, incoming.proposedDamage};
    for (auto& effect : attacker->effects) {
        effect->onAttackResolving(attack);
    }
    if (turn && turn->gameRun) {
        turn->gameRun->resolveAttackRunScope(attack);
    }

    const int damage = std::max(0, attack.damage);
    boss->takeDamage(damage);

    if (attack.consumeAttacker) {
        // Consumption is attack SEMANTICS, not a destruction effect: no
        // TileDestroyed is logged (bomb-reactive cards must not fire on an
        // attack) — BossDamaged below carries the whole interaction. Same
        // hover guard as destroyTile: the consumed tile must not dangle.
        if (attacker == hoveredTile) hoveredTile = nullptr;
        attacker->slot->removeTile();
    }
    moveValidFlag = true;

    if (turn) turn->log().push(TurnEvent::bossDamaged(damage, boss->getHp(), at));

    if (boss->getHp() <= 0) {
        if (turn) turn->log().push(TurnEvent::bossDefeated(boss->getFootprint().front()));
        defeatPending = true;
    }
}

void Board::resolveBossDefeat() {
    // DEFERRED from the sweep: BossDefeated was logged at detection time (in
    // resolveBossAttack), so the event sits in the log at the moment the kill
    // happened. The death effect and body removal run HERE, after the sweep is
    // fully drained — safe to spawn tiles, destroy slots, or do any board
    // mutation without corrupting the sweep's cellBefore map or movementQueue.
    if (turn && turn->gameRun) {
        EffectContext ctx(*turn->gameRun, *this, turn->log());
        boss->runDefeat(ctx);
    }
    boss.reset();
    defeatPending = false;
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

    // Identity snapshot for the per-tile movement events: which tile sat where
    // BEFORE the sweep. Pointers of tiles destroyed by merges are afterwards
    // used only as lookup KEYS (never dereferenced), and no tile is created
    // during a sweep, so no address can be reused mid-map.
    std::map<Tile*, Coord> cellBefore;
    for (const auto& [coord, slot] : slots) {
        if (!slot->isEmpty()) cellBefore[slot->tile.get()] = coord;
    }

    initializeMovementQueue(dir);
    while (!movementQueue.empty()) {
        resolveNextTileMove(dir);
    }

    // One TileSlid per surviving tile whose CELL changed — "a tile moved",
    // regardless of distance (Red Light's counting unit). The mover of a merge
    // counts (its cell changed); the stationary merge target was destroyed and
    // is naturally absent. Emitted after the sweep, in coord order, so the
    // merge events are already in the log.
    if (turn) {
        for (Tile* t : getAllTiles()) {
            auto it = cellBefore.find(t);
            if (it != cellBefore.end() && it->second != t->slot->getCoord()) {
                turn->log().push(TurnEvent::tileSlid(t->getValue(), t->slot->getCoord()));
            }
        }
    }

    // Boss death consequences run AFTER the TileSlid emission: onDefeat may
    // spawn tiles, and the allocator can reuse a merge-victim's address —
    // cellBefore would match the new tile to the dead tile's old position,
    // producing a phantom TileSlid. Safe here: cellBefore is consumed.
    if (defeatPending) {
        resolveBossDefeat();
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

    // An immobilized tile (brick, frozen — any effect with immobilizesOwner)
    // neither slides nor initiates a merge. It can still be a merge *target* —
    // when another tile sweeps into it, mergeIntoSlot destroys this tile and its
    // tags go with it. See effects/TileTags.h and the "brick" item.
    if (tile->isImmobilized()) return;

    while (true) {
        Coord next = getNextCoord(current, dir);
        if (!slots.count(next)) break;
        // A boss-occupied cell holds no tile but is not free to slide into.
        if (isBossAt(next)) break;

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

    // The FOURTH answer (boss-design §3): a boss-occupied next cell resolves
    // through the occupant — Block (a wall) or Hit (tile consumed, boss
    // damaged) — never through the merge rules below.
    if (isBossAt(mergeCoord)) {
        resolveBossAttack(tile, mergeCoord);
        return;
    }

    Slot* neighbor = slots[mergeCoord].get();
    if (neighbor->isEmpty()) return;
    if (neighbor->tile->getValue() != tile->getValue()) return;
    // Value cap: two MaxValue tiles stay side by side instead of producing a
    // value with no artwork (the texture lookup would throw). The shop strategy
    // clamps to MaxValue/2 for the same reason; this covers regular play.
    if (tile->getValue() >= Tile::MaxValue) return;
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

bool Board::hasLegalMove() const {
    // Mirror of resolveNextTileMove's decision logic, side-effect-free: a tile
    // has a legal move if it can SLIDE one step (empty target slot, step-in and
    // step-out allowed) or MERGE into an equal-valued neighbor below the value
    // cap. One step suffices for the existence check — sliding is stepwise, so
    // any longer slide implies a first step.
    //
    // A board with NO tiles also reports false, and that is correct, not an
    // artifact: a move with nothing to slide is refused by the turn machinery
    // (moveValidFlag stays false), so an emptied board never reaches the
    // BoardResolution spawn — a genuine softlock, not a fresh start.
    static constexpr std::array<Direction, 4> dirs = {
        Direction::Left, Direction::Right, Direction::Up, Direction::Down};

    for (const auto& [coord, slot] : slots) {
        if (slot->isEmpty()) continue;

        const Tile* tile = slot->tile.get();
        // Immobilized tiles (brick, frozen) neither slide nor initiate merges.
        // They can still be merge TARGETS — covered when the mover is iterated.
        if (tile->isImmobilized()) continue;
        // Same for a locked-in tile (the shop's phantom): step-out gates both
        // the slide and the merge in the sweep, so it gates both here.
        if (!slot->canTileStepOut) continue;

        for (Direction dir : dirs) {
            const Coord next = getNextCoord(coord, dir);
            auto it = slots.find(next);
            if (it == slots.end()) continue;  // no slot: board edge or a hole

            // The fourth answer (§3/§8): attacking the boss IS a legal move —
            // exactly when the occupant answers Hit. A Block is a wall and
            // unlocks nothing. Checked before the empty test: a boss cell
            // holds no tile but is not slidable-into.
            if (isBossAt(next)) {
                if (boss->resolveIncoming(*tile).kind ==
                    IncomingResolution::Kind::Hit) {
                    return true;
                }
                continue;
            }

            const Slot* target = it->second.get();
            if (target->isEmpty()) {
                if (target->canTileStepIn) return true;  // a slide
            } else if (tile->getValue() == target->tile->getValue() &&
                       tile->getValue() < Tile::MaxValue) {
                // A merge. The sweep doesn't check the target's canTileStepIn
                // for merges (that's how feeding the shop's phantom works), so
                // neither does the predicate — merging into a shop counts.
                return true;
            }
        }
    }
    return false;
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

sf::FloatRect Board::getContentBounds() {
    if (slots.empty()) return sf::FloatRect({0.f, 0.f}, {0.f, 0.f});

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
    return sf::FloatRect({minX, minY}, {maxX - minX, maxY - minY});
}

sf::Vector2f Board::getContentCenter() {
    sf::FloatRect b = getContentBounds();
    return {b.position.x + b.size.x / 2.f, b.position.y + b.size.y / 2.f};
}

void Board::setAnimationCallback(AnimationCallback callback) {
    animationCallback = std::move(callback);
}

int Board::getRandomInt(int min, int max) {
    return turn->gameRun->getRandomInt(min, max);
}
