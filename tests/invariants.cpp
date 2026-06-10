// Invariant tests for the delicate core semantics: clone/undo, merge emission,
// transient-vs-persistent tile state, shop protection, RNG determinism. These
// are the regressions the effect-engine slices (docs/effect-engine-design.md
// §11) must not introduce. Plain asserts on purpose — no framework dependency;
// exit code 0 = all green. The core is still coupled to RenderSystem, so the
// driver opens a hidden window and loads the real assets (run from the exe dir,
// e.g. via `ctest -C Debug` which sets the working directory).

#include "core/Board.h"
#include "core/GameRun.h"
#include "core/Slot.h"
#include "core/Tile.h"
#include "core/Turn.h"
#include "rendering/RenderSystem.h"

#include <SFML/Graphics.hpp>
#include <iostream>
#include <memory>

namespace {

int failures = 0;
int checks = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        ++checks;                                                            \
        if (!(cond)) {                                                       \
            ++failures;                                                      \
            std::cout << "FAIL " << __FILE__ << ":" << __LINE__ << "  "      \
                      << #cond << '\n';                                      \
        }                                                                    \
    } while (false)

Tile* tileAt(Board& board, Coord c) {
    for (Tile* t : board.getAllTiles()) {
        if (t->slot && t->slot->getCoord() == c) return t;
    }
    return nullptr;
}

} // namespace

