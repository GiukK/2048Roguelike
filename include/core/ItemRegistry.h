#pragma once

#include <string>
#include <functional>
#include <unordered_map>
#include <vector>
#include <random>
#include <stdexcept>

class GameRun;

struct ItemDef {
    std::string id;
    std::string textureId;

    // Display strings for the UI (tooltips). Functional placeholders for now.
    std::string name;
    std::string description;

    // Base cost before modifiers. The actual price shown/charged in shop
    // goes through GameRun::getEffectiveCost(), which applies passives and discounts.
    int cost = 0;

    // Rarity weight for shop generation. Higher = more common.
    // e.g. weight 1.0 is standard, 0.2 is rare, 3.0 is very common.
    float weight = 1.0f;

    // Returns true if the item was consumed, false if it couldn't be used right now.
    // A false return keeps the item in inventory (e.g. bomb with no valid target).
    std::function<bool(GameRun&)> onUse;
};

class ItemRegistry {
public:
    void registerItem(ItemDef def);
    const ItemDef& get(const std::string& id) const;
    bool has(const std::string& id) const;

    // Every registered item, sorted by id for a stable ordering. Lets callers
    // (e.g. the debug shop) enumerate the full catalogue without hardcoding its
    // size — it scales automatically as items are added.
    std::vector<const ItemDef*> getAll() const;

    // Weighted random selection. Items with higher weight appear more often.
    // pickMultiple allows duplicates — a common item can fill multiple shop slots.
    const ItemDef* pickWeightedRandom(std::mt19937& rng) const;
    std::vector<const ItemDef*> pickMultiple(int count, std::mt19937& rng) const;

private:
    std::unordered_map<std::string, ItemDef> items;
};
