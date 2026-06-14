#include "core/Turn.h"
#include "core/GameRun.h"
#include "rendering/RenderSystem.h"
#include "Debug.h"

#include <iostream>

namespace {
// Debug-only one-line formatting of a turn event. Kept here (not on TurnEvent) so
// the event type stays a pure data POD with no presentation concern.
std::string describe(const TurnEvent& e) {
    switch (e.type) {
    case TurnEvent::Type::Moved:
        return std::string("Moved ") + dirToString(static_cast<Direction>(e.valueA));
    case TurnEvent::Type::TileMerged:
        return "TileMerged ->" + std::to_string(e.valueA) +
               " (from " + std::to_string(e.valueB) + ") at (" +
               std::to_string(e.coord.x) + "," + std::to_string(e.coord.y) + ")" +
               (e.flag ? " [brick broke]" : "");
    case TurnEvent::Type::TileSpawned:
        return "TileSpawned " + std::to_string(e.valueA) + " at (" +
               std::to_string(e.coord.x) + "," + std::to_string(e.coord.y) + ")";
    case TurnEvent::Type::TileDestroyed:
        return "TileDestroyed " + std::to_string(e.valueA) + " at (" +
               std::to_string(e.coord.x) + "," + std::to_string(e.coord.y) + ")";
    case TurnEvent::Type::ShopTriggered:
        return "ShopTriggered at (" + std::to_string(e.coord.x) + "," +
               std::to_string(e.coord.y) + ")";
    case TurnEvent::Type::ShopSpawned:
        return "ShopSpawned " + std::to_string(e.valueA) + " at (" +
               std::to_string(e.coord.x) + "," + std::to_string(e.coord.y) + ")";
    case TurnEvent::Type::TileSlid:
        return "TileSlid " + std::to_string(e.valueA) + " to (" +
               std::to_string(e.coord.x) + "," + std::to_string(e.coord.y) + ")";
    case TurnEvent::Type::CardTriggered:
        return "CardTriggered " + (e.itemId.empty() ? std::string("<engine>") : e.itemId);
    case TurnEvent::Type::CoinsGained:
        return "CoinsGained " + std::to_string(e.valueA) +
               " (base " + std::to_string(e.valueB) + ")" +
               (e.flag ? " at (" + std::to_string(e.coord.x) + "," +
                             std::to_string(e.coord.y) + ")"
                       : "");
    case TurnEvent::Type::ItemUsed:
        return "ItemUsed " + e.itemId;
    case TurnEvent::Type::BossSpawned:
        return "BossSpawned maxHp " + std::to_string(e.valueA) + " at (" +
               std::to_string(e.coord.x) + "," + std::to_string(e.coord.y) + ")";
    case TurnEvent::Type::BossDamaged:
        return "BossDamaged " + std::to_string(e.valueA) + " -> hp " +
               std::to_string(e.valueB) + " at (" + std::to_string(e.coord.x) +
               "," + std::to_string(e.coord.y) + ")";
    case TurnEvent::Type::BossDefeated:
        return "BossDefeated at (" + std::to_string(e.coord.x) + "," +
               std::to_string(e.coord.y) + ")";
    case TurnEvent::Type::AntePhaseChanged:
        return std::string("AntePhaseChanged -> ") +
               GameRun::antePhaseName(static_cast<GameRun::AntePhase>(e.valueA)) +
               " (ante " + std::to_string(e.valueB) + ")";
    default:
        return "Unknown";
    }
}
} // namespace

Turn::Turn(RenderSystem& renderer, GameRun* gameRun)
    : renderer(renderer),
      gameRun(gameRun),
      board(renderer, this),
      boardSnapshot(Board::cloneFrom(board, this)),
      shopCountdownAtStart(gameRun->getShopCountdown()),
      anteCountdownAtStart(gameRun->getAnteCountdown())
{
    board.setAnimationCallback(gameRun->getAnimationCallback());
}

Turn::Turn(RenderSystem& renderer, GameRun* gameRun, const Board& initialBoard)
    : renderer(renderer),
      gameRun(gameRun),
      board(Board::cloneFrom(initialBoard, this)),
      boardSnapshot(Board::cloneFrom(initialBoard, this)),
      shopCountdownAtStart(gameRun->getShopCountdown()),
      anteCountdownAtStart(gameRun->getAnteCountdown())
{
    board.setAnimationCallback(gameRun->getAnimationCallback());
}

