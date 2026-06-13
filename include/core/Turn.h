#pragma once

#include "core/Board.h"
#include "core/TurnLog.h"
#include "core/utils/Direction.h"

class GameRun;
class RenderSystem;

class Turn {
public:
    enum class Phase { Begin, Movement, BoardResolution, End };

    Turn(RenderSystem& renderer, GameRun* gameRun);
    Turn(RenderSystem& renderer, GameRun* gameRun, const Board& initialBoard);

    void handleInput(sf::Event& event);
    void update(float deltaTime);
    void render(RenderSystem& renderer);

    void requestShop();

    // This turn's event log. Emission sites reach it through the board's back
    // pointer (e.g. tile->slot->board->turn->log()); consumers read it via
    // GameRun::currentTurnLog(). See TurnLog / TurnEvent.
    TurnLog& log() { return eventLog; }
    const TurnLog& log() const { return eventLog; }

    // Declared FIRST so it is already constructed when `board`'s constructor runs
    // setupInitialBoard() -> spawnTileInRandomEmptySlot(), which emits into it.
    TurnLog eventLog;

    // How many of this turn's events the reactors have already seen. Advanced
    // by GameRun::flushReactors at each safe point, so dispatch is in-order and
    // exactly-once no matter how many flushes happen; reset with the log in
    // endTurn so a replayed turn re-fires from scratch.
    size_t reactorCursor = 0;

    GameRun* gameRun;
    Board board;
    Board boardSnapshot;

    // Shop and ante countdowns captured when this turn began. goBack()
    // restores them, so a rewound turn replays with its original clocks
    // instead of ticking either twice — "the world rewinds, the player
    // persists" (docs/effect-engine-design.md §13).
    int shopCountdownAtStart;
    int anteCountdownAtStart;

    Phase currentPhase = Phase::Begin;
    Direction currentMove = Direction::None;

private:
    static const char* phaseToString(Phase phase);

    void nextPhase();
    void endTurn();

    void handleBeginInput(sf::Event& event);

    RenderSystem& renderer;
    bool inputReceived = false;
    bool shopRequested = false;
};
