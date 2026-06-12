#include "content/BaseContent.h"

#include "core/CardRegistry.h"
#include "core/GameRun.h"
#include "core/ItemRegistry.h"
#include "effects/Cards.h"
#include "effects/EffectContext.h"

#include <algorithm>

namespace content {

void registerBaseItems(ItemRegistry& items) {
    items.registerItem({"coin_bag", "coin_bag",
        "Coin Bag", "Gives 50 coins.", 10, 1.0f,
        [](GameRun& run) -> bool { run.addCoins(50); return true; }
    });

    items.registerItem({"bomb", "bomb",
        "Bomb", "Destroys the selected tile.", 50, 0.5f,
        [](GameRun& run) -> bool {
            auto tiles = run.getSelectedTiles();
            if (tiles.size() != 1) return false;
            run.destroyTile(tiles[0]);
            return true;
        }
    });

    // Switch: swaps any two selected tiles — INCLUDING a shop's phantom tile
    // (allowProtected). Repositioning the shop's requirement, or planting a
    // cheap tile on the shop slot for an easier activation, is part of the
    // item's intended value: a deliberate balance decision priced into the
    // item, not an oversight. Every other board manipulation still refuses
    // protected slots by default (Board::swapTiles).
    items.registerItem({"switch", "switch",
        "Switch", "Swaps the two selected tiles.", 50, 0.25f,
        [](GameRun& run) -> bool {
            auto tiles = run.getSelectedTiles();
            if (tiles.size() != 2) return false;
            run.swapTiles(tiles[0], tiles[1], /*allowProtected=*/true);
            return true;
        }
    });

    // Bomb II: anchored on the selected tile, destroys up to two RANDOM tiles
    // among its 8 neighbours (diagonals included). The anchor itself is left
    // intact — the payload is purely the surrounding tiles. Refuses (does not
    // consume) if nothing surrounds the anchor.
    items.registerItem({"bomb_2", "bomb_2",
        "Bomb II", "Destroys up to two random tiles next to the selected one.", 75, 0.4f,
        [](GameRun& run) -> bool {
            auto sel = run.getSelectedTiles();
            if (sel.size() != 1) return false;

            auto neighbours = run.getTilesInRadius(sel[0], 1, /*includeCenter=*/false);
            if (neighbours.empty()) return false;

            int hits = std::min<int>(2, static_cast<int>(neighbours.size()));
            for (int i = 0; i < hits; ++i) {
                int idx = run.getRandomInt(0, static_cast<int>(neighbours.size()) - 1);
                run.destroyTile(neighbours[idx]);
                neighbours.erase(neighbours.begin() + idx);
            }
            return true;
        }
    });

    // Bomb III: destroys every tile in the full 3x3 block centred on the selected
    // tile (the anchor included).
    items.registerItem({"bomb_3", "bomb_3",
        "Bomb III", "Destroys every tile in the 3x3 block around the selected tile.", 120, 0.2f,
        [](GameRun& run) -> bool {
            auto sel = run.getSelectedTiles();
            if (sel.size() != 1) return false;

            for (Tile* target : run.getTilesInRadius(sel[0], 1, /*includeCenter=*/true)) {
                run.destroyTile(target);
            }
            return true;
        }
    });

    // Brick: locks the selected tile in place (immovable, like a shop slot) until
    // a merge clears it. Refuses if the tile is already bricked.
    items.registerItem({"brick", "brick",
        "Brick", "Locks the selected tile in place until a merge frees it.", 60, 0.3f,
        [](GameRun& run) -> bool {
            auto sel = run.getSelectedTiles();
            if (sel.size() != 1) return false;
            if (sel[0]->isBricked()) return false;

            sel[0]->setBricked(true);
            return true;
        }
    });

    // Black Hole: wipes every (non-shop) tile, then seeds one fresh tile so the
    // board stays playable — an empty board has no legal move and would soft-lock.
    items.registerItem({"black_hole", "black_hole",
        "Black Hole", "Destroys every tile on the board, then spawns a new one.", 150, 0.15f,
        [](GameRun& run) -> bool {
            run.destroyAllTiles();
            run.spawnTile();
            return true;
        }
    });

    // Die: uniformly reshuffles the tiles among their occupied cells (the filled
    // cells stay the same; only which tile sits where changes). Needs >= 2 tiles.
    items.registerItem({"die", "die",
        "Die", "Randomly reshuffles all tiles among their cells.", 40, 0.4f,
        [](GameRun& run) -> bool {
            return run.shuffleTiles() >= 2;
        }
    });

    // Hourglass: rewinds the game. In normal play this is the ONLY way to go back
    // (the B key is debug-only). The depth is card-modifiable (Back to Back):
    // rewind up to getRewindDepth() turns, stopping at the first turn; consumed
    // if at least one rewind happened, kept otherwise (nothing to rewind).
    items.registerItem({"hourglass", "hourglass",
        "Hourglass", "Rewinds the game by one turn.", 80, 0.25f,
        [](GameRun& run) -> bool {
            const int depth = run.getRewindDepth();
            bool rewound = false;
            for (int i = 0; i < depth && run.goBack(); ++i) {
                rewound = true;
            }
            return rewound;
        }
    });

    // Mount: adds a base empty slot at a uniformly-random cell adjacent to the
    // board (a border cell or an interior hole), expanding the playfield.
    items.registerItem({"mount", "mount",
        "Mount", "Adds a new empty slot at the edge of the board.", 70, 0.3f,
        [](GameRun& run) -> bool {
            return run.addRandomSlot();
        }
    });

    // Wrench: removes the selected tile AND the slot beneath it, punching a hole
    // in the board. Cannot target the shop.
    items.registerItem({"wrench", "wrench",
        "Wrench", "Removes the selected tile and its slot, leaving a hole.", 50, 0.35f,
        [](GameRun& run) -> bool {
            auto sel = run.getSelectedTiles();
            if (sel.size() != 1) return false;
            return run.removeSlotUnder(sel[0]);
        }
    });
}

void registerBaseCards(CardRegistry& cards) {
    // Two for Two: the first real card. Reward sourced AT the merge cell, so a
    // future coin chip mounted on that slot scales it (card × chip composition).
    // valueB is the pre-merge source value: "two 2s" reads the tiles as they
    // were, regardless of what merge modifiers do to the result.
    cards.registerCard({"two_for_two", "two_for_two",
        "Two for Two", "Every time two 2 tiles merge, gain 2 coins.", 0, 1.0f,
        []() -> std::unique_ptr<Effect> {
            return std::make_unique<ReactorCard>(
                [](const TurnEvent& e, EffectContext& ctx) {
                    if (e.type == TurnEvent::Type::TileMerged && e.valueB == 2) {
                        ctx.addCoins(2, ctx.board().slotAt(e.coord));
                    }
                });
        }
    });

    // Economic Boom: reacts to bomb use. Plain id check FOR NOW — the intended
    // future is an item TAG system ("bomb" family covering Bomb II/III, plus
    // "X counts as a bomb" modifier effects), which needs ItemDef tags + a
    // tag-query hook on the run. Deliberately not built for one card.
    cards.registerCard({"economic_boom", "economic_boom",
        "Economic Boom", "Every time you use a Bomb, gain 3 coins.", 30, 1.0f,
        []() -> std::unique_ptr<Effect> {
            return std::make_unique<ReactorCard>(
                [](const TurnEvent& e, EffectContext& ctx) {
                    if (e.type == TurnEvent::Type::ItemUsed && e.itemId == "bomb") {
                        ctx.addCoins(3);
                    }
                });
        }
    });

    // Vase of Two: each board resolution rolls the spawn twice (independent
    // random draws, not copies). Copies multiply: two cards = x4, three = x8.
    cards.registerCard({"vase_of_two", "vase_of_two",
        "Vase of Two", "Twice as many tiles spawn each turn. Copies multiply.", 80, 1.0f,
        []() -> std::unique_ptr<Effect> {
            return std::make_unique<SpawnCountCard>(2);
        }
    });

    // Back to Back: each copy makes an Hourglass use worth a full 3 rewinds
    // (two copies = 6, not 5 — copies stack their whole effect, the game's
    // card-stacking rule). The Hourglass itself reads the depth.
    cards.registerCard({"back_to_back", "back_to_back",
        "Back to Back", "An Hourglass rewinds 3 turns instead of 1. Copies add 3 each.", 50, 1.0f,
        []() -> std::unique_ptr<Effect> {
            return std::make_unique<HourglassRewindsCard>(3);
        }
    });

    // Bob: the brick-break signal is the TileMerged event's flag (the only way
    // a brick breaks today). If breaks ever get more sources, promote the flag
    // to a dedicated BrickBroken event and Bob listens to that instead. The
    // granted brick is silently lost if the inventory is full.
    cards.registerCard({"bob", "bob",
        "Bob", "When a Brick breaks, gain a Brick item.", 40, 1.0f,
        []() -> std::unique_ptr<Effect> {
            return std::make_unique<ReactorCard>(
                [](const TurnEvent& e, EffectContext& ctx) {
                    if (e.type == TurnEvent::Type::TileMerged && e.flag) {
                        ctx.addItem("brick");
                    }
                });
        }
    });

    // Consume: end-of-turn aggregate over the ItemUsed events.
    cards.registerCard({"consume", "consume",
        "Consume", "At the end of the turn, gain 2 coins per item used.", 40, 1.0f,
        []() -> std::unique_ptr<Effect> {
            return std::make_unique<ReactorCard>(nullptr,
                [](const TurnLog& log, EffectContext& ctx) {
                    int used = log.countOf(TurnEvent::Type::ItemUsed);
                    if (used > 0) ctx.addCoins(2 * used);
                });
        }
    });

    // Red Light: end-of-turn aggregate over TileSlid — one per tile whose cell
    // changed during the move, distance-independent (merge movers included).
    cards.registerCard({"red_light", "red_light",
        "Red Light", "At the end of the turn, gain 1 coin per tile that moved.", 40, 1.0f,
        []() -> std::unique_ptr<Effect> {
            return std::make_unique<ReactorCard>(nullptr,
                [](const TurnLog& log, EffectContext& ctx) {
                    int moved = log.countOf(TurnEvent::Type::TileSlid);
                    if (moved > 0) ctx.addCoins(moved);
                });
        }
    });

    // Card Ruler: end-of-turn aggregate over the CardTriggered activations.
    // Counts ACTIVATIONS (a card firing on three events = three), and only the
    // granular onEvent ones — every end-of-turn card (itself included) is
    // outside the count by construction (see EffectContext::noteActivation).
    cards.registerCard({"card_ruler", "card_ruler",
        "Card Ruler", "At the end of the turn, gain 3 coins per card effect triggered.", 60, 1.0f,
        []() -> std::unique_ptr<Effect> {
            return std::make_unique<ReactorCard>(nullptr,
                [](const TurnLog& log, EffectContext& ctx) {
                    int fired = log.countOf(TurnEvent::Type::CardTriggered);
                    if (fired > 0) ctx.addCoins(3 * fired);
                });
        }
    });
}

} // namespace content
