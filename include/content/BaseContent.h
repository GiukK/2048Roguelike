#pragma once

class ItemRegistry;
class CardRegistry;

// The base game CONTENT, split from the engine: GameRun owns the registries
// (mechanism), this unit fills them (policy). Items and cards are data +
// lambdas keyed by id, so adding/balancing content means editing this unit —
// never GameRun. Future content families (BossRegistry, chips) follow the
// same pattern; a config-driven catalogue would replace only this layer.
namespace content {

void registerBaseItems(ItemRegistry& items);
void registerBaseCards(CardRegistry& cards);

} // namespace content
