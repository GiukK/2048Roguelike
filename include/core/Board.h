#pragma once

#include <map>
#include <memory>
#include <functional>

#include "core/utils/Coord.h"
#include "core/utils/Direction.h"
#include "core/utils/MovementQueue.h"
#include "core/Slot.h"

#include <SFML/Graphics.hpp>

class Turn;
class RenderSystem;
class Animation;

class Board {
public:
    using AnimationCallback = std::function<void(std::unique_ptr<Animation>)>;

    Board(RenderSystem& renderer, Turn* turn, bool doInitialSetup = true);

    static Board cloneFrom(const Board& other, Turn* turn);
    void copyStateFrom(const Board& other);

    // cloneFrom returns by value, so the slots (which hold a back-pointer to
    // their owning Board) must be re-parented to the destination after the
    // move; otherwise Slot::board dangles to the moved-from temporary.
    Board(Board&& other) noexcept;

    void handleInput(sf::Event& event);
    void render(RenderSystem& renderer);
    void update(float deltaTime);

    void spawnTileInRandomEmptySlot();
    void move(Direction dir);
    void clear();

    bool allAnimationsFinished() const;
    bool moveWasValid() const;

    void setAnimationCallback(AnimationCallback callback);

    // Tile selection — managed by Board input, queried by item effects.
    // Items like bomb check getSelectedTiles().size() to validate targeting.
    std::vector<Tile*> getSelectedTiles() const;
    void clearSelection();
    void destroyTile(Tile* tile);

    Turn* turn;

private:
    void setupInitialBoard();
    void initVisuals();

    void initializeMovementQueue(Direction dir);
    void resolveNextTileMove(Direction dir);
    Coord getNextCoord(Coord from, Direction dir);

    // interaction
    void updateHoverState();
    void handleClick(sf::Vector2f worldPos);
    Tile* findTileAt(sf::Vector2f pos) const;

    int getRandomInt(int min, int max);

    RenderSystem& renderer;
    sf::Sprite boardSprite;

    std::map<Coord, std::unique_ptr<Slot>> slots;
    MovementQueue movementQueue;

    std::pair<int, int> colRange{-1, 3};
    std::pair<int, int> rowRange{0, 3};

    bool moveValidFlag = false;

    AnimationCallback animationCallback;

    Tile* hoveredTile = nullptr;
};
