// Invariant tests for the delicate core semantics: clone/undo, merge emission,
// transient-vs-persistent tile state, shop protection, RNG determinism. These
// are the regressions the effect-engine slices (docs/effect-engine-design.md
// §11) must not introduce. Plain asserts on purpose — no framework dependency;
// exit code 0 = all green. The core is still coupled to RenderSystem, so the
// driver opens a hidden window and loads the real assets (run from the exe dir,
// e.g. via `ctest -C Debug` which sets the working directory).

#include "core/Board.h"
#include "core/Boss.h"
#include "core/GameRun.h"
#include "core/Slot.h"
#include "core/Tile.h"
#include "core/Turn.h"
#include "effects/AttackContext.h"
#include "effects/BossEffects.h"
#include "effects/Cards.h"
#include "effects/CoinChips.h"
#include "effects/EffectContext.h"
#include "effects/MergeContext.h"
#include "rendering/RenderSystem.h"
#include "Debug.h"

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

// Locked-board fixture for the defeat-check tests: fills the 4x4 base grid so
// no two orthogonal neighbors are equal — no merge anywhere, and once full, no
// slide either. Variants overwrite cells before/after filling.
void fillGrid(Board& board, const int (&vals)[4][4]) {
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x)
            board.spawnTileAt({x, y}, vals[y][x]);
}

constexpr int LockedGrid[4][4] = {{2, 4, 2, 4},
                                  {8, 16, 8, 16},
                                  {2, 4, 2, 4},
                                  {8, 16, 8, 16}};

// Test-only merge modifier: forces the merge's resultValue, to exercise the
// apply-site validation (unbacked values are dropped, backed ones applied).
class ForcedValueModifier : public Effect {
public:
    explicit ForcedValueModifier(int v) : v(v) {}
    void onMergeResolving(MergeContext& merge) override { merge.resultValue = v; }
    std::unique_ptr<Effect> clone() const override {
        return std::make_unique<ForcedValueModifier>(*this);
    }
private:
    int v;
};

// Test-only attack modifier: scales the resolving attack's damage and may
// preserve the attacker ("ghost strike"), exercising both AttackContext knobs
// from either the tile scope (mounted on the attacker) or the run scope
// (added as a card).
class AttackModifier : public Effect {
public:
    explicit AttackModifier(int multiply, bool keepAttacker = false)
        : multiply(multiply), keepAttacker(keepAttacker) {}
    void onAttackResolving(AttackContext& attack) override {
        attack.damage *= multiply;
        if (keepAttacker) attack.consumeAttacker = false;
    }
    std::unique_ptr<Effect> clone() const override {
        return std::make_unique<AttackModifier>(*this);
    }
private:
    int multiply;
    bool keepAttacker;
};

