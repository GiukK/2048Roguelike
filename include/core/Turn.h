#pragma once

#include "core/Board.h"
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

    GameRun* gameRun;
    Board board;
    Board boardSnapshot;

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
