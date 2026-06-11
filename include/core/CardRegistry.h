#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class Effect;

// One card's content: display data + a factory building its run-scoped reactor
// (usually a ReactorCard, see effects/Cards.h; a stateful card returns its own
// Effect subclass). The ItemRegistry pattern applied to cards: data + lambdas
// keyed by id, no new control flow per card.
struct CardDef {
    std::string id;
    std::string textureId;
    std::string name;
    std::string description;

    // Base cost before modifiers; charged via GameRun::getEffectiveCost(CardDef)
    // — the single hook point for future card-price modifiers.
    int cost = 0;

    // Rarity weight, for when the shop picks cards randomly (today the catalogue
    // is small enough that every unowned card is simply offered).
    float weight = 1.0f;

    // Builds a fresh instance of the card's effect, mounted at run scope by
    // GameRun::acquireCard.
    std::function<std::unique_ptr<Effect>()> instantiate;
};

class CardRegistry {
public:
    void registerCard(CardDef def);
    const CardDef& get(const std::string& id) const;
    bool has(const std::string& id) const;

    // Every registered card, sorted by id — deterministic order for shop
    // display and (future) weighted picks, like ItemRegistry::getAll.
    std::vector<const CardDef*> getAll() const;

private:
    std::unordered_map<std::string, CardDef> cards;
};
