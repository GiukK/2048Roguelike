#include "states/PlayState.h"
#include "states/StateManager.h"
#include "states/ShopState.h"
#include "states/CardsState.h"
#include "rendering/RenderSystem.h"

#include <algorithm>
#include <cmath>

namespace {
// Screen-space HUD positions for the corner buttons (UI view). Temporary home
// until the data-driven UI layer lands.
constexpr float ExitButtonX  = 1800.f;
constexpr float ExitButtonY  = 100.f;
constexpr float CardsButtonX = 1800.f;
constexpr float CardsButtonY = 210.f;
} // namespace

PlayState::PlayState(StateManager& stateManager, RenderSystem& renderer)
    : stateManager(stateManager),
      renderer(renderer)
{
    auto animCallback = [this](std::unique_ptr<Animation> anim) {
        addAnimation(std::move(anim));
    };

    auto shopCallback = [this](GameRun* run) {
        // The shop is modal: it draws the play screen behind it as a frozen
        // backdrop (renderBehind), but the world below is NOT updated while the
        // shop is open — so the inventory can't be touched mid-shop.
        this->stateManager.pushState(std::make_unique<ShopState>(
            this->stateManager, this->renderer, run,
            [this](RenderSystem& r) { renderWorldAndHud(r); }));
    };

    currentRun = std::make_unique<GameRun>(renderer, animCallback, shopCallback);
    currentRun->setAnimationsActiveQuery([this]() { return hasActiveAnimations(); });

    // The HUD/inventory UI lives in PlayUI (the play state's view layer); GameRun
    // is the model. Created after currentRun, which PlayUI reads.
    playUI = std::make_unique<PlayUI>(renderer, *currentRun);

    // Point the board camera at the starting board (native-centered). The camera
    // is otherwise left where the player puts it once panning lands.
    renderer.getBoardCamera().setCenter(currentRun->getBoardContentCenter());

    buttons.emplace_back(renderer, "exit_button", sf::Vector2f{ExitButtonX, ExitButtonY},
        [this]() { this->stateManager.requestPop(); });

    // Cards panel: same modal contract as the shop — frozen backdrop, world
    // below not updated, ESC closes it.
    buttons.emplace_back(renderer, "cards_button", sf::Vector2f{CardsButtonX, CardsButtonY},
        [this]() {
            this->stateManager.pushState(std::make_unique<CardsState>(
                this->stateManager, this->renderer, currentRun.get(),
                [this](RenderSystem& r) { renderWorldAndHud(r); }));
        });

    enter();
}

void PlayState::enter() {
    currentRun->enter();
}

void PlayState::exit() {
    stateManager.requestPop();
}

void PlayState::handleInput(sf::Event& event) {
    auto* key = event.getIf<sf::Event::KeyReleased>();
    if (key && key->scancode == sf::Keyboard::Scancode::Escape) {
        exit();
        return;
    }

    // Scroll wheel zooms the board toward the cursor. Each notch multiplies the
    // zoom by ZoomStepPerNotch ^ delta; the camera clamps to its zoom range.
    if (auto* scroll = event.getIf<sf::Event::MouseWheelScrolled>()) {
        float factor = std::pow(Camera::ZoomStepPerNotch, scroll->delta);
        renderer.zoomBoardTowardPixel(scroll->position, factor);
        return;
    }

    // --- Left button: camera pan only ---
    // Selection lives on the right button (handled by the board), so the left
    // button is purely the pan handle and never selects. The drag threshold only
    // suppresses micro-pans from a shaky click. Right-click events are not
    // intercepted here; they fall through to currentRun -> board.
    if (auto* pressed = event.getIf<sf::Event::MouseButtonPressed>();
        pressed && pressed->button == sf::Mouse::Button::Left) {
        // A press on a UI widget belongs to that widget — don't start a pan, so
        // the inventory/exit area is never a pan handle. The button (which polls
        // the mouse) handles the click itself.
        if (isPointOverUI(pressed->position)) {
            return;
        }
        leftButtonDown = true;
        isDraggingBoard = false;
        pressPixel = pressed->position;
        lastDragPixel = pressed->position;
        return;
    }

    if (auto* moved = event.getIf<sf::Event::MouseMoved>(); moved && leftButtonDown) {
        // Safety: if a release was missed (e.g. mouse left the window), stop.
        if (!sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
            leftButtonDown = false;
            isDraggingBoard = false;
            return;
        }
        sf::Vector2i pos = moved->position;
        if (!isDraggingBoard) {
            sf::Vector2i d = pos - pressPixel;
            if (d.x * d.x + d.y * d.y > DragThresholdPixels * DragThresholdPixels) {
                isDraggingBoard = true;
            }
        }
        if (isDraggingBoard) {
            renderer.panBoardByPixels(pos - lastDragPixel);
        }
        lastDragPixel = pos;
        return;
    }

    if (auto* released = event.getIf<sf::Event::MouseButtonReleased>();
        released && released->button == sf::Mouse::Button::Left) {
        // End the pan gesture. Snapping is driven continuously in update(), so
        // there's nothing to trigger here — and a missed release can't lose it.
        leftButtonDown = false;
        isDraggingBoard = false;
        return;
    }

    // A right-click over the UI must not select the board tile drawn underneath
    // (the board can render below the inventory when zoomed out). Swallow it.
    if (auto* released = event.getIf<sf::Event::MouseButtonReleased>();
        released && released->button == sf::Mouse::Button::Right &&
        isPointOverUI(released->position)) {
        return;
    }

    currentRun->handleInput(event);
}

