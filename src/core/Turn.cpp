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
    case TurnEvent::Type::ItemUsed:
        return "ItemUsed " + e.itemId;
    default:
        return "Unknown";
    }
}
} // namespace

Turn::Turn(RenderSystem& renderer, GameRun* gameRun)
    : renderer(renderer),
      gameRun(gameRun),
      board(renderer, this),
      boardSnapshot(Board::cloneFrom(board, this))
{
    board.setAnimationCallback(gameRun->getAnimationCallback());
}

Turn::Turn(RenderSystem& renderer, GameRun* gameRun, const Board& initialBoard)
    : renderer(renderer),
      gameRun(gameRun),
      board(Board::cloneFrom(initialBoard, this)),
      boardSnapshot(Board::cloneFrom(initialBoard, this))
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
        // update()); the per-frame re-calls of board.move never get here. So this
        // is the single point that fires the Moved event exactly once per move.
        eventLog.push(TurnEvent::moved(static_cast<int>(currentMove)));
        currentPhase = Phase::BoardResolution;
        break;
    case Phase::BoardResolution: currentPhase = Phase::End;             break;
    case Phase::End:             endTurn();                             break;
    }
}

void Turn::endTurn() {
    // Debug: dump what happened this turn before anything advances. Done first so
    // getTurnCount() still reports THIS finishing turn (newTurn below pushes the
    // next one). Verifies the event log and that nothing leaks across turns; fold
    // into the real score/ability consumers in Step 2/3.
    if (debug::Enabled) {
        std::cout << "[turn " << gameRun->getTurnCount() << "] "
                  << eventLog.events().size() << " event(s)\n";
        for (const auto& e : eventLog.events()) {
            std::cout << "    " << describe(e) << '\n';
        }
    }

    // Resolve the shop lifecycle on this turn's finished board *before* cloning
    // it into the next turn: a consumed shop is removed and, when the countdown
    // elapses, a new shop is spawned. Doing it here means the next turn's board
    // (and its snapshot) inherit the correct shop state through the clone, while
    // this turn's board is immediately reset to its own pre-shop snapshot below
    // — so the undo history stays consistent and no shop pointer dangles.
    gameRun->advanceShopState(board);

    gameRun->newTurn(board);

    currentPhase = Phase::Begin;
    currentMove = Direction::None;
    inputReceived = false;
    // Reset board AND log together: this turn is now rewound to its pre-move
    // snapshot, ready to be replayed if the player undoes back into it (Hourglass
    // / debug B). Clearing the log here is what keeps a replayed turn from
    // carrying ghost events from its previous play-through.
    board.copyStateFrom(boardSnapshot);
    eventLog.clear();
}

void Turn::requestShop() {
    shopRequested = true;
}

void Turn::update(float deltaTime) {
    switch (currentPhase) {

    case Phase::Begin:
        board.update(deltaTime);
        if (inputReceived) {
            nextPhase();
        }
        break;

    case Phase::Movement:
        if (inputReceived) {
            board.move(currentMove);
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

    case Phase::BoardResolution:
        board.spawnTileInRandomEmptySlot();
        nextPhase();
        break;

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
    case sf::Keyboard::Scancode::X:
        board.spawnTileInRandomEmptySlot();
        break;
    case sf::Keyboard::Scancode::Delete:
        board.clear();
        board.spawnTileInRandomEmptySlot();
        break;
    case sf::Keyboard::Scancode::B:
        // Debug-only shortcut. In normal play the Hourglass item is the only
        // way to rewind a turn.
        if (debug::Enabled) gameRun->goBack();
        break;
    default:
        break;
    }
}
