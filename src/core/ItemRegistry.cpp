#include "core/ItemRegistry.h"

#include <algorithm>

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

std::vector<const ItemDef*> ItemRegistry::getAll() const {
    std::vector<const ItemDef*> result;
    result.reserve(items.size());
    for (const auto& [_, def] : items) {
        result.push_back(&def);
    }
    // items is an unordered_map, so sort for a deterministic shop layout.
    std::sort(result.begin(), result.end(),
              [](const ItemDef* a, const ItemDef* b) { return a->id < b->id; });
    return result;
}

const ItemDef* ItemRegistry::pickWeightedRandom(std::mt19937& rng) const {
    if (items.empty()) return nullptr;

    // Walk the id-sorted view, not the unordered_map: map iteration order is
    // unspecified, so a given roll would map to different items across builds —
    // breaking the "same seed = same run" reproducibility the run RNG provides.
    const auto sorted = getAll();

    float totalWeight = 0.f;
    for (const ItemDef* def : sorted) {
        totalWeight += def->weight;
    }

    std::uniform_real_distribution<float> dist(0.f, totalWeight);
    float roll = dist(rng);

    float cumulative = 0.f;
    for (const ItemDef* def : sorted) {
        cumulative += def->weight;
        if (roll < cumulative) {
            return def;
        }
    }

    return sorted.front();
}

std::vector<const ItemDef*> ItemRegistry::pickMultiple(int count, std::mt19937& rng) const {
    std::vector<const ItemDef*> result;
    result.reserve(count);
    for (int i = 0; i < count; ++i) {
        result.push_back(pickWeightedRandom(rng));
    }
    return result;
}
