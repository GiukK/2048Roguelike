#pragma once

class ItemRegistry;
class CardRegistry;
class BossRegistry;

// The base game CONTENT, split from the engine: GameRun owns the registries
// (mechanism), this unit fills them (policy). Items, cards and bosses are
// data + lambdas keyed by id, so adding/balancing content means editing this
// unit — never GameRun. Future content families (chips) follow the same
// pattern; a config-driven catalogue would replace only this layer.
namespace content {

void registerBaseItems(ItemRegistry& items);
void registerBaseCards(CardRegistry& cards);
void registerBaseBosses(BossRegistry& bosses);

} // namespace content