// Minimal boss content for the fixtures: all-default behavior (the Brute
// shape) at a chosen HP; tests that need Block/onDefeat set the lambdas.
BossDef makeTestBoss(int hp) {
    BossDef def;
    def.id = "test_boss";
    def.name = "Test Boss";
    def.textureId = "monstro";
    def.baseHp = hp;
    return def;
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

        // Causal log order: the trigger is a CONSEQUENCE of the merge, so it
        // must follow it (ShopEffect reacts post-apply via onMergeApplied; a
        // regression to the pre-apply hook would log them inverted).
        {
            int mergedAt = -1, triggeredAt = -1;
            const auto& events = turn.log().events();
            for (size_t i = 0; i < events.size(); ++i) {
                if (events[i].type == TurnEvent::Type::TileMerged)    mergedAt = static_cast<int>(i);
                if (events[i].type == TurnEvent::Type::ShopTriggered) triggeredAt = static_cast<int>(i);
            }
            CHECK(mergedAt >= 0 && triggeredAt >= 0 && mergedAt < triggeredAt);
        }

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

    // --- TileSlid: one per tile whose cell changed, distance-independent -----
    {
        auto run = makeRun(25);
        Turn turn(renderer, run.get());
        turn.board.clear();
        CHECK(turn.board.spawnTileAt({0, 0}, 2) != nullptr);  // at the wall: stays
        CHECK(turn.board.spawnTileAt({2, 0}, 4) != nullptr);  // slides (1 cell or 2, irrelevant)
        turn.log().clear();
        turn.board.move(Direction::Left);
        CHECK(turn.log().countOf(TurnEvent::Type::TileSlid) == 1);

        // The mover of a merge counts too (its cell changed); the stationary
        // target was destroyed and is naturally absent.
        Turn m(renderer, run.get());
        m.board.clear();
        CHECK(m.board.spawnTileAt({0, 0}, 2) != nullptr);
        CHECK(m.board.spawnTileAt({1, 0}, 2) != nullptr);
        m.log().clear();
        m.board.move(Direction::Left);
        CHECK(m.log().mergeCount() == 1);
        CHECK(m.log().countOf(TurnEvent::Type::TileSlid) == 1);
    }

    // --- Consume: 2 coins per item used, at end of turn -----------------------
    {
        auto run = makeRun(26);
        CHECK(run->acquireCard("consume"));
        Turn turn(renderer, run.get());
        turn.log().clear();
        turn.log().push(TurnEvent::itemUsed("bomb"));
        turn.log().push(TurnEvent::itemUsed("die"));

        const int coins0 = run->getCoins();
        run->dispatchTurnEnd(turn);
        CHECK(run->getCoins() == coins0 + 4);
    }

    // --- Red Light: 1 coin per tile that moved, at end of turn ----------------
    {
        auto run = makeRun(27);
        CHECK(run->acquireCard("red_light"));
        Turn turn(renderer, run.get());
        turn.board.clear();
        CHECK(turn.board.spawnTileAt({0, 0}, 2) != nullptr);  // stays
        CHECK(turn.board.spawnTileAt({2, 0}, 4) != nullptr);  // moves
        CHECK(turn.board.spawnTileAt({3, 0}, 8) != nullptr);  // moves
        turn.log().clear();
        turn.board.move(Direction::Left);

        const int coins0 = run->getCoins();
        run->dispatchTurnEnd(turn);
        CHECK(run->getCoins() == coins0 + 2);
    }

    // --- Card Ruler: 3 coins per onEvent ACTIVATION; turn-end cards excluded --
    {
        auto run = makeRun(28);
        CHECK(run->acquireCard("two_for_two"));
        CHECK(run->acquireCard("card_ruler"));

        Turn turn(renderer, run.get());
        turn.board.clear();
        CHECK(turn.board.spawnTileAt({0, 0}, 2) != nullptr);
        CHECK(turn.board.spawnTileAt({1, 0}, 2) != nullptr);
        CHECK(turn.board.spawnTileAt({0, 1}, 2) != nullptr);
        CHECK(turn.board.spawnTileAt({1, 1}, 2) != nullptr);
        turn.log().clear();
        turn.board.move(Direction::Left);  // two 2+2 merges

        const int coins0 = run->getCoins();
        run->flushReactors(turn);          // Two for Two fires TWICE: +4
        CHECK(run->getCoins() == coins0 + 4);
        // ...and each firing is its own activation ("how many times" parsable).
        CHECK(turn.log().countOf(TurnEvent::Type::CardTriggered) == 2);

        // Activation precedes its consequence in the log.
        int trigIdx = -1, coinIdx = -1;
        const auto& evs = turn.log().events();
        for (int i = 0; i < static_cast<int>(evs.size()); ++i) {
            if (trigIdx < 0 && evs[i].type == TurnEvent::Type::CardTriggered) trigIdx = i;
            if (coinIdx < 0 && evs[i].type == TurnEvent::Type::CoinsGained)   coinIdx = i;
        }
        CHECK(trigIdx >= 0 && coinIdx > trigIdx);

        run->dispatchTurnEnd(turn);        // Card Ruler: 3 x 2 activations
        CHECK(run->getCoins() == coins0 + 4 + 6);
        // Its own end-of-turn grant is NOT an activation: count unchanged.
        CHECK(turn.log().countOf(TurnEvent::Type::CardTriggered) == 2);
    }

    // --- Swap: protected slots keep their tile; override is an explicit opt-in -
    {
        auto run = makeRun(29);
        Turn turn(renderer, run.get());
        turn.board.clear();
        CHECK(turn.board.spawnShop(4));
        Tile* phantom = nullptr;
        for (Tile* t : turn.board.getAllTiles()) {
            if (t->slot && t->slot->isProtected()) phantom = t;
        }
        CHECK(phantom != nullptr);
        Tile* regular = turn.board.spawnTileAt({0, 0}, 2);
        CHECK(regular != nullptr);

        Slot* shopSlot = phantom->slot;
        Slot* baseSlot = regular->slot;

        // Default policy: the swap is refused, consistent with destroy/wrench.
        turn.board.swapTiles(phantom, regular);
        CHECK(phantom->slot == shopSlot && regular->slot == baseSlot);

        // Self-swap is a no-op (would otherwise release the same slot twice).
        turn.board.swapTiles(regular, regular);
        CHECK(regular->slot == baseSlot);

        // allowProtected is the deliberate escape hatch (the Switch item uses it).
        turn.board.swapTiles(phantom, regular, /*allowProtected=*/true);
        CHECK(phantom->slot == baseSlot && regular->slot == shopSlot);
    }

    // --- Switch item: moving the shop's phantom tile is its FEATURE ----------
    // End-to-end through the real item flow (selection -> useHeldItem): the
    // Switch opts into allowProtected, so it may reposition the shop's
    // requirement tile — a deliberate balance decision, pinned here so the
    // opt-in can't be lost in a refactor.
    {
        auto run = makeRun(34);
        Board& board = run->currentBoard();
        board.clear();
        CHECK(board.spawnShop(4));
        Tile* phantom = nullptr;
        for (Tile* t : board.getAllTiles()) {
            if (t->slot && t->slot->isProtected()) phantom = t;
        }
        CHECK(phantom != nullptr);
        Tile* regular = board.spawnTileAt({0, 0}, 2);
        CHECK(regular != nullptr);
        Slot* shopSlot = phantom->slot;
        Slot* baseSlot = regular->slot;

        phantom->setSelected(true);
        regular->setSelected(true);
        run->addItem("switch");
        run->toggleSelectedItem(0);
        run->useHeldItem();

        CHECK(run->getInventoryItems().empty());   // consumed: the swap happened
        CHECK(phantom->slot == baseSlot && regular->slot == shopSlot);
        CHECK(board.getSelectedTiles().empty());   // targeting cleared after use
    }

    // --- Merge validation: unbacked resultValue dropped, backed one applied --
    {
        auto run = makeRun(30);
        Turn turn(renderer, run.get());
        turn.board.clear();
        Tile* anchor = turn.board.spawnTileAt({0, 0}, 2);
        CHECK(anchor != nullptr);
        CHECK(turn.board.spawnTileAt({1, 0}, 2) != nullptr);
        anchor->slot->addEffect(std::make_unique<ForcedValueModifier>(5));  // no artwork
        turn.log().clear();

        turn.board.move(Direction::Left);          // must not crash
        Tile* merged = tileAt(turn.board, {0, 0});
        CHECK(merged && merged->getValue() == 4);   // modifier dropped, sum applied
        CHECK(turn.log().highestMergeValue() == 4); // the EVENT matches what landed

        // A backed value passes through the same gate untouched.
        Turn valid(renderer, run.get());
        valid.board.clear();
        Tile* anchor2 = valid.board.spawnTileAt({0, 0}, 2);
        CHECK(anchor2 != nullptr);
        CHECK(valid.board.spawnTileAt({1, 0}, 2) != nullptr);
        anchor2->slot->addEffect(std::make_unique<ForcedValueModifier>(16));
        valid.board.move(Direction::Left);
        Tile* merged2 = tileAt(valid.board, {0, 0});
        CHECK(merged2 && merged2->getValue() == 16);
    }

    // --- Spawn validation: unbacked values refused like a taken cell ----------
    {
        auto run = makeRun(31);
        Turn turn(renderer, run.get());
        turn.board.clear();
        turn.log().clear();
        CHECK(turn.board.spawnTileAt({2, 2}, 3) == nullptr);
        CHECK(turn.board.spawnTileAt({2, 2}, 0) == nullptr);
        CHECK(turn.board.spawnTileAt({2, 2}, -4) == nullptr);
        CHECK(turn.board.spawnTileAt({2, 2}, Tile::MaxValue * 2) == nullptr);
        CHECK(turn.log().countOf(TurnEvent::Type::TileSpawned) == 0);  // nothing logged
        CHECK(turn.board.spawnTileAt({2, 2}, 2) != nullptr);  // the cell itself was fine
    }

    // --- Deferred rewind: requestGoBack pops at the next update, not mid-call -
    {
        auto run = makeRun(32);
        Turn scratch(renderer, run.get());
        run->newTurn(scratch.board);
        CHECK(run->getTurnCount() == 2);
        run->requestGoBack();
        CHECK(run->getTurnCount() == 2);   // deferred: nothing popped yet
        run->update(0.f);                  // processed at the safe point
        CHECK(run->getTurnCount() == 1);
    }

    // --- UI content versions: bumped on every mutation, only on mutations -----
    {
        auto run = makeRun(33);
        const unsigned int i0 = run->getInventoryVersion();
        run->addItem("coin_bag");
        CHECK(run->getInventoryVersion() != i0);
        const unsigned int i1 = run->getInventoryVersion();
        run->toggleSelectedItem(0);
        CHECK(run->getInventoryVersion() == i1);   // selection is not content
        run->useHeldItem();                        // consume mutates
        CHECK(run->getInventoryVersion() != i1);

        const unsigned int c0 = run->getCardsVersion();
        CHECK(run->acquireCard("bob"));
        CHECK(run->getCardsVersion() != c0);
        const unsigned int c1 = run->getCardsVersion();
        CHECK(run->discardCard(0));
        CHECK(run->getCardsVersion() != c1);

        // A refused mutation (inventory full) must NOT bump.
        run->addItem("coin_bag");
        run->addItem("coin_bag");
        run->addItem("coin_bag");
        CHECK(run->isInventoryFull());
        const unsigned int iFull = run->getInventoryVersion();
        run->addItem("coin_bag");
        CHECK(run->getInventoryVersion() == iFull);
    }

    // --- hasLegalMove: slides, full-board lock, immobilized-only escape -------
    {
        auto run = makeRun(40);
        Turn turn(renderer, run.get());
        turn.board.clear();

        // Empty board: nothing can act. Correct as DEFEAT semantics, not just a
        // degenerate answer — an emptied board never produces a valid move, so
        // BoardResolution's spawn is unreachable (a genuine softlock).
        CHECK(!turn.board.hasLegalMove());

        // One tile on an open board: slides everywhere.
        CHECK(turn.board.spawnTileAt({1, 1}, 2) != nullptr);
        CHECK(turn.board.hasLegalMove());

        // Full board, no equal neighbors: locked.
        turn.board.clear();
        fillGrid(turn.board, LockedGrid);
        CHECK(turn.board.getAllTiles().size() == 16);
        CHECK(!turn.board.hasLegalMove());

        // Free one cell: its neighbors gain a slide...
        Tile* center = tileAt(turn.board, {1, 1});
        CHECK(center != nullptr);
        turn.board.destroyTile(center);
        CHECK(turn.board.hasLegalMove());

        // ...unless every tile adjacent to the empty cell is immobilized (the
        // "immobilized-only" fixture: bricks may not slide or initiate merges).
        for (Coord c : {Coord{0, 1}, Coord{2, 1}, Coord{1, 0}, Coord{1, 2}}) {
            Tile* t = tileAt(turn.board, c);
            CHECK(t != nullptr);
            t->setBricked(true);
        }
        CHECK(!turn.board.hasLegalMove());

        // Un-bricking one neighbor re-opens the slide.
        tileAt(turn.board, {0, 1})->setBricked(false);
        CHECK(turn.board.hasLegalMove());
    }

    // --- hasLegalMove: the value cap is not a move; holes are not targets ----
    {
        auto run = makeRun(41);
        Turn turn(renderer, run.get());
        turn.board.clear();

        // The only equal-adjacent pair is two MaxValue tiles: merging them is
        // refused by the cap (no artwork above it), so the board is LOCKED.
        int vals[4][4];
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x)
                vals[y][x] = LockedGrid[y][x];
        vals[0][0] = Tile::MaxValue;
        vals[0][1] = Tile::MaxValue;
        fillGrid(turn.board, vals);
        CHECK(!turn.board.hasLegalMove());

        // A wrenched-out slot is a HOLE, not an empty cell: no slot to slide
        // into, so the board stays locked (the "hole-locked" fixture).
        Tile* wrenched = tileAt(turn.board, {2, 0});
        CHECK(wrenched != nullptr);
        CHECK(turn.board.removeSlotUnder(wrenched));
        CHECK(turn.board.slotAt({2, 0}) == nullptr);
        CHECK(!turn.board.hasLegalMove());

        // An uncapped equal pair anywhere unlocks it.
        Tile* repaint = tileAt(turn.board, {2, 1});
        CHECK(repaint != nullptr);
        turn.board.destroyTile(repaint);
        CHECK(turn.board.spawnTileAt({2, 1}, 16) != nullptr);  // pairs with {1,1}
        CHECK(turn.board.hasLegalMove());
    }

    // --- hasLegalMove: feeding the shop's phantom is the escape valve --------
    {
        auto run = makeRun(42);
        Turn turn(renderer, run.get());
        turn.board.clear();

        // Shop value 32: a value LockedGrid never uses, so the phantom can only
        // ever pair with the tile we repaint below.
        CHECK(turn.board.spawnShop(32));
        Tile* phantom = nullptr;
        for (Tile* t : turn.board.getAllTiles()) {
            if (t->slot && t->slot->isProtected()) phantom = t;
        }
        CHECK(phantom != nullptr);

        // The base-grid cell the shop is attached to (it spawns orthogonally
        // adjacent to an existing slot).
        const Coord sc = phantom->slot->getCoord();
        Coord nc{0, 0};
        bool found = false;
        for (Coord off : {Coord{-1, 0}, Coord{1, 0}, Coord{0, -1}, Coord{0, 1}}) {
            Coord c{sc.x + off.x, sc.y + off.y};
            if (turn.board.slotAt(c)) {
                nc = c;
                found = true;
                break;
            }
        }
        CHECK(found);

        // Full locked grid + a shop whose phantom matches nothing: still defeat.
        // The phantom itself may not initiate (its slot forbids stepping out).
        fillGrid(turn.board, LockedGrid);
        CHECK(!turn.board.hasLegalMove());

        // Repaint the attached neighbor to the phantom's value: feeding the
        // shop IS a legal move (boss-design §8 — same rule attacks will use).
        turn.board.destroyTile(tileAt(turn.board, nc));
        CHECK(turn.board.spawnTileAt(nc, 32) != nullptr);
        CHECK(turn.board.hasLegalMove());
    }

    // --- Defeat signal: a dead new turn fires the callback, latched once -----
    {
        auto run = makeRun(43);
        int defeats = 0;
        run->setDefeatCallback([&defeats](GameRun*) { ++defeats; });
        CHECK(!run->isDefeated());

        // A healthy board does not trip the check.
        Turn scratch(renderer, run.get());
        run->newTurn(scratch.board);
        CHECK(!run->isDefeated());
        CHECK(defeats == 0);

        // A locked board does — exactly once, even if more turns are pushed
        // (the latch: one game-over per run).
        scratch.board.clear();
        fillGrid(scratch.board, LockedGrid);
        run->newTurn(scratch.board);
        CHECK(run->isDefeated());
        CHECK(defeats == 1);
        run->newTurn(scratch.board);
        CHECK(defeats == 1);
    }

    // --- Board-scope reactors: a slot-mounted effect reacts, with identity ----
    // The same ReactorCard substrate as the run-scoped cards, mounted on a
    // slot: board-scope dispatch is the SAME pipeline, not a parallel engine
    // (boss-design §2/§9.2). NOTE: zero cards are mounted here — the board leg
    // must dispatch on its own.
    {
        auto run = makeRun(50);
        Turn turn(renderer, run.get());
        turn.board.clear();
        Tile* anchor = turn.board.spawnTileAt({0, 0}, 2);
        CHECK(anchor != nullptr);
        CHECK(turn.board.spawnTileAt({1, 0}, 2) != nullptr);

        // "Every merge: +1 coin, sourced at MY slot" — owner identity through
        // ctx.ownerSlot(), the §9.2 interface (passed at dispatch, not bound).
        Coord seenOwner{-1, -1};
        anchor->slot->addEffect(std::make_unique<ReactorCard>(
            [&seenOwner](const TurnEvent& e, EffectContext& ctx) {
                if (e.type == TurnEvent::Type::TileMerged && ctx.ownerSlot()) {
                    seenOwner = ctx.ownerSlot()->getCoord();
                    ctx.addCoins(1, ctx.ownerSlot());
                }
            }));
        turn.log().clear();
        turn.board.move(Direction::Left);

        const int coins0 = run->getCoins();
        run->flushReactors(turn);
        CHECK(run->getCoins() == coins0 + 1);
        CHECK(seenOwner == (Coord{0, 0}));

        // Exactly-once: the cursor covers the board scope too.
        run->flushReactors(turn);
        CHECK(run->getCoins() == coins0 + 1);
    }

    // --- Board-scope reactors: owners destroyed mid-flush are skipped ---------
    // The owner-lifetime wrinkle (boss-design §9.2): the dispatch list is
    // snapshot up front, so each owner is re-validated before its hook runs.
    // (Destruction and mid-flush MOUNTING are tested in separate blocks on
    // purpose: free-then-allocate in one flush can hand the freed effect's
    // address to the newcomer — the accepted-benign pointer-reuse corner
    // documented in GameRun::flushReactors — which would make a combined
    // test nondeterministic.)
    {
        auto run = makeRun(51);
        Turn turn(renderer, run.get());
        turn.board.clear();
        Tile* killer = turn.board.spawnTileAt({0, 0}, 2);
        Tile* victim = turn.board.spawnTileAt({2, 0}, 4);
        CHECK(killer && victim);

        // Killer (tile effect at {0,0}) dispatches first in the tile leg's
        // coord order and destroys the victim's tile — the victim's own
        // reactor must be defensively skipped, not fired or crashed into.
        killer->addEffect(std::make_unique<ReactorCard>(
            [&victim](const TurnEvent& e, EffectContext& ctx) {
                if (e.type == TurnEvent::Type::Moved) ctx.destroyTile(victim);
            }));
        victim->addEffect(std::make_unique<ReactorCard>(
            [](const TurnEvent& e, EffectContext& ctx) {
                if (e.type == TurnEvent::Type::Moved) ctx.addCoins(100);
            }));

        turn.log().clear();
        turn.log().push(TurnEvent::moved(static_cast<int>(Direction::Left)));

        const int coins0 = run->getCoins();
        run->flushReactors(turn);
        CHECK(run->getCoins() == coins0);                  // the victim never fired
        CHECK(tileAt(turn.board, {2, 0}) == nullptr);      // it died mid-flush
        CHECK(turn.log().countOf(TurnEvent::Type::TileDestroyed) == 1);
    }

    // --- Board-scope reactors: effects mounted mid-flush wait for the next ----
    // Additions join like appended events do — never the in-flight pass.
    {
        auto run = makeRun(54);
        Turn turn(renderer, run.get());
        turn.board.clear();
        Tile* mounter = turn.board.spawnTileAt({0, 0}, 2);
        CHECK(mounter != nullptr);

        mounter->addEffect(std::make_unique<ReactorCard>(
            [](const TurnEvent& e, EffectContext& ctx) {
                if (e.type == TurnEvent::Type::Moved) {
                    ctx.ownerSlot()->addEffect(std::make_unique<ReactorCard>(
                        [](const TurnEvent& ev, EffectContext& c) {
                            if (ev.type == TurnEvent::Type::Moved) c.addCoins(100);
                        }));
                }
            }));

        turn.log().clear();
        turn.log().push(TurnEvent::moved(static_cast<int>(Direction::Left)));

        const int coins0 = run->getCoins();
        run->flushReactors(turn);
        CHECK(run->getCoins() == coins0);   // the newcomer saw nothing this flush

        // The NEXT flush is its first: a fresh Moved event reaches it.
        turn.log().push(TurnEvent::moved(static_cast<int>(Direction::Right)));
        run->flushReactors(turn);
        CHECK(run->getCoins() == coins0 + 100);
    }

    // --- Board-scope reactors: dispatch order board-before-cards, no
    // CardTriggered attribution for board effects -----------------------------
    {
        auto run = makeRun(52);
        run->addCard(std::make_unique<ReactorCard>(
            [](const TurnEvent& e, EffectContext& ctx) {
                if (e.type == TurnEvent::Type::TileMerged) {
                    ctx.addCoins(2, ctx.board().slotAt(e.coord));
                }
            }));

        Turn turn(renderer, run.get());
        turn.board.clear();
        CHECK(turn.board.spawnTileAt({0, 0}, 2) != nullptr);
        CHECK(turn.board.spawnTileAt({1, 0}, 2) != nullptr);
        // Mounted on an EMPTY slot far from the merge: chips keep reacting
        // whether or not a tile sits on them.
        Slot* corner = turn.board.slotAt({3, 3});
        CHECK(corner != nullptr && corner->isEmpty());
        corner->addEffect(std::make_unique<ReactorCard>(
            [](const TurnEvent& e, EffectContext& ctx) {
                if (e.type == TurnEvent::Type::TileMerged) {
                    ctx.addCoins(1, ctx.ownerSlot());
                }
            }));

        turn.log().clear();
        turn.board.move(Direction::Left);
        const int coins0 = run->getCoins();
        run->flushReactors(turn);
        CHECK(run->getCoins() == coins0 + 3);

        // The board scope fires before the run scope (the pinned cross-scope
        // order), so its +1 is logged before the card's +2...
        int boardIdx = -1, cardIdx = -1;
        const auto& evs = turn.log().events();
        for (int i = 0; i < static_cast<int>(evs.size()); ++i) {
            if (evs[i].type != TurnEvent::Type::CoinsGained) continue;
            if (evs[i].valueA == 1) boardIdx = i;
            if (evs[i].valueA == 2) cardIdx = i;
        }
        CHECK(boardIdx >= 0 && cardIdx > boardIdx);
        // ...and only the CARD logs an activation (board reactions are
        // attribution-free for now — pinned in GameRun::flushReactors).
        CHECK(turn.log().countOf(TurnEvent::Type::CardTriggered) == 1);
    }

    // --- Board-scope reactors: the aggregate onTurnEnd pass, with identity ----
    {
        auto run = makeRun(53);
        Turn turn(renderer, run.get());
        turn.board.clear();
        Tile* anchor = turn.board.spawnTileAt({0, 0}, 2);
        CHECK(anchor != nullptr);
        CHECK(turn.board.spawnTileAt({1, 0}, 2) != nullptr);

        anchor->slot->addEffect(std::make_unique<ReactorCard>(
            nullptr,
            [](const TurnLog& log, EffectContext& ctx) {
                if (log.mergeCount() >= 1 && ctx.ownerSlot()) {
                    ctx.addCoins(2, ctx.ownerSlot());
                }
            }));
        turn.log().clear();
        turn.board.move(Direction::Left);

        const int coins0 = run->getCoins();
        run->dispatchTurnEnd(turn);
        CHECK(run->getCoins() == coins0 + 2);
    }

    // --- Boss: spawn, default Hit, attacker consumed, exactly-once per sweep --
    {
        auto run = makeRun(60);
        Turn turn(renderer, run.get());
        turn.board.clear();
        turn.log().clear();
        Boss* boss = turn.board.spawnBoss(makeTestBoss(10), {3, 0});
        CHECK(boss != nullptr);
        CHECK(turn.log().countOf(TurnEvent::Type::BossSpawned) == 1);
        CHECK(turn.board.getBoss() == boss);
        CHECK(turn.board.isBossAt({3, 0}) && !turn.board.isBossAt({2, 0}));

        // One boss at a time (spawn-primitive contract: refused, not crashed).
        CHECK(turn.board.spawnBoss(makeTestBoss(5), {0, 0}) == nullptr);

        // A tile sweeping toward the body slides up to it and hits: damage =
        // its own value (the pinned default), the attacker is consumed, and
        // consumption is NOT a TileDestroyed (bomb-reactive cards stay quiet).
        CHECK(turn.board.spawnTileAt({0, 0}, 4) != nullptr);
        turn.board.move(Direction::Right);
        CHECK(boss->getHp() == 6);
        CHECK(turn.board.getAllTiles().empty());
        CHECK(turn.board.moveWasValid());
        CHECK(turn.log().countOf(TurnEvent::Type::BossDamaged) == 1);
        CHECK(turn.log().countOf(TurnEvent::Type::TileDestroyed) == 0);

        // Event payload: valueA = final damage, valueB = HP after, coord = cell.
        bool payloadOk = false;
        for (const auto& e : turn.log().events()) {
            if (e.type == TurnEvent::Type::BossDamaged) {
                payloadOk = e.valueA == 4 && e.valueB == 6 && e.coord == (Coord{3, 0});
            }
        }
        CHECK(payloadOk);

        // Re-running the sweep changes nothing — the attack happened exactly
        // once (the attacker is gone), same fixpoint contract as merges.
        turn.board.move(Direction::Right);
        CHECK(boss->getHp() == 6);
        CHECK(turn.log().countOf(TurnEvent::Type::BossDamaged) == 1);
    }

    // --- Boss: the body clones with the turn; undo rewinds damage (§7) -------
    {
        auto run = makeRun(61);
        Board& board = run->currentBoard();
        board.clear();
        Boss* boss = run->currentBoard().spawnBoss(makeTestBoss(10), {3, 0});
        CHECK(boss != nullptr);
        CHECK(board.spawnTileAt({0, 0}, 4) != nullptr);

        run->newTurn(board);
        Boss* cloned = run->currentBoard().getBoss();
        CHECK(cloned != nullptr && cloned != boss);     // a real copy
        CHECK(cloned->getHp() == 10);
        CHECK(cloned->occupies({3, 0}));

        // Damage the clone's turn...
        run->currentBoard().move(Direction::Right);
        CHECK(cloned->getHp() == 6);

        // ...then rewind: boss state is board state — HP and the consumed
        // attacker both come back ("the world rewinds").
        CHECK(run->goBack());
        Boss* rewound = run->currentBoard().getBoss();
        CHECK(rewound != nullptr && rewound->getHp() == 10);
        CHECK(run->currentBoard().getAllTiles().size() == 1);
    }

    // --- Boss: the AttackContext pipeline (tile scope + run scope, ghost) ----
    {
        auto run = makeRun(62);
        Turn turn(renderer, run.get());
        turn.board.clear();
        Boss* boss = turn.board.spawnBoss(makeTestBoss(100), {3, 0});
        CHECK(boss != nullptr);

        // Attacker tile effect ×2, run card ×3: 4 × 2 × 3 = 24 damage — the
        // §3 composition point ("+50% damage" chips/cards) exercised end to end.
        Tile* attacker = turn.board.spawnTileAt({0, 0}, 4);
        CHECK(attacker != nullptr);
        attacker->addEffect(std::make_unique<AttackModifier>(2));
        run->addCard(std::make_unique<AttackModifier>(3));

        turn.board.move(Direction::Right);
        CHECK(boss->getHp() == 100 - 24);
        CHECK(turn.board.getAllTiles().empty());

        // Ghost strike: consumeAttacker = false — damage lands, the tile
        // survives, parked flush against the body.
        Tile* ghost = turn.board.spawnTileAt({0, 0}, 8);
        CHECK(ghost != nullptr);
        ghost->addEffect(std::make_unique<AttackModifier>(1, /*keepAttacker=*/true));
        turn.board.move(Direction::Right);
        CHECK(boss->getHp() == 100 - 24 - 24);   // 8 × 1 × 3 (the card still triples)
        CHECK(tileAt(turn.board, {2, 0}) == ghost);
    }

    // --- Boss: a Block answer is a wall — nothing moves, nothing lands -------
    {
        auto run = makeRun(63);
        Turn turn(renderer, run.get());
        turn.board.clear();
        BossDef blocker = makeTestBoss(10);
        blocker.resolveIncoming = [](const Boss&, const Tile&) {
            return IncomingResolution{IncomingResolution::Kind::Block, 0};
        };
        Boss* boss = turn.board.spawnBoss(blocker, {1, 0});
        CHECK(boss != nullptr);

        CHECK(turn.board.spawnTileAt({0, 0}, 4) != nullptr);
        turn.log().clear();
        turn.board.move(Direction::Right);   // flush against the blocker
        CHECK(!turn.board.moveWasValid());   // a wall: the move is invalid
        CHECK(boss->getHp() == 10);
        CHECK(turn.board.getAllTiles().size() == 1);
        CHECK(turn.log().countOf(TurnEvent::Type::BossDamaged) == 0);
    }

    // --- Boss × defeat check: Hit unlocks a packed board, Block doesn't (§8) --
    {
        auto run = makeRun(64);
        Turn turn(renderer, run.get());
        turn.board.clear();
        BossDef blocker = makeTestBoss(10);
        blocker.resolveIncoming = [](const Boss&, const Tile&) {
            return IncomingResolution{IncomingResolution::Kind::Block, 0};
        };
        CHECK(turn.board.spawnBoss(blocker, {1, 1}) != nullptr);
        fillGrid(turn.board, LockedGrid);    // the body's cell refuses its tile
        CHECK(turn.board.getAllTiles().size() == 15);
        CHECK(!turn.board.hasLegalMove());   // Block = a wall: still locked
    }
    {
        auto run = makeRun(65);
        Turn turn(renderer, run.get());
        turn.board.clear();
        CHECK(turn.board.spawnBoss(makeTestBoss(10), {1, 1}) != nullptr);  // default: Hit
        fillGrid(turn.board, LockedGrid);
        CHECK(turn.board.getAllTiles().size() == 15);
        CHECK(turn.board.hasLegalMove());    // feeding the boss is the escape valve

        // ...and the defeat signal agrees: a board packed solid against an
        // attackable boss is NOT a loss.
        int defeats = 0;
        run->setDefeatCallback([&defeats](GameRun*) { ++defeats; });
        run->newTurn(turn.board);
        CHECK(!run->isDefeated() && defeats == 0);
    }

    // --- Boss death: kill order, onDefeat hook, footprint freed intact (§4) --
    {
        auto run = makeRun(66);
        Turn turn(renderer, run.get());
        turn.board.clear();
        BossDef weakling = makeTestBoss(4);
        // Death effect: pay out, sourced at the corpse's cell (the body is
        // still on the board while onDefeat runs — removal comes after).
        weakling.onDefeat = [](Boss& b, EffectContext& ctx) {
            ctx.addCoins(7, ctx.board().slotAt(b.getFootprint().front()));
        };
        CHECK(turn.board.spawnBoss(weakling, {3, 0}) != nullptr);
        CHECK(turn.board.spawnTileAt({0, 0}, 4) != nullptr);
        turn.log().clear();

        const int coins0 = run->getCoins();
        turn.board.move(Direction::Right);   // 4 damage: the exact kill

        CHECK(turn.board.getBoss() == nullptr);   // body removed
        CHECK(run->getCoins() == coins0 + 7);     // the death effect ran
        CHECK(turn.log().countOf(TurnEvent::Type::BossDamaged) == 1);
        CHECK(turn.log().countOf(TurnEvent::Type::BossDefeated) == 1);

        // Cause before consequence: the killing BossDamaged, then BossDefeated,
        // then the death effect's CoinsGained.
        int dmgIdx = -1, defIdx = -1, coinIdx = -1;
        const auto& evs = turn.log().events();
        for (int i = 0; i < static_cast<int>(evs.size()); ++i) {
            if (evs[i].type == TurnEvent::Type::BossDamaged)  dmgIdx = i;
            if (evs[i].type == TurnEvent::Type::BossDefeated) defIdx = i;
            if (evs[i].type == TurnEvent::Type::CoinsGained)  coinIdx = i;
        }
        CHECK(dmgIdx >= 0 && defIdx > dmgIdx && coinIdx > defIdx);

        // The footprint cell is freed INTACT: spawnable again (§4 default).
        CHECK(turn.board.spawnTileAt({3, 0}, 2) != nullptr);
    }

    // --- Boss death: onDefeat can safely spawn tiles (deferred from sweep) ---
    // Previously, resolveBossDefeat ran mid-sweep — an onDefeat that spawned a
    // tile could reuse a freed Tile* address, corrupting the cellBefore map
    // that produces TileSlid events. Now onDefeat is deferred to after the
    // sweep, so tile spawns are safe.
    {
        auto run = makeRun(76);
        Turn turn(renderer, run.get());
        turn.board.clear();
        BossDef spawner = makeTestBoss(2);
        spawner.onDefeat = [](Boss& b, EffectContext& ctx) {
            ctx.spawnTile({0, 0}, 2);
        };
        CHECK(turn.board.spawnBoss(spawner, {3, 0}) != nullptr);
        CHECK(turn.board.spawnTileAt({2, 0}, 2) != nullptr);
        turn.log().clear();

        turn.board.move(Direction::Right);

        CHECK(turn.board.getBoss() == nullptr);
        Tile* loot = tileAt(turn.board, {0, 0});
        CHECK(loot != nullptr);                            // the loot spawned safely
        CHECK(loot->getValue() == 2);
        CHECK(turn.log().countOf(TurnEvent::Type::BossDefeated) == 1);
    }

    // --- Boss death: overkill sweep — only one BossDefeated per kill ----------
    // Two tiles on adjacent cells both reach the boss in one move. The first
    // kills it; the second must NOT attack the dead body (no extra BossDamaged,
    // no duplicate BossDefeated). Rightward sweep processes right-to-left, so
    // (2,0) reaches the boss first, (1,0) follows into the same cell.
    {
        auto run = makeRun(78);
        Turn turn(renderer, run.get());
        turn.board.clear();
        CHECK(turn.board.spawnBoss(makeTestBoss(2), {3, 0}) != nullptr);
        CHECK(turn.board.spawnTileAt({2, 0}, 2) != nullptr);   // processed first: kills
        CHECK(turn.board.spawnTileAt({1, 0}, 4) != nullptr);   // processed second: must not hit
        turn.log().clear();

        turn.board.move(Direction::Right);

        CHECK(turn.board.getBoss() == nullptr);
        CHECK(turn.log().countOf(TurnEvent::Type::BossDamaged) == 1);
        CHECK(turn.log().countOf(TurnEvent::Type::BossDefeated) == 1);
    }

    // --- Boss action hook: onTurnAction fires during BossFight ---------------
    {
        auto run = makeRun(77);
        run->setAnteFreePlayTurns(2);
        Turn scratch(renderer, run.get());
        scratch.board.clear();

        int actionCount = 0;
        BossDef actor = makeTestBoss(100);
        actor.onTurnAction = [&actionCount](Boss&, EffectContext& ctx) {
            ++actionCount;
            ctx.addCoins(1);
        };
        CHECK(scratch.board.spawnBoss(actor, {0, 0}) != nullptr);

        // During FreePlay the action does NOT fire.
        run->resolveBossAction(scratch.board);
        CHECK(actionCount == 0);

        // Promote to BossFight — now it fires.
        run->advanceAnteState(scratch.board);
        CHECK(run->getAntePhase() == GameRun::AntePhase::BossFight);
        const int coins0 = run->getCoins();
        run->resolveBossAction(scratch.board);
        CHECK(actionCount == 1);
        CHECK(run->getCoins() == coins0 + 1);
    }

    // --- Boss action self-damage: the non-sweep death path reaps the body ----
    // A boss action that drives HP to <= 0 must be DETECTED. Before the reap
    // such a boss was an unreaped 0-HP zombie: the only death path was the
    // movement sweep, and the kill check reads the BossDefeated event that only
    // resolveBossDefeat emits. resolveBossAction now reaps inline (it runs at an
    // end-of-turn safe point, not mid-sweep), so the whole death sequence —
    // BossDefeated, onDefeat, body removal — fires through the one shared path.
    {
        auto run = makeRun(79);
        Turn scratch(renderer, run.get());
        scratch.board.clear();

        int defeatLoot = 0;
        BossDef suicide = makeTestBoss(50);
        suicide.onTurnAction = [](Boss& b, EffectContext&) { b.takeDamage(999); };
        suicide.onDefeat = [&defeatLoot](Boss&, EffectContext&) { ++defeatLoot; };
        CHECK(scratch.board.spawnBoss(suicide, {0, 0}) != nullptr);

        run->advanceAnteState(scratch.board);    // → BossFight
        CHECK(run->getAntePhase() == GameRun::AntePhase::BossFight);
        scratch.log().clear();

        run->resolveBossAction(scratch.board);   // the action's self-damage kills it
        CHECK(scratch.board.getBoss() == nullptr);                        // body reaped inline
        CHECK(defeatLoot == 1);                                           // onDefeat ran
        CHECK(scratch.log().countOf(TurnEvent::Type::BossDefeated) == 1); // exactly one

        // ...and the kill check (which now reads the event, not body-absence)
        // arms the interactive reward, exactly as a sweep kill would.
        run->advanceAnteState(scratch.board);
        CHECK(run->getAntePhase() == GameRun::AntePhase::Reward);
        CHECK(run->isRewardPending());
        CHECK(!run->getRewardOffer().empty());
    }

    // --- Boss as effect owner: the Sleeper (stateful boss, slice 5a) ----------
    // Invincible for SleeperState::InvincibleTurns fight turns (resolveIncoming
    // answers Block — a wall), then awake and damaged by everything; each awake
    // turn it self-inflicts the SUM of its orthogonally adjacent tiles. The
    // phase lives in a mounted SleeperState effect (clones + rewinds with the
    // body) — the entity carrying real behavior as CONTENT, zero core changes.
    {
        auto run = makeRun(81);
        Turn scratch(renderer, run.get());
        scratch.board.clear();
        Boss* boss = scratch.board.spawnBoss(run->getBossRegistry().get("sleeper"), {3, 0});
        CHECK(boss != nullptr);
        CHECK(boss->findEffect<SleeperState>() != nullptr);          // state mounted
        CHECK(!boss->findEffect<SleeperState>()->isVulnerable());    // asleep at spawn

        run->advanceAnteState(scratch.board);   // adopt → BossFight (action can fire)
        CHECK(run->getAntePhase() == GameRun::AntePhase::BossFight);

        // Asleep = a wall: a sweeping tile is Blocked, no damage, attacker parks.
        CHECK(scratch.board.spawnTileAt({0, 0}, 4) != nullptr);
        scratch.board.move(Direction::Right);
        CHECK(boss->getHp() == 32);                          // invulnerable
        CHECK(tileAt(scratch.board, {2, 0}) != nullptr);     // parked against the wall
        CHECK(boss->statusText().rfind("ASLEEP", 0) == 0);   // phase line for the banner

        // Advance through the invincible turns; none deal self-damage (asleep).
        for (int i = 0; i < SleeperState::InvincibleTurns; ++i) {
            CHECK(!boss->findEffect<SleeperState>()->isVulnerable());
            run->resolveBossAction(scratch.board);
        }
        CHECK(boss->findEffect<SleeperState>()->isVulnerable());   // awake now
        CHECK(boss->statusText() == "AWAKE - vulnerable");
        CHECK(boss->getHp() == 32);                                // unhurt while asleep

        // Awake + the parked 4 still adjacent → self-inflicts the sum at turn end.
        run->resolveBossAction(scratch.board);
        CHECK(boss->getHp() == 32 - 4);

        // Awake = damaged by everything: a sweeping tile now lands a Hit.
        scratch.board.clear();
        CHECK(scratch.board.spawnTileAt({0, 0}, 8) != nullptr);
        scratch.board.move(Direction::Right);
        CHECK(boss->getHp() == 28 - 8);
        CHECK(scratch.board.getAllTiles().empty());   // attacker consumed (Hit)
    }

    // --- Sleeper: the body texture follows the phase (bodyTextureId hint) ------
    // Presentation twin of statusText: SleeperState::bodyTextureId drives
    // Boss::currentTextureId, so Board::render shows asleep vs awake art without
    // type-switching on concrete effects. A boss with no such effect (the Brute)
    // falls through to its def's base texture.
    {
        auto run = makeRun(84);
        Turn scratch(renderer, run.get());
        scratch.board.clear();
        Boss* boss = scratch.board.spawnBoss(run->getBossRegistry().get("sleeper"), {1, 1});
        CHECK(boss != nullptr);
        CHECK(boss->currentTextureId() == "sleeper_asleep");   // asleep at spawn

        run->advanceAnteState(scratch.board);                  // adopt → BossFight
        for (int i = 0; i < SleeperState::InvincibleTurns; ++i) {
            run->resolveBossAction(scratch.board);             // alone on board: no self-damage
        }
        CHECK(boss->findEffect<SleeperState>()->isVulnerable());
        CHECK(boss->currentTextureId() == "sleeper_awake");    // awake art once vulnerable

        // The Brute has no body-texture effect → its def's base texture.
        Turn other(renderer, run.get());
        other.board.clear();
        Boss* brute = other.board.spawnBoss(run->getBossRegistry().get("brute"), {1, 1});
        CHECK(brute != nullptr);
        CHECK(brute->currentTextureId() == "monstro");
    }

    // --- Sleeper: the phase clones and rewinds WITHIN the fight (§7) ----------
    // Boss state is board state: a mid-fight rewind undoes the phase like any
    // world change. Also the in-fight rewind SUCCESS path — goBack works ABOVE
    // the fight floor (door one only blocks rewinding OUT of the fight).
    {
        auto run = makeRun(82);
        Board& board = run->currentBoard();
        board.clear();
        CHECK(board.spawnBoss(run->getBossRegistry().get("sleeper"), {3, 0}) != nullptr);
        CHECK(board.spawnTileAt({0, 0}, 2) != nullptr);   // a legal move exists (no defeat latch)
        run->advanceAnteState(board);   // FreePlay→BossFight (arms door one)
        run->newTurn(board);            // applies the cut: this turn is the fight floor

        run->resolveBossAction(run->currentBoard());    // one fight turn elapses
        const int wake1 = run->currentBoard().getBoss()
                              ->findEffect<SleeperState>()->turnsUntilWake();
        run->newTurn(run->currentBoard());              // no transition → no cut; stack grows

        Boss* cloned = run->currentBoard().getBoss();
        CHECK(cloned != nullptr);
        CHECK(cloned->findEffect<SleeperState>()->turnsUntilWake() == wake1);  // phase cloned
        run->resolveBossAction(run->currentBoard());    // advance the clone
        CHECK(run->currentBoard().getBoss()->findEffect<SleeperState>()
                  ->turnsUntilWake() == wake1 - 1);

        CHECK(run->goBack());   // in-fight rewind SUCCEEDS (above the floor)
        CHECK(run->currentBoard().getBoss()->findEffect<SleeperState>()
                  ->turnsUntilWake() == wake1);   // phase rewound with the world
    }

    // --- Sleeper: lethal awake adjacency self-damage is reaped (content path) -
    // The real-content version of the self-damage reap: once awake, a heavy
    // surround sums past its HP; resolveBossAction reaps it inline (the
    // non-sweep death path) and logs exactly one BossDefeated.
    {
        auto run = makeRun(83);
        Turn scratch(renderer, run.get());
        scratch.board.clear();
        Boss* boss = scratch.board.spawnBoss(run->getBossRegistry().get("sleeper"), {1, 1});
        CHECK(boss != nullptr);
        run->advanceAnteState(scratch.board);   // BossFight
        // Surround it: four 16s = 64 adjacent, past its 32 HP.
        CHECK(scratch.board.spawnTileAt({0, 1}, 16) != nullptr);
        CHECK(scratch.board.spawnTileAt({2, 1}, 16) != nullptr);
        CHECK(scratch.board.spawnTileAt({1, 0}, 16) != nullptr);
        CHECK(scratch.board.spawnTileAt({1, 2}, 16) != nullptr);

        for (int i = 0; i < SleeperState::InvincibleTurns; ++i) {
            run->resolveBossAction(scratch.board);   // asleep: advance only, no damage
        }
        CHECK(scratch.board.getBoss() != nullptr);   // survived the sleep
        scratch.log().clear();
        run->resolveBossAction(scratch.board);       // awake: 64 self-damage → dead
        CHECK(scratch.board.getBoss() == nullptr);   // reaped inline (non-sweep path)
        CHECK(scratch.log().countOf(TurnEvent::Type::BossDefeated) == 1);
    }

    // --- Boss occupancy: spawn exclusions on both sides -----------------------
    {
        auto run = makeRun(67);
        Turn turn(renderer, run.get());
        turn.board.clear();
        CHECK(turn.board.spawnBoss(makeTestBoss(10), {1, 1}) != nullptr);

        // The occupied cell is taken for tile spawns despite holding no tile...
        CHECK(turn.board.spawnTileAt({1, 1}, 2) == nullptr);

        // ...and the random spawn never draws it: with only the body's cell
        // left, the spawn is a no-op instead of a wasted (or crashing) pick.
        fillGrid(turn.board, LockedGrid);
        CHECK(turn.board.getAllTiles().size() == 15);
        turn.board.spawnTileInRandomEmptySlot();
        CHECK(turn.board.getAllTiles().size() == 15);
    }
    {
        auto run = makeRun(68);
        Turn turn(renderer, run.get());
        turn.board.clear();
        CHECK(turn.board.spawnShop(4));
        Tile* phantom = nullptr;
        for (Tile* t : turn.board.getAllTiles()) {
            if (t->slot && t->slot->isProtected()) phantom = t;
        }
        CHECK(phantom != nullptr);

        // The body refuses protected slots (a squatted shop would freeze the
        // spawn countdown forever), missing slots, and taken cells.
        CHECK(turn.board.spawnBoss(makeTestBoss(10), phantom->slot->getCoord()) == nullptr);
        CHECK(turn.board.spawnBoss(makeTestBoss(10), {99, 99}) == nullptr);
        CHECK(turn.board.spawnBoss(makeTestBoss(10), {0, 0}) != nullptr);
    }

    // --- Ante machine: countdown → fight → kill → reward → next ante ---------
    {
        auto run = makeRun(70);
        // Decouple the STATE-MACHINE test from boss content: inject a trivially
        // killable test boss with known, ante-scaled HP. The real ante boss is
        // the Sleeper, whose invulnerability window is its own fight's concern,
        // not the machine's. This is exactly what the bossForAnte seam is for.
        run->setBossForAnteStrategy([](const BossRegistry&, int ante) {
            return makeTestBoss(64 * ante);
        });
        run->setAnteFreePlayTurns(2);
        CHECK(run->getAntePhase() == GameRun::AntePhase::FreePlay);
        CHECK(run->getAnte() == 1);

        Turn scratch(renderer, run.get());
        scratch.board.clear();
        scratch.board.spawnTileInRandomEmptySlot();  // keeps the board alive

        run->advanceAnteState(scratch.board);        // 2 -> 1
        CHECK(run->getAnteCountdown() == 1);
        CHECK(scratch.board.getBoss() == nullptr);

        run->advanceAnteState(scratch.board);        // 1 -> 0: the ante boss arrives
        Boss* boss = scratch.board.getBoss();
        CHECK(boss != nullptr);
        CHECK(run->getAntePhase() == GameRun::AntePhase::BossFight);
        CHECK(boss->getMaxHp() == 64);               // ante 1: 64 × 1 (injected strategy)

        // Door one: the next turn push cuts the stack — no rewinding out of
        // the fight — while the run-level turn number keeps counting.
        run->newTurn(scratch.board);
        CHECK(!run->goBack());
        CHECK(run->getTurnCount() == 2);

        // Shop machinery frozen during the fight.
        const int sc = run->getShopCountdown();
        run->advanceShopState(run->currentBoard());
        CHECK(run->getShopCountdown() == sc);

        // Kill the boss: feed it a 64 from an adjacent cell.
        Board& fight = run->currentBoard();
        CHECK(fight.getBoss() != nullptr);
        const Coord bc = fight.getBoss()->getFootprint().front();
        struct Probe { Coord off; Direction dir; };
        const Probe probes[] = {{{-1, 0}, Direction::Right},
                                {{1, 0}, Direction::Left},
                                {{0, -1}, Direction::Down},
                                {{0, 1}, Direction::Up}};
        Direction dir = Direction::None;
        for (const Probe& p : probes) {
            if (fight.spawnTileAt({bc.x + p.off.x, bc.y + p.off.y}, 64)) {
                dir = p.dir;
                break;
            }
        }
        CHECK(dir != Direction::None);
        fight.move(dir);
        CHECK(fight.getBoss() == nullptr);

        // The kill is detected at turn end: an INTERACTIVE reward is armed
        // (offer generated, rewardPending) and the phase moves to Reward. The
        // grant is deferred to the player's pick, not auto-applied.
        run->advanceAnteState(fight);
        CHECK(run->getAntePhase() == GameRun::AntePhase::Reward);
        CHECK(run->isRewardPending());
        CHECK(!run->getRewardOffer().empty());
        CHECK(run->getRewardOffer().size() <= 3);

        // Picking grants the chosen option (an item or a card) and clears the
        // owed-reward state — the model path the modal drives in real play.
        const size_t inv0 = run->getInventoryItems().size();
        const size_t cards0 = run->getOwnedCards().size();
        run->pickReward(0);
        CHECK(!run->isRewardPending());
        CHECK(!run->isRewardOpen());
        CHECK(run->getInventoryItems().size() == inv0 + 1 ||
              run->getOwnedCards().size() == cards0 + 1);

        // Door two at the next push: no rewinding past the killing blow.
        run->newTurn(fight);
        CHECK(!run->goBack());

        // The reward turn ends: the next ante begins, clock reset, and its
        // boss arrives harder (placeholder linear HP scaling).
        run->advanceAnteState(run->currentBoard());
        CHECK(run->getAntePhase() == GameRun::AntePhase::FreePlay);
        CHECK(run->getAnte() == 2);
        CHECK(run->getAnteCountdown() == 2);

        run->advanceAnteState(run->currentBoard()); // 2 -> 1
        run->advanceAnteState(run->currentBoard()); // 1 -> 0: ante-2 boss
        Boss* boss2 = run->currentBoard().getBoss();
        CHECK(boss2 != nullptr && boss2->getMaxHp() == 128);
    }

    // --- Reward: defeat arms an interactive offer; pick grants, skip doesn't --
    // The BossFight→Reward transition builds an offer from cards + consumables;
    // the boss's defId rides the BossDefeated event into the context. We drive
    // the model directly here (no modal): picking applies one grant, skipping
    // clears the owed reward with none.
    {
        auto run = makeRun(90);
        Turn scratch(renderer, run.get());
        scratch.board.clear();
        Boss* boss = scratch.board.spawnBoss(makeTestBoss(2), {2, 2});
        CHECK(boss != nullptr);
        run->advanceAnteState(scratch.board);              // adopt → BossFight
        CHECK(run->getAntePhase() == GameRun::AntePhase::BossFight);

        // Lethal 2 from the west (default Hit for the tile's value).
        CHECK(scratch.board.spawnTileAt({1, 2}, 2) != nullptr);
        scratch.board.move(Direction::Right);
        CHECK(scratch.board.getBoss() == nullptr);         // dead

        // The kill event carries the boss defId for the reward context.
        bool taggedDefId = false;
        for (const auto& e : scratch.log().events()) {
            if (e.type == TurnEvent::Type::BossDefeated && e.itemId == "test_boss") {
                taggedDefId = true;
            }
        }
        CHECK(taggedDefId);

        run->advanceAnteState(scratch.board);              // BossFight → Reward
        CHECK(run->getAntePhase() == GameRun::AntePhase::Reward);
        CHECK(run->isRewardPending());
        const size_t offered = run->getRewardOffer().size();
        CHECK(offered >= 1 && offered <= 3);

        // Skip: the owed reward clears, nothing is granted.
        const size_t inv0 = run->getInventoryItems().size();
        const size_t cards0 = run->getOwnedCards().size();
        run->dismissReward();
        CHECK(!run->isRewardPending());
        CHECK(run->getRewardOffer().empty());
        CHECK(run->getInventoryItems().size() == inv0);
        CHECK(run->getOwnedCards().size() == cards0);
    }

    // --- Ante machine: goBack restores the resumed turn's ante countdown -----
    {
        auto run = makeRun(71);
        Turn scratch(renderer, run.get());
        const int a0 = run->getAnteCountdown();
        run->advanceAnteState(scratch.board);
        CHECK(run->getAnteCountdown() == a0 - 1);
        run->newTurn(scratch.board);                 // the new top captured a0 - 1
        run->advanceAnteState(scratch.board);
        CHECK(run->getAnteCountdown() == a0 - 2);
        CHECK(run->goBack());                        // resume the FIRST turn...
        CHECK(run->getAnteCountdown() == a0);        // ...which started at a0
    }

    // --- Ante machine: a debug-spawned body is adopted IMMEDIATELY -----------
    // (not replaced, and not left waiting for the countdown: a visible boss
    // that is mechanically not a fight would be a lie — the V-key regression).
    {
        auto run = makeRun(72);
        Turn scratch(renderer, run.get());
        scratch.board.clear();
        CHECK(scratch.board.spawnBoss(makeTestBoss(10), {0, 0}) != nullptr);
        CHECK(run->getAnteCountdown() > 1);          // budget far from exhausted
        run->advanceAnteState(scratch.board);        // adopt now, preempting the clock
        CHECK(run->getAntePhase() == GameRun::AntePhase::BossFight);
        CHECK(scratch.board.getBoss() != nullptr);
        CHECK(scratch.board.getBoss()->getMaxHp() == 10);
        // The transition is a first-class event in the finishing turn's log.
        CHECK(scratch.log().countOf(TurnEvent::Type::AntePhaseChanged) == 1);
    }

    // --- Ante machine: a full board delays the fight, the retry lands it -----
    {
        auto run = makeRun(73);
        run->setAnteFreePlayTurns(1);
        Turn scratch(renderer, run.get());
        scratch.board.clear();
        fillGrid(scratch.board, LockedGrid);         // 16/16: nowhere to spawn
        run->advanceAnteState(scratch.board);
        CHECK(run->getAntePhase() == GameRun::AntePhase::FreePlay);
        CHECK(scratch.board.getBoss() == nullptr);

        scratch.board.destroyTile(tileAt(scratch.board, {1, 1}));
        run->advanceAnteState(scratch.board);        // the freed cell hosts it
        CHECK(run->getAntePhase() == GameRun::AntePhase::BossFight);
        CHECK(scratch.board.getBoss() != nullptr);
    }

    // --- Ante machine × the real turn loop: the cut spares the cutting turn --
    // The double-door cut fires inside Turn::endTurn (via newTurn). Without
    // the retired-turns graveyard it destroyed the very Turn whose endTurn
    // was still on the call stack — a use-after-free crash the direct-call
    // tests above can't see, because they never reach the cut from endTurn.
    {
        auto run = makeRun(74);
        run->setAnteFreePlayTurns(1);                // the fight starts this turn

        Board& board = run->currentBoard();
        board.clear();
        CHECK(board.spawnTileAt({1, 0}, 2) != nullptr);
        CHECK(board.spawnTileAt({2, 0}, 2) != nullptr);  // Left is a valid move

        sf::Event::KeyReleased key;
        key.scancode = sf::Keyboard::Scancode::A;        // move Left
        sf::Event event(key);
        run->handleInput(event);

        // Drive the real loop until the turn cycles (movement, resolution,
        // endTurn -> boss spawn -> newTurn applies the cut). Bounded so a
        // regression that stalls the loop fails instead of hanging.
        for (int i = 0; i < 200 && run->getTurnCount() == 1; ++i) {
            run->update(0.5f);
        }
        CHECK(run->getTurnCount() == 2);
        CHECK(run->getAntePhase() == GameRun::AntePhase::BossFight);
        CHECK(run->currentBoard().getBoss() != nullptr);
        CHECK(!run->goBack());                       // door one: the new stack floor
        run->update(0.f);                            // empties the graveyard safely
        CHECK(run->currentBoard().getBoss() != nullptr);
    }

    // --- Ante machine: third door — Reward→FreePlay is unrewindable -----------
    // Without door three, an Hourglass on the first turn of a new ante rewinds
    // into the spent ante's Reward turn. anteCountdown restores to 0 (it was 0
    // throughout the fight/reward), and the replayed Reward→FreePlay branch of
    // advanceAnteState spawns the next boss immediately — the player loses all
    // free-play turns of the new ante.
    {
        auto run = makeRun(75);
        run->setAnteFreePlayTurns(2);
        Turn scratch(renderer, run.get());
        scratch.board.clear();

        // Walk through one full ante: FreePlay → BossFight → kill → Reward → FreePlay.
        // Uses a 4-tile feeding a 4-hp boss (the real sweep path, not raw takeDamage).
        CHECK(scratch.board.spawnBoss(makeTestBoss(4), {1, 0}) != nullptr);
        run->advanceAnteState(scratch.board);                // → BossFight (door 1)
        CHECK(run->getAntePhase() == GameRun::AntePhase::BossFight);

        CHECK(scratch.board.spawnTileAt({0, 0}, 4) != nullptr);
        scratch.board.move(Direction::Right);                // kill via the sweep
        CHECK(scratch.board.getBoss() == nullptr);
        run->advanceAnteState(scratch.board);                // → Reward (door 2)
        CHECK(run->getAntePhase() == GameRun::AntePhase::Reward);

        run->advanceAnteState(scratch.board);                // → FreePlay ante 2 (door 3)
        CHECK(run->getAntePhase() == GameRun::AntePhase::FreePlay);
        CHECK(run->getAnte() == 2);
        CHECK(run->getAnteCountdown() == 2);

        // The third door must have armed: goBack is refused.
        run->newTurn(scratch.board);
        CHECK(!run->goBack());
    }

    // --- Debug toggle: runtime switch between admin mode and the real economy -
    // (The tests run in a debug build, where active() starts true and the
    // toggle is live; in release both fold to constant false.)
    {
        auto run = makeRun(80);
        const ItemDef& bag = run->getItemRegistry().get("coin_bag");
        CHECK(debug::active());                         // debug builds start ON
        CHECK(run->getEffectiveCost(bag) == 0);         // admin mode: free
        debug::setActive(false);
        CHECK(run->getEffectiveCost(bag) == bag.cost);  // the real economy
        debug::setActive(true);                         // restore for any later block
        CHECK(run->getEffectiveCost(bag) == 0);
    }

    std::cout << checks << " checks, " << failures << " failure(s)\n";
    return failures == 0 ? 0 : 1;
}
