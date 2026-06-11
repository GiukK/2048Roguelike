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
#include "effects/Cards.h"
#include "effects/CoinChips.h"
#include "effects/EffectContext.h"
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

    // --- Coin pipeline: a merge chip grants, the event follows the merge -----
    {
        auto run = makeRun(9);
        Turn turn(renderer, run.get());
        turn.board.clear();
        Tile* anchor = turn.board.spawnTileAt({0, 0}, 2);
        CHECK(anchor != nullptr);
        CHECK(turn.board.spawnTileAt({1, 0}, 2) != nullptr);

        // No mounting UI yet: programmatic mount IS the engine-level contract.
        anchor->slot->addEffect(std::make_unique<CoinBonusChip>(5));
        turn.log().clear();

        const int coins0 = run->getCoins();
        turn.board.move(Direction::Left);
        CHECK(run->getCoins() == coins0 + 5);
        CHECK(turn.log().countOf(TurnEvent::Type::CoinsGained) == 1);

        // Causal order in the log: the merge precedes its coin consequence,
        // and the coin event carries final/base amounts + the source cell.
        int mergedIdx = -1, coinsIdx = -1;
        const auto& evs = turn.log().events();
        for (int i = 0; i < static_cast<int>(evs.size()); ++i) {
            if (evs[i].type == TurnEvent::Type::TileMerged)  mergedIdx = i;
            if (evs[i].type == TurnEvent::Type::CoinsGained) coinsIdx = i;
        }
        CHECK(mergedIdx >= 0 && coinsIdx > mergedIdx);
        CHECK(evs[coinsIdx].valueA == 5 && evs[coinsIdx].valueB == 5);
        CHECK(evs[coinsIdx].flag && evs[coinsIdx].coord == (Coord{0, 0}));
    }

    // --- Coin pipeline: multiplier chips scale gains, chips survive the clone -
    {
        auto run = makeRun(10);
        Turn turn(renderer, run.get());
        turn.board.clear();
        Tile* anchor = turn.board.spawnTileAt({0, 0}, 2);
        CHECK(anchor != nullptr);
        CHECK(turn.board.spawnTileAt({1, 0}, 2) != nullptr);

        anchor->slot->addEffect(std::make_unique<CoinBonusChip>(5));
        anchor->slot->addEffect(std::make_unique<CoinMultiplierChip>(2));
        turn.log().clear();

        const int coins0 = run->getCoins();
        turn.board.move(Direction::Left);
        CHECK(run->getCoins() == coins0 + 10);  // 5 granted, ×2 in the pipeline
        bool amountsMatch = false;
        for (const auto& e : turn.log().events()) {
            if (e.type == TurnEvent::Type::CoinsGained) {
                amountsMatch = (e.valueA == 10 && e.valueB == 5);
            }
        }
        CHECK(amountsMatch);  // event records final AND pre-modifier amounts

        // Chips are persistent slot effects: the turn-to-turn clone keeps them.
        Board next = Board::cloneFrom(turn.board, &turn);
        Tile* nextTile = tileAt(next, {0, 0});
        CHECK(nextTile && nextTile->slot->findEffect<CoinBonusChip>() != nullptr);
        CHECK(nextTile && nextTile->slot->findEffect<CoinMultiplierChip>() != nullptr);
    }

    // --- Coin pipeline: spending bypasses modifiers; gains persist undo ------
    {
        auto run = makeRun(11);
        const int c0 = run->getCoins();
        run->addCoins(-30);  // spending: direct, never amplified, not logged
        CHECK(run->getCoins() == c0 - 30);
        CHECK(run->currentTurnLog().countOf(TurnEvent::Type::CoinsGained) == 0);

        run->addCoins(50);   // sourceless gain: logged into the current turn
        CHECK(run->getCoins() == c0 + 20);
        CHECK(run->currentTurnLog().countOf(TurnEvent::Type::CoinsGained) == 1);

        // "The world rewinds, the player persists": coins survive goBack.
        Turn scratch(renderer, run.get());
        run->newTurn(scratch.board);
        CHECK(run->goBack());
        CHECK(run->getCoins() == c0 + 20);
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

    // --- Cards: a reactor observes the completed turn and acts ---------------
    {
        auto run = makeRun(14);
        // "Every merge of two 2s → +3 coins, sourced at the merge cell."
        run->addCard(std::make_unique<ReactorCard>(
            [](const TurnEvent& e, EffectContext& ctx) {
                if (e.type == TurnEvent::Type::TileMerged && e.valueB == 2) {
                    ctx.addCoins(3, ctx.board().slotAt(e.coord));
                }
            }));

        Turn turn(renderer, run.get());
        turn.board.clear();
        CHECK(turn.board.spawnTileAt({0, 0}, 2) != nullptr);
        CHECK(turn.board.spawnTileAt({1, 0}, 2) != nullptr);
        turn.log().clear();
        turn.board.move(Direction::Left);

        const int coins0 = run->getCoins();
        run->flushReactors(turn);
        CHECK(run->getCoins() == coins0 + 3);
        // The reward is logged into the observed turn (sourced channel)...
        CHECK(turn.log().countOf(TurnEvent::Type::CoinsGained) == 1);

        // ...and a 4+4 merge does not match the card's condition.
        turn.board.move(Direction::Right);  // lone 4 slides, no merge
        CHECK(turn.log().mergeCount() == 1);
    }

    // --- Cards × chips: a slot chip scales the card's sourced reward ---------
    {
        auto run = makeRun(15);
        run->addCard(std::make_unique<ReactorCard>(
            [](const TurnEvent& e, EffectContext& ctx) {
                if (e.type == TurnEvent::Type::TileMerged) {
                    ctx.addCoins(3, ctx.board().slotAt(e.coord));
                }
            }));

        Turn turn(renderer, run.get());
        turn.board.clear();
        Tile* anchor = turn.board.spawnTileAt({0, 0}, 2);
        CHECK(anchor != nullptr);
        CHECK(turn.board.spawnTileAt({1, 0}, 2) != nullptr);
        anchor->slot->addEffect(std::make_unique<CoinMultiplierChip>(2));
        turn.log().clear();
        turn.board.move(Direction::Left);

        const int coins0 = run->getCoins();
        run->flushReactors(turn);
        CHECK(run->getCoins() == coins0 + 6);  // card grants 3, chip doubles it
    }

    // --- Cards: reactor-caused events are logged but NOT re-dispatched -------
    {
        auto run = makeRun(16);
        // "Every coin gain → +1 more coin": would loop forever if observations
        // cascaded; the capture-count rule makes it fire once per PRIOR gain.
        run->addCard(std::make_unique<ReactorCard>(
            [](const TurnEvent& e, EffectContext& ctx) {
                if (e.type == TurnEvent::Type::CoinsGained) {
                    ctx.addCoins(1, ctx.board().slotAt(e.coord));
                }
            }));

        Turn turn(renderer, run.get());
        turn.board.clear();
        Tile* anchor = turn.board.spawnTileAt({0, 0}, 2);
        CHECK(anchor != nullptr);
        CHECK(turn.board.spawnTileAt({1, 0}, 2) != nullptr);
        anchor->slot->addEffect(std::make_unique<CoinBonusChip>(5));
        turn.log().clear();

        const int coins0 = run->getCoins();
        turn.board.move(Direction::Left);            // chip: +5, logged
        run->flushReactors(turn);
        CHECK(run->getCoins() == coins0 + 6);         // +5 chip, +1 card, no cascade
        CHECK(turn.log().countOf(TurnEvent::Type::CoinsGained) == 2);
    }

    // --- Cards: onTurnEnd fires once with the whole log ----------------------
    {
        auto run = makeRun(17);
        run->addCard(std::make_unique<ReactorCard>(
            nullptr,
            [](const TurnLog& log, EffectContext& ctx) {
                if (log.mergeCount() >= 1) ctx.addCoins(2);
            }));

        Turn turn(renderer, run.get());
        turn.board.clear();
        CHECK(turn.board.spawnTileAt({0, 0}, 2) != nullptr);
        CHECK(turn.board.spawnTileAt({1, 0}, 2) != nullptr);
        turn.log().clear();
        turn.board.move(Direction::Left);

        const int coins0 = run->getCoins();
        run->dispatchTurnEnd(turn);   // the aggregate pass, separate from flush
        CHECK(run->getCoins() == coins0 + 2);
    }

    // --- Hourglass: ItemUsed lands in the ARRIVAL turn's log (pinned) ---------
    // Design doc §13: the popped turn's events rewind away; the hourglass use
    // itself is recorded in the turn the player resumes and dispatched to the
    // reactors right away (the use-time safe point), exactly once.
    {
        auto run = makeRun(13);
        run->addItem("hourglass");
        Turn scratch(renderer, run.get());
        run->newTurn(scratch.board);                  // now on turn 2
        CHECK(run->getTurnCount() == 2);

        run->toggleSelectedItem(0);
        run->useHeldItem();                           // rewinds to turn 1
        CHECK(run->getTurnCount() == 1);
        CHECK(run->getInventoryItems().empty());      // consumed: player persists
        CHECK(run->currentTurnLog().countOf(TurnEvent::Type::ItemUsed) == 1);
    }

    // --- Card registry: acquire + stacking + discard + Two for Two behavior --
    {
        auto run = makeRun(18);
        CHECK(!run->acquireCard("no_such_card"));      // unknown id refused
        CHECK(run->acquireCard("two_for_two"));
        CHECK(run->ownsCard("two_for_two"));
        CHECK(run->acquireCard("two_for_two"));        // stacking is legal:
        CHECK(run->getOwnedCards().size() == 2);       // two copies = two reactors

        Turn turn(renderer, run.get());
        turn.board.clear();
        CHECK(turn.board.spawnTileAt({0, 0}, 2) != nullptr);
        CHECK(turn.board.spawnTileAt({1, 0}, 2) != nullptr);
        turn.log().clear();
        turn.board.move(Direction::Left);

        const int coins0 = run->getCoins();
        run->flushReactors(turn);
        CHECK(run->getCoins() == coins0 + 4);          // both copies fired: 2 + 2

        // Discard unmounts one reactor; the survivor still fires alone.
        CHECK(run->discardCard(0));
        CHECK(run->getOwnedCards().size() == 1);
        CHECK(run->ownsCard("two_for_two"));           // one copy remains

        Turn again(renderer, run.get());
        again.board.clear();
        CHECK(again.board.spawnTileAt({0, 0}, 2) != nullptr);
        CHECK(again.board.spawnTileAt({1, 0}, 2) != nullptr);
        again.log().clear();
        again.board.move(Direction::Left);

        const int coins1 = run->getCoins();
        run->flushReactors(again);
        CHECK(run->getCoins() == coins1 + 2);

        // A 4+4 merge does not qualify (sourceValue != 2).
        Turn other(renderer, run.get());
        other.board.clear();
        CHECK(other.board.spawnTileAt({0, 0}, 4) != nullptr);
        CHECK(other.board.spawnTileAt({1, 0}, 4) != nullptr);
        other.log().clear();
        other.board.move(Direction::Left);

        const int coins2 = run->getCoins();
        run->flushReactors(other);
        CHECK(run->getCoins() == coins2);

        // Discarding the last copy: nothing fires anymore.
        CHECK(run->discardCard(0));
        CHECK(!run->ownsCard("two_for_two"));
        CHECK(!run->discardCard(0));                   // nothing left to discard
    }

    // --- Economic Boom: reacts to bomb use only -------------------------------
    {
        auto run = makeRun(19);
        CHECK(run->acquireCard("economic_boom"));

        Turn turn(renderer, run.get());
        turn.log().clear();
        turn.log().push(TurnEvent::itemUsed("bomb"));
        turn.log().push(TurnEvent::itemUsed("coin_bag"));  // not a bomb: ignored

        const int coins0 = run->getCoins();
        run->flushReactors(turn);
        CHECK(run->getCoins() == coins0 + 3);

        // Exactly-once: the cursor makes a second flush a no-op.
        run->flushReactors(turn);
        CHECK(run->getCoins() == coins0 + 3);
    }

    // --- Vase of Two: spawn count multiplies, capped, zero is legal ----------
    {
        auto run = makeRun(20);
        CHECK(run->getSpawnCountPerTurn() == 1);           // baseline
        CHECK(run->acquireCard("vase_of_two"));
        CHECK(run->getSpawnCountPerTurn() == 2);
        CHECK(run->acquireCard("vase_of_two"));
        CHECK(run->getSpawnCountPerTurn() == 4);           // copies multiply

        for (int i = 0; i < 10; ++i) run->acquireCard("vase_of_two");
        CHECK(run->getSpawnCountPerTurn() == 64);          // sanity cap holds

        auto zeroRun = makeRun(21);
        zeroRun->addCard(std::make_unique<SpawnCountCard>(0));
        CHECK(zeroRun->getSpawnCountPerTurn() == 0);       // "nothing spawns"
    }

    // --- Back to Back: hourglass rewinds 3, clamps at the first turn ----------
    {
        auto run = makeRun(22);
        CHECK(run->getRewindDepth() == 1);                 // baseline
        CHECK(run->acquireCard("back_to_back"));
        CHECK(run->getRewindDepth() == 3);

        Turn scratch(renderer, run.get());
        run->newTurn(scratch.board);
        run->newTurn(scratch.board);
        run->newTurn(scratch.board);                       // 4 turns on the stack
        CHECK(run->getTurnCount() == 4);

        run->addItem("hourglass");
        run->toggleSelectedItem(0);
        run->useHeldItem();
        CHECK(run->getTurnCount() == 1);                   // went back 3, not 1
        CHECK(run->getInventoryItems().empty());           // consumed

        // Fewer turns than the depth: rewind as far as possible, still consumed.
        run->newTurn(scratch.board);                       // back to 2 turns
        run->addItem("hourglass");
        run->toggleSelectedItem(0);
        run->useHeldItem();
        CHECK(run->getTurnCount() == 1);
        CHECK(run->getInventoryItems().empty());

        // Nothing to rewind at all: the hourglass is refused and kept.
        run->addItem("hourglass");
        run->toggleSelectedItem(0);
        run->useHeldItem();
        CHECK(run->getTurnCount() == 1);
        CHECK(run->getInventoryItems().size() == 1);

        // Copies stack their FULL effect: two cards = 6 rewinds ("as if you
        // used 6 Hourglasses"), not 1 + 2 + 2.
        CHECK(run->acquireCard("back_to_back"));
        CHECK(run->getRewindDepth() == 6);
        for (int i = 0; i < 6; ++i) run->newTurn(scratch.board);
        CHECK(run->getTurnCount() == 7);
        run->useHeldItem();                                // still held from above
        CHECK(run->getTurnCount() == 1);                   // 7 -> 1: all 6 rewinds
        CHECK(run->getInventoryItems().empty());
    }

    // --- Bob: a broken brick grants a brick item; full inventory loses it ----
    {
        auto run = makeRun(23);
        CHECK(run->acquireCard("bob"));

        Turn turn(renderer, run.get());
        turn.board.clear();
        Tile* brick = turn.board.spawnTileAt({0, 0}, 2);
        CHECK(brick != nullptr);
        CHECK(turn.board.spawnTileAt({1, 0}, 2) != nullptr);
        brick->setBricked(true);
        turn.log().clear();

        turn.board.move(Direction::Left);                  // merge breaks the brick
        run->flushReactors(turn);
        CHECK(run->getInventoryItems().size() == 1);
        CHECK(run->getInventoryItems()[0] == "brick");

        // Full inventory: the granted brick is silently lost (not queued).
        run->addItem("coin_bag");
        run->addItem("coin_bag");
        CHECK(run->isInventoryFull());

        Turn again(renderer, run.get());
        again.board.clear();
        Tile* brick2 = again.board.spawnTileAt({0, 0}, 2);
        CHECK(brick2 != nullptr);
        CHECK(again.board.spawnTileAt({1, 0}, 2) != nullptr);
        brick2->setBricked(true);
        again.log().clear();
        again.board.move(Direction::Left);
        run->flushReactors(again);
        CHECK(run->getInventoryItems().size() == 3);       // unchanged
    }

    // --- Streaming dispatch: item reactions fire AT use time, not at turn end -
    {
        auto run = makeRun(24);
        run->addCard(std::make_unique<ReactorCard>(
            [](const TurnEvent& e, EffectContext& ctx) {
                if (e.type == TurnEvent::Type::ItemUsed && e.itemId == "coin_bag") {
                    ctx.addCoins(3);
                }
            }));

        run->addItem("coin_bag");
        run->toggleSelectedItem(0);
        const int coins0 = run->getCoins();
        run->useHeldItem();  // no turn ends anywhere in this test...
        CHECK(run->getCoins() == coins0 + 50 + 3);  // ...yet the card already paid
    }

    std::cout << checks << " checks, " << failures << " failure(s)\n";
    return failures == 0 ? 0 : 1;
}
