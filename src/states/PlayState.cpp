#include "states/PlayState.h"
#include "states/StateManager.h"
#include "states/ShopState.h"
#include "rendering/RenderSystem.h"
#include "ui/UI.h"
#include "Debug.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace {
// Screen-space HUD position for the exit button (UI view). Temporary home until
// the data-driven UI layer lands.
constexpr float ExitButtonX = 1800.f;
constexpr float ExitButtonY = 100.f;
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

    // --- Step 2 verification (debug only) ---------------------------------
    // Build a small data-driven UINode tree (a tooltip-shaped card: title +
    // wrapped description + a badge row), run the two-pass layout, and draw it via
    // the generic framework. Also exercises hitTest: the card tints when hovered.
    // Rebuilt each frame here only because it's a static demo; real UI rebuilds on
    // state change. Folds into the real tooltip at Step 3.
    if (debug::Enabled) {
        using namespace ui;

        UINode title{UIType::Text};
        title.text = "Bomb III";
        title.style.charSize = 30;
        title.style.textColor = sf::Color(255, 220, 120);

        UINode desc{UIType::Text};
        desc.text = "Destroys every tile in the 3x3 block around the selected tile.";
        desc.style.charSize = 20;
        desc.maxW = 320.f;

        UINode badge{UIType::Box};
        badge.padding = 8.f;
        badge.style.fill = sf::Color(60, 50, 90);
        badge.style.cornerRadius = 8.f;
        {
            UINode badgeText{UIType::Text};
            badgeText.text = "120 coins";
            badgeText.style.charSize = 18;
            badge.children.push_back(std::move(badgeText));
        }

        UINode card{UIType::Box};
        card.direction = UIDir::Column;
        card.padding = 16.f;
        card.gap = 12.f;
        card.style.cornerRadius = 16.f;
        card.style.border = sf::Color(210, 210, 225);
        card.style.borderThickness = 3.f;

        sf::Vector2i mp = sf::Mouse::getPosition(renderer.getWindow());
        sf::Vector2f mouse(mp);

        card.children.push_back(std::move(title));
        card.children.push_back(std::move(desc));
        card.children.push_back(std::move(badge));

        measureNode(card, renderer);
        layoutNode(card, 60.f, 700.f);

        // Tint the card background when the cursor is over it (verifies hitTest).
        card.style.fill = (hitTest(card, mouse) != nullptr)
            ? sf::Color(45, 45, 65, 240)
            : sf::Color(28, 28, 40, 235);

        drawNode(card, renderer);
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