bool PlayState::isPointOverUI(sf::Vector2i pixel) const {
    sf::Vector2f point(pixel);
    for (const auto& btn : buttons) {
        if (btn.contains(point)) return true;
    }
    return playUI->isPointOverUI(point);
}

void PlayState::update(float deltaTime) {
    // Board camera: advance any running snap, then — whenever the player isn't
    // actively panning and nothing is in flight — keep the camera centre inside
    // the board bounds. Driving the snap from the update loop (instead of the
    // drag-release event) makes it robust: a missed release can no longer leave
    // the board off-centre. We poll the real button state rather than the
    // event-tracked drag flags. snapCenterInto is a no-op while already centred,
    // and it also re-centres for free after Mount/Wrench reshapes the board.
    Camera& camera = renderer.getBoardCamera();
    camera.update(deltaTime);
    if (!sf::Mouse::isButtonPressed(sf::Mouse::Button::Left) && !camera.isAnimating()) {
        camera.snapCenterInto(currentRun->getBoardContentBounds());
    }

    updateWorldAndHud(deltaTime);

    for (auto& btn : buttons) {
        btn.update(deltaTime);
    }

    for (auto& anim : animations) {
        anim->update(deltaTime);
    }

    animations.erase(
        std::remove_if(animations.begin(), animations.end(),
            [](const std::unique_ptr<Animation>& a) { return a->isFinished(); }),
        animations.end());
}

void PlayState::updateWorldAndHud(float dt) {
    currentRun->update(dt);   // model / turn
    playUI->update(dt);       // HUD widgets + deferred select/use/discard
}

void PlayState::render(RenderSystem& renderer) {
    // Layered render: UI backdrop, then the board (under the camera) together
    // with its board-anchored effects, then the screen-space HUD on top.
    renderer.useUIView();
    playUI->renderBackground(renderer);

    renderer.useBoardView();
    currentRun->renderBoard(renderer);
    // Animations here are board-anchored (e.g. merge bursts), so they belong in
    // the board layer and must zoom/pan with it.
    for (auto& anim : animations) {
        renderer.draw(anim->getSprite());
    }

    renderer.useUIView();
    playUI->renderForeground(renderer);
    for (auto& btn : buttons) {
        renderer.draw(btn.getSprite());
    }
}

void PlayState::renderWorldAndHud(RenderSystem& renderer) {
    // The play screen WITHOUT the board-anchored animations or the exit button —
    // matches what ShopState used to get from GameRun::render(). Board anims and
    // the exit button are PlayState-only and not shown behind the shop.
    renderer.useUIView();
    playUI->renderBackground(renderer);
    renderer.useBoardView();
    currentRun->renderBoard(renderer);
    renderer.useUIView();
    playUI->renderForeground(renderer);
}

void PlayState::addAnimation(std::unique_ptr<Animation> anim) {
    if (anim) {
        animations.push_back(std::move(anim));
    }
}