int main() {
    sf::RenderWindow window(sf::VideoMode({640u, 360u}), "invariants", sf::Style::None);
    window.setVisible(false);
    RenderSystem renderer(window);
    renderer.initialize(window.getSize());

    auto makeRun = [&renderer](unsigned int seed) {
        return std::make_unique<GameRun>(
            renderer,
            [](std::unique_ptr<Animation>) {},  // no animations in tests
            [](GameRun*) {},                    // no shop UI in tests
            seed);
    };

    // --- RNG determinism: same seed => same sequence -------------------------
    {
        auto r1 = makeRun(123);
        auto r2 = makeRun(123);
        CHECK(r1->getRunSeed() == 123u);
        bool same = true;
        for (int i = 0; i < 50; ++i) {
            same = same && (r1->getRandomInt(0, 1 << 20) == r2->getRandomInt(0, 1 << 20));
        }
        CHECK(same);
    }

    // --- Merge: exactly one event, correct values, sweep is a fixpoint -------
    {
        auto run = makeRun(1);
        Turn turn(renderer, run.get());
        turn.board.clear();
        CHECK(turn.board.spawnTileAt({0, 0}, 2) != nullptr);
        CHECK(turn.board.spawnTileAt({1, 0}, 2) != nullptr);
        CHECK(turn.board.spawnTileAt({2, 0}, 4) != nullptr);
        turn.log().clear();

        turn.board.move(Direction::Left);
        CHECK(turn.log().mergeCount() == 1);
        CHECK(turn.log().highestMergeValue() == 4);
        CHECK(turn.board.getAllTiles().size() == 2);  // [4][4]
        Tile* merged = tileAt(turn.board, {0, 0});
        CHECK(merged && merged->getValue() == 4);

        // Re-running the sweep (the old per-frame behavior) must change NOTHING:
        // the two 4s may not merge because mergedThisSweep persists for the turn.
        // This is the substrate the one-shot move contract relies on.
        turn.board.move(Direction::Left);
        CHECK(turn.log().mergeCount() == 1);
        CHECK(turn.board.getAllTiles().size() == 2);
    }

    // --- Value cap: two MaxValue tiles never merge (no artwork above it) -----
    {
        auto run = makeRun(8);
        Turn turn(renderer, run.get());
        turn.board.clear();
        CHECK(turn.board.spawnTileAt({0, 0}, Tile::MaxValue) != nullptr);
        CHECK(turn.board.spawnTileAt({1, 0}, Tile::MaxValue) != nullptr);
        turn.log().clear();

        turn.board.move(Direction::Left);
        CHECK(turn.log().mergeCount() == 0);          // refused, no crash
        CHECK(turn.board.getAllTiles().size() == 2);  // both still on the board
    }

    // --- Brick is persistent across clones; frozen is transient --------------
    {
        auto run = makeRun(2);
        Turn turn(renderer, run.get());
        turn.board.clear();
        Tile* brick = turn.board.spawnTileAt({0, 0}, 2);
        CHECK(turn.board.spawnTileAt({1, 0}, 2) != nullptr);
        CHECK(brick != nullptr);
        brick->setBricked(true);

        Board copy = Board::cloneFrom(turn.board, &turn);
        Tile* copiedBrick = tileAt(copy, {0, 0});
        CHECK(copiedBrick && copiedBrick->isBricked());

        // Merging INTO the brick: the brick breaks during this turn, the result
        // stays frozen for the rest of it...
        turn.log().clear();
        turn.board.move(Direction::Left);
        Tile* result = tileAt(turn.board, {0, 0});
        CHECK(turn.board.getAllTiles().size() == 1);
        CHECK(result && result->getValue() == 4);
        CHECK(result && result->isFrozenThisTurn());
        CHECK(result && !result->isBricked());
        CHECK(turn.log().mergeCount() == 1);
        bool brickBrokeFlag = false;
        for (const auto& e : turn.log().events()) {
            if (e.type == TurnEvent::Type::TileMerged) brickBrokeFlag = e.flag;
        }
        CHECK(brickBrokeFlag);  // the merge event records the brick break

        // ...the frozen result may not move for the rest of this turn...
        turn.board.move(Direction::Right);
        CHECK(tileAt(turn.board, {0, 0}) == result);

        // ...and frozen does NOT survive into the next turn's clone.
        Board next = Board::cloneFrom(turn.board, &turn);
        Tile* nextTile = tileAt(next, {0, 0});
        CHECK(nextTile && !nextTile->isFrozenThisTurn() && !nextTile->isBricked());
    }

    // --- Immobilized tiles neither slide nor initiate merges -----------------
    {
        auto run = makeRun(7);
        Turn turn(renderer, run.get());
        turn.board.clear();
        Tile* brick = turn.board.spawnTileAt({2, 0}, 2);
        CHECK(turn.board.spawnTileAt({3, 0}, 2) != nullptr);
        CHECK(brick != nullptr);
        brick->setBricked(true);
        CHECK(brick->isImmobilized());
        turn.log().clear();

        turn.board.move(Direction::Right);
        CHECK(turn.log().mergeCount() == 0);         // a brick may not initiate a merge
        CHECK(tileAt(turn.board, {2, 0}) == brick);  // and didn't slide

        brick->setBricked(false);                    // un-bricking removes the tag...
        CHECK(!brick->isBricked() && !brick->isImmobilized());
        turn.board.move(Direction::Right);
        CHECK(turn.log().mergeCount() == 1);         // ...and the merge happens now
    }

    // --- Snapshot rewind: copyStateFrom restores the turn-start board --------
    {
        auto run = makeRun(3);
        Turn turn(renderer, run.get());
        turn.board.clear();
        turn.board.spawnTileAt({0, 0}, 1024);
        turn.board.copyStateFrom(turn.boardSnapshot);

        auto restored = turn.board.getAllTiles();   // both in coord order
        auto expected = turn.boardSnapshot.getAllTiles();
        CHECK(restored.size() == expected.size());
        bool match = restored.size() == expected.size();
        for (size_t i = 0; match && i < restored.size(); ++i) {
            match = restored[i]->getValue() == expected[i]->getValue() &&
                    restored[i]->slot->getCoord() == expected[i]->slot->getCoord();
        }
        CHECK(match);
    }

    // --- goBack restores the resumed turn's shop countdown -------------------
    {
        auto run = makeRun(4);
        Turn scratch(renderer, run.get());  // a board for advanceShopState to act on
        const int c0 = run->getShopCountdown();
        run->advanceShopState(scratch.board);
        CHECK(run->getShopCountdown() == c0 - 1);
        run->newTurn(scratch.board);        // the new top turn captures c0 - 1
        run->advanceShopState(scratch.board);
        CHECK(run->getShopCountdown() == c0 - 2);
        CHECK(run->goBack());               // resume the FIRST turn...
        CHECK(run->getShopCountdown() == c0);  // ...which started at c0
        CHECK(!run->goBack());              // never below the first turn
    }

    // --- Shop slot: protected objective + triggered state clones -------------
    {
        auto run = makeRun(5);
        Turn turn(renderer, run.get());
        turn.board.clear();
        turn.log().clear();
        CHECK(turn.board.spawnShop(4));
        CHECK(turn.log().countOf(TurnEvent::Type::ShopSpawned) == 1);
        CHECK(turn.board.countActiveShops() == 1);

        Tile* shopTile = nullptr;
        for (Tile* t : turn.board.getAllTiles()) {
            if (t->slot && !t->slot->canTileStepIn) shopTile = t;
        }
        CHECK(shopTile && shopTile->getValue() == 4);

        // Destruction effects and the wrench must refuse the shop.
        const size_t before = turn.board.getAllTiles().size();
        turn.board.destroyTile(shopTile);
        CHECK(turn.board.getAllTiles().size() == before);
        CHECK(!turn.board.removeSlotUnder(shopTile));

        // Merge a matching tile into the shop from an adjacent slot: the shop
        // spawns outside the base grid, so exactly the neighbour it is attached
        // to accepts a spawn; move toward the shop from there.
        const Coord sc = shopTile->slot->getCoord();
        struct Probe { Coord off; Direction dir; };
        const Probe probes[] = {{{-1, 0}, Direction::Right},
                                {{1, 0}, Direction::Left},
                                {{0, -1}, Direction::Down},
                                {{0, 1}, Direction::Up}};
        Direction dir = Direction::None;
        Tile* mover = nullptr;
        for (const Probe& p : probes) {
            mover = turn.board.spawnTileAt({sc.x + p.off.x, sc.y + p.off.y}, 4);
            if (mover) {
                dir = p.dir;
                break;
            }
        }
        CHECK(dir != Direction::None);
        CHECK(shopTile->slot->isProtected());         // capability query, no casts
        CHECK(mover && !mover->slot->isProtected());  // base slots are unprotected
        turn.log().clear();
        turn.board.move(dir);

        CHECK(turn.log().countOf(TurnEvent::Type::ShopTriggered) == 1);
        CHECK(turn.board.countActiveShops() == 0);  // triggered, awaiting removal

        // The triggered flag is effect state: it must survive the board clone
        // (Slot's copy-ctor clones effects), so the NEXT turn removes the shop.
        Board next = Board::cloneFrom(turn.board, &turn);
        CHECK(next.countActiveShops() == 0);
        CHECK(next.removeConsumedShops() == 1);
    }

    // --- Item use: allowed in Begin, consumed, logged -------------------------
    {
        auto run = makeRun(6);
        run->addItem("coin_bag");
        CHECK(run->getInventoryItems().size() == 1);
        run->toggleSelectedItem(0);
        const int coins = run->getCoins();
        run->useHeldItem();
        CHECK(run->getCoins() == coins + 50);
        CHECK(run->getInventoryItems().empty());
        CHECK(run->currentTurnLog().countOf(TurnEvent::Type::ItemUsed) == 1);
    }

    std::cout << checks << " checks, " << failures << " failure(s)\n";
    return failures == 0 ? 0 : 1;
}
