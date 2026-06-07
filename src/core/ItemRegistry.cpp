#include "core/ItemRegistry.h"

void ItemRegistry::registerItem(ItemDef def) {
    std::string id = def.id;
    items[id] = std::move(def);
}

const ItemDef& ItemRegistry::get(const std::string& id) const {
    auto it = items.find(id);
    if (it == items.end()) {
        throw std::runtime_error("Unknown item: " + id);
    }
    return it->second;
}

bool ItemRegistry::has(const std::string& id) const {
    return items.count(id) > 0;
}

const ItemDef* ItemRegistry::pickWeightedRandom(std::mt19937& rng) const {
    if (items.empty()) return nullptr;

    float totalWeight = 0.f;
    for (const auto& [_, def] : items) {
        totalWeight += def.weight;
    }

    std::uniform_real_distribution<float> dist(0.f, totalWeight);
    float roll = dist(rng);

    float cumulative = 0.f;
    for (const auto& [_, def] : items) {
        cumulative += def.weight;
        if (roll < cumulative) {
            return &def;
        }
    }

    return &items.begin()->second;
}

std::vector<const ItemDef*> ItemRegistry::pickMultiple(int count, std::mt19937& rng) const {
    std::vector<const ItemDef*> result;
    result.reserve(count);
    for (int i = 0; i < count; ++i) {
        result.push_back(pickWeightedRandom(rng));
    }
    return result;
}