const char* Turn::phaseToString(Phase phase) {
    switch (phase) {
    case Phase::Begin:            return "Begin";
    case Phase::Movement:         return "Movement";
    case Phase::BoardResolution:  return "BoardResolution";
    case Phase::End:              return "End";
    default:                      return "Unknown";
    }
}

void Turn::nextPhase() {
    switch (currentPhase) {
    case Phase::Begin:           currentPhase = Phase::Movement;        break;
    case Phase::Movement:
        // Reached only after a VALID move whose animations have finished (see
        // update(); the move itself resolves once, on the phase's first frame).
        // This is the single point that fires the Moved event, once per move.
        eventLog.push(TurnEvent::moved(static_cast<int>(currentMove)));
        currentPhase = Phase::BoardResolution;
        break;
    case Phase::BoardResolution: currentPhase = Phase::End;             break;
    case Phase::End:             endTurn();                             break;
    }
}

void Turn::endTurn() {
    // Resolve the shop lifecycle on this turn's finished board *before* cloning
    // it into the next turn: a consumed shop is removed and, when the countdown
    // elapses, a new shop is spawned. Doing it here means the next turn's board
    // (and its snapshot) inherit the correct shop state through the clone, while
    // this turn's board is immediately reset to its own pre-shop snapshot below
    // — so the undo history stays consistent and no shop pointer dangles.
    gameRun->advanceShopState(board);

    // Boss action: during a fight, the boss acts on the finished board before
    // the ante clock checks for the kill — so consequences like tile destruction
    // are visible to the kill check and to the reactors.
    gameRun->resolveBossAction(board);

    // Then the ante clock, on the same finished board: a fight start spawns
    // the boss here (inherited by the clone), a kill detected here grants the
    // reward into THIS turn's log — both ahead of the flushes below, so the
    // reactors see BossSpawned / the reward CoinsGained the turn they happen.
    gameRun->advanceAnteState(board);

    // Final flush (catches end-of-turn events like ShopSpawned) + the aggregate
    // onTurnEnd pass over the full log. Before newTurn, so reactor mutations to
    // `board` are inherited by the next turn's clone, while the snapshot reset
    // below wipes them from THIS turn — a replay after undo starts clean, per
    // the §13 undo semantics.
    gameRun->flushReactors(*this);
    gameRun->dispatchTurnEnd(*this);

    // Debug: dump what happened this turn — including what the reactors just
    // appended. Before newTurn, so getTurnCount() still reports THIS finishing
    // turn. Independent verification channel; kept alongside the real consumers.
    // The header carries the ante machine's state AFTER advanceAnteState, so
    // every turn shows where the run stands (and the boss clock in FreePlay).
    if (debug::Enabled) {
        const GameRun::AntePhase phase = gameRun->getAntePhase();
        std::cout << "[turn " << gameRun->getTurnCount() << "] "
                  << eventLog.events().size() << " event(s) | ante "
                  << gameRun->getAnte() << ' ' << GameRun::antePhaseName(phase);
        if (phase == GameRun::AntePhase::FreePlay) {
            std::cout << ", boss in " << gameRun->getAnteCountdown();
        }
        std::cout << '\n';
        for (const auto& e : eventLog.events()) {
            std::cout << "    " << describe(e) << '\n';
        }
    }

    gameRun->newTurn(board);

    currentPhase = Phase::Begin;
    currentMove = Direction::None;
    inputReceived = false;
    // Reset board, log AND reactor cursor together: this turn is now rewound to
    // its pre-move snapshot, ready to be replayed if the player undoes back into
    // it (Hourglass / debug B). Clearing the log keeps a replayed turn from
    // carrying ghost events; resetting the cursor lets the replay re-fire its
    // reactors from scratch (the §13 balance-watch consequence, accepted).
    board.copyStateFrom(boardSnapshot);
    eventLog.clear();
    reactorCursor = 0;
}

void Turn::requestShop() {
    shopRequested = true;
}

