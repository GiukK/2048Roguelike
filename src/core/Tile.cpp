#include "core/Tile.h"
#include "core/Slot.h"
#include "core/Board.h"
#include "core/Turn.h"
#include "core/GameRun.h"
#include "states/PlayState.h"
#include "rendering/RenderSystem.h"
#include "rendering/Animation.h"


//the entirety of this class has to be rethought as a more solid and less visual-oriented (sprites)

Tile::Tile(RenderSystem& renderer , Slot* slot, int value) :
value(value),
renderer(renderer),
tile(renderer.getTextureManager().get(std::to_string(value))),
slot(slot)
{
	fixVisualAssets();
}




void Tile::fixVisualAssets() {

	tile.setOrigin({ tile.getGlobalBounds().size.x / 2 , tile.getGlobalBounds().size.y / 2 });
	tile.setPosition(slot->getSlotSprite().getPosition());

	//asset resizing --- all tiles are the same -> bad practice
	renderer.resizeSprite("2" , tile);

}



void Tile::render(RenderSystem& renderer) {

    // evidenziazione merge (se la usi per un solo frame, magari rimetti false dopo il draw)
    if (mergedThisSweep) {
        tile.setColor(sf::Color::Red);
    }

    // --- DRAW -----------------------------------------------------------------
    renderer.draw(tile);


}


//changes ownership of tile ALSO manages internal slot ownership (tile-><-slot) 
void Tile::changeSlot(Slot* a, Slot* b) {
	this->slot = b;
	b->setTile(a->releaseTile());

	startPosition = tile.getPosition();
	targetPosition = b->getSlotSprite().getPosition();
	animationTime = 0.f;
	animating = true;
}



sf::Vector2f Tile::getPosition() {

	return tile.getPosition();
}


int Tile::getValue() const {

	return value;

}

void Tile::setValue(int x) {

	value = x;

}

//the in-merging tile is the one destroyed
void Tile::mergeIntoSlot(Slot* other) {


	//multiple merges are allowed

	std::cout << "Slot merged at: x = " << other->getCoord().x << " and y = " << other->getCoord().y << std::endl;

	int sum = value + other->tile->getValue();

	/*
	other->tile->setValue(sum);
	slot->removeTile();
	other->tile->changeSprite();
	other->triggerMergeEffects();
	*/
	other->removeTile();
	changeSlot(slot, other);
	other->tile->setValue(sum);

	other->tile->changeSprite();
	other->triggerMergeEffects();

	mergedThisSweep = true;

	//in the future the animation of merging and the logic should be separated

	/*  WORKING ON MERGE PARTICLE COMPONENT


	auto ani = std::make_unique<Animation>(
		renderer,           // riferimento al tuo RenderSystem
		"coin_animation",         // id o nome della sprite-sheet registrata
		sf::Vector2i(32, 32),// dimensione di ogni frame
		8,                  // numero di frame totali
		tile.getLocalBounds().getCenter()                // posizione in mondo
	);
	ani->shouldLoop = false;



	*/
}


void Tile::changeSprite() {

	std::string val = std::to_string(value);

	tile.setTexture(renderer.getTextureManager().get(val));

}

void Tile::update(float deltaTime) {
	//----------------------------------------------------------------------------------------------------------

		// --- INPUT & HIT-TEST -----------------------------------------------------
	// usa mapPixelToCoords per essere robusti a view/zoom
	sf::Vector2i mousePosI = sf::Mouse::getPosition(renderer.getWindow());
	sf::Vector2f mousePos = renderer.getWindow().mapPixelToCoords(mousePosI);

	bool hovering = tile.getGlobalBounds().contains(mousePos);
	bool mouseDown = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);

	// edge detection
	bool justPressed = mouseDown && !mouseDownLastFrame;
	bool justReleased = !mouseDown && mouseDownLastFrame;

	// --- HOVER: entra/esci una sola volta (niente pompaggio di scala) ---------
	if (hovering && !wasHovering) {
		currentState = State::Hovered;

		if (!resizeFlag) {
			tile.setScale(tile.getScale() * 1.1f);
			resizeFlag = true;
		}
	}
	if (!hovering && wasHovering) {
		// non forzare Idle se stai “premendo” sopra
		if (currentState != State::Pressed) {
			currentState = State::Idle;
		}

		if (resizeFlag) {
			tile.setScale(tile.getScale() / 1.1f);
			resizeFlag = false;
		}
	}
	wasHovering = hovering;

	// --- CLICK: toggle SOLO su release (press+release dentro) -----------------
	if (justPressed && hovering) {
		currentState = State::Pressed;
		pressedInside = true;
	}

	if (justReleased) {
		if (pressedInside && hovering) {
			// <— toggle UNA volta per click completo
			selected = !selected;
		}
		pressedInside = false;

		// torna allo stato coerente con il puntatore
		currentState = hovering ? State::Hovered : State::Idle;
	}

	// --- COLORE: guidato da selected + hover (se non è merge “forzato”) -------
	if (!mergedThisSweep) {
		if (selected) {
			// leggermente chiaro quando hover
			tile.setColor(hovering ? sf::Color(255, 120, 120) : sf::Color::Red);
		}
		else {
			tile.setColor(hovering ? sf::Color(200, 200, 200) : sf::Color::White);
		}
	}

	// --- BOOKKEEPING FINALE ---------------------------------------------------
	mouseDownLastFrame = mouseDown;


	//----------------------------------------------------------------------------------------------------------

	if (!animating) return;

	animationTime += deltaTime;
	float t = animationTime / animationDuration;

	if (t >= 1.f) {
		tile.setPosition(targetPosition);
		animating = false;
		return;
	}

	// Elastic easing out: tweaked version of easeOutElastic
	float p = 1.f;
	float s = p / 4.f;
	float elasticT = std::pow(2.f, -12.f * t) * std::sin((t - s) * (2.f * 3.14159f) / p) + 1.f;

	sf::Vector2f newPos = startPosition + (targetPosition - startPosition) * elasticT;
	tile.setPosition(newPos);
}
