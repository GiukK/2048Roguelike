#include <SFML/Graphics.hpp>
#include <iostream>
#include <cmath>

int prova()
{
    // ----------------------------------------
    // Create fullscreen window
    // ----------------------------------------
    std::vector<sf::VideoMode> modes = sf::VideoMode::getFullscreenModes();
    if (modes.empty()) {
        std::cerr << "No fullscreen modes available!" << std::endl;
        return -1;
    }
    sf::VideoMode fullscreenMode = modes[0];
    sf::RenderWindow window(fullscreenMode, "Game", sf::Style::Default);
    window.setFramerateLimit(100);

    // ----------------------------------------
    // Load texture & create sprite
    // (Using your requested constructor)
    // ----------------------------------------
    sf::Texture board_texture("Sprites/board.png", false, sf::IntRect({ 0, 0 }, { 66, 66 }));

    // Scaling factor for both the board and side boards
    float scaleFactor = 10.f; // Adjust as needed

    // Create the square sprite and apply scaling
    sf::Sprite square(board_texture);
    square.setScale({ scaleFactor, scaleFactor });

    // Get the unscaled local bounds then calculate effective dimensions.
    sf::FloatRect spriteBounds = square.getLocalBounds();
    sf::Vector2f spriteSize = spriteBounds.size;
    float halfW = spriteSize.x * 0.5f;
    float halfH = spriteSize.y * 0.5f;
    // Effective half-width after scaling:
    float effectiveHalfW = halfW * scaleFactor;

    // Center the sprite on screen
    sf::Vector2f center{
        static_cast<float>(window.getSize().x) * 0.5f,
        static_cast<float>(window.getSize().y) * 0.5f
    };
    square.setOrigin({ halfW, halfH });
    square.setPosition(center);

    // ----------------------------------------
    // "Board side" rectangle shape
    // ----------------------------------------
    // Original dimensions: width = 5, height = 44.
    sf::RectangleShape boardSide({ 5.f, 66.f });
    boardSide.setFillColor(sf::Color::Black);

    // Scale the board side rectangle according to the board's scale factor.
    boardSide.setSize({ 5.f * scaleFactor, 44.f * scaleFactor });

    // Center the board side on its own origin.
    sf::FloatRect sideBounds = boardSide.getLocalBounds();
    sf::Vector2f sideSize = sideBounds.size;
    boardSide.setOrigin({ sideSize.x * 0.5f, sideSize.y * 0.5f });

    // Position the board side to the right edge of the board.
    boardSide.setPosition({ center.x + effectiveHalfW, center.y });

    // ----------------------------------------
    // Rotation/page-flip variables
    // ----------------------------------------
    float rotationProgress = 0.f;
    bool  isRed = true;               // Color tracking.
    bool  isBoardSideRight = true;    // Which side the board side is on.
    bool  colorChanged = false;       // Toggle color only once per flip.

    bool isRotating = false;
    bool rotationComplete = true;

    // The sprite starts colored red.
    square.setColor(sf::Color::Red);

    // Board side starts invisible (width=0 scale)
    boardSide.setScale({ 0.f, 1.f });

    // ----------------------------------------
    // Main loop
    // ----------------------------------------
    while (window.isOpen())
    {
        // ----------------------------------------
        // Handle events
        // ----------------------------------------
        while (const std::optional event = window.pollEvent())
        {
            // Close if requested
            if (event->is<sf::Event::Closed>())
                window.close();

            // Handle key releases
            if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>())
            {
                if (keyReleased->scancode == sf::Keyboard::Scancode::Escape) {
                    window.close();
                }

                // Press Space to trigger rotation
                if (keyReleased->scancode == sf::Keyboard::Scancode::Space && rotationComplete) {
                    isRotating = true;
                    rotationComplete = false;
                }
            }
        }

        // ----------------------------------------
        // Rotation logic (page-flip effect)
        // ----------------------------------------

        if (isRotating)
        {
            rotationProgress += 2.f; // degrees per frame

            // Flip horizontally using cosine
            float scaleX = std::cos(rotationProgress * 3.14159f / 180.f);
            // Adjust sprite scale while keeping the effective scale factor.
            square.setScale({ std::abs(scaleX) * scaleFactor, scaleFactor });

            // When nearly edge-on, switch the board side.
            if (std::abs(scaleX) <= 0.01f) {
                isBoardSideRight = !isBoardSideRight;
            }

            // Calculate how wide the board side should be (proportional to the board flip progress)
            float boardSideScale = 1.f - std::abs(scaleX);
            boardSideScale = std::clamp(boardSideScale, 0.f, 1.f);

            // Compute offset based on the effective half-width.
            float boardSideOffset = effectiveHalfW * (1.f - std::abs(scaleX));

            // Update the board side's position depending on its side.
            if (isBoardSideRight) {
                // Phase 1: moving from right edge to center.
                boardSide.setPosition({ center.x + effectiveHalfW - boardSideOffset, center.y });
            }
            else {
                // Phase 2: moving from left edge to center.
                boardSide.setPosition({ center.x - effectiveHalfW + boardSideOffset, center.y });
            }
            boardSide.setScale({ boardSideScale, 1.5f });

            // Toggle color at 90 degrees, only once.
            if (rotationProgress >= 90.f && !colorChanged) {
                isRed = !isRed;
                square.setColor(isRed ? sf::Color::Red : sf::Color::Blue);
                colorChanged = true;
            }

            // End the rotation at 180 degrees.
            if (rotationProgress >= 180.f)
            {
                isRotating = false;
                rotationComplete = true;
                rotationProgress = 0.f;

                // Reset transforms.
                square.setScale({ scaleFactor, scaleFactor });
                boardSide.setScale({ 0.f, 1.f });

                // Ready for next flip.
                colorChanged = false;
            }
        }

        // ----------------------------------------
        // Rendering
        // ----------------------------------------
        window.clear(sf::Color::White);
        window.draw(square);
        window.draw(boardSide);
        window.display();
    }

    return 0;
}