void Turn::update(float deltaTime) {
    switch (currentPhase) {

    case Phase::Begin:
        // A reward owed from the kill turn opens its picker before play resumes
        // — the Phase::Begin twin of the shop's Phase::End gate. The modal
        // pauses the world (PlayState stops updating the run) until the player
        // chooses, so no move is processed while a reward is owed.
        if (gameRun->isRewardPending()) {
            if (!gameRun->isRewardOpen()) gameRun->openReward();
            // Drop any move captured the frame the reward turn opened, so a
            // keypress can't leak past the modal and fire once it closes.
            inputReceived = false;
            currentMove = Direction::None;
            break;
        }
        board.update(deltaTime);
        if (inputReceived) {
            nextPhase();
        }
        break;

    case Phase::Movement:
        if (inputReceived) {
            // Resolve the move ONCE; the rest of this phase only plays animations.
            // Re-running move() every frame happened to be a no-op (mergedThisSweep
            // persists), but a modifier that reshapes the board mid-merge (e.g. a
            // destroy-on-merge chip) would make a re-run slide tiles into the freed
            // space within the same move — so the one-shot contract is load-bearing.
            board.move(currentMove);
            inputReceived = false;
            // Safe point: the sweep is fully resolved, so reactors may now see
            // its events (merges, brick breaks) and mutate freely — granular
            // "as it happens" timing without ever touching an in-flight sweep.
            gameRun->flushReactors(*this);
        }
        board.update(deltaTime);

        if (board.moveWasValid()) {
            if (board.allAnimationsFinished()) {
                nextPhase();
            }
        } else {
            currentPhase = Phase::Begin;
            currentMove = Direction::None;
            inputReceived = false;
        }
        break;

    case Phase::BoardResolution: {
        // Cards may multiply the per-turn spawn count (Vase of Two). Each spawn
        // is an independent random draw — position and value are re-rolled, not
        // copied — and a full board simply absorbs the extras as no-ops.
        const int spawns = gameRun->getSpawnCountPerTurn();
        for (int i = 0; i < spawns; ++i) {
            board.spawnTileInRandomEmptySlot();
        }
        // Safe point: the Moved event (pushed on entering this phase) and the
        // spawns above reach the reactors before the turn wraps up.
        gameRun->flushReactors(*this);
        nextPhase();
        break;
    }

    case Phase::End:
        if (!shopRequested && !gameRun->shopOpen) {
            nextPhase();
        } else if (shopRequested && gameRun->hasActiveAnimations()) {
            // wait for animations to finish before opening shop
        } else if (shopRequested && !gameRun->shopOpen) {
            gameRun->openShop();
            shopRequested = false;
        }
        // if shopOpen, wait for it to close
        break;
    }
}

void Turn::render(RenderSystem& renderer) {
    board.render(renderer);
}

void Turn::handleInput(sf::Event& event) {
    if (currentPhase == Phase::Begin) {
        board.handleInput(event);
        handleBeginInput(event);
    }
}

void Turn::handleBeginInput(sf::Event& event) {
    auto* key = event.getIf<sf::Event::KeyReleased>();
    if (!key) return;

    switch (key->scancode) {
    case sf::Keyboard::Scancode::A: currentMove = Direction::Left;  inputReceived = true; break;
    case sf::Keyboard::Scancode::D: currentMove = Direction::Right; inputReceived = true; break;
    case sf::Keyboard::Scancode::W: currentMove = Direction::Up;    inputReceived = true; break;
    case sf::Keyboard::Scancode::S: currentMove = Direction::Down;  inputReceived = true; break;
    // The debug keys below are gameplay-affecting, so they follow the RUNTIME
    // toggle (debug::active, the play screen's DEBUG button) — switched off,
    // the binary behaves like the real game without a rebuild.
    case sf::Keyboard::Scancode::X:
        // Debug-only: free spawn to set up board states quickly.
        if (debug::active()) board.spawnTileInRandomEmptySlot();
        break;
    case sf::Keyboard::Scancode::Delete:
        // Debug-only: wipe the board back to a single fresh tile.
        if (debug::active()) {
            board.clear();
            board.spawnTileInRandomEmptySlot();
        }
        break;
    case sf::Keyboard::Scancode::B:
        // Debug-only shortcut (the Hourglass is the only rewind in normal play).
        // DEFERRED via requestGoBack: a synchronous goBack() here would pop —
        // and destroy — this very Turn while this method is still executing.
        if (debug::active()) gameRun->requestGoBack();
        break;
    case sf::Keyboard::Scancode::V:
        // Debug-only: drop the Sleeper on a random empty slot for instant
        // fight/phase-art testing (the ante machine adopts an on-board boss as
        // the fight next endTurn). Points at the current content focus; the
        // Brute stays the all-defaults engine-test fixture (invariant suite).
        if (debug::active() && gameRun->getBossRegistry().has("sleeper")) {
            board.spawnBossInRandomEmptySlot(gameRun->getBossRegistry().get("sleeper"));
        }
        break;
    default:
        break;
    }
}
