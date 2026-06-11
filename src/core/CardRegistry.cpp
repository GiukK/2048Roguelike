#include "core/CardRegistry.h"

#include <algorithm>
#include <stdexcept>

void CardRegistry::registerCard(CardDef def) {
    std::string id = def.id;
    cards[id] = std::move(def);
}

const CardDef& CardRegistry::get(const std::string& id) const {
    auto it = cards.find(id);
    if (it == cards.end()) {
        throw std::runtime_error("Unknown card: " + id);
    }
    return it->second;
}

bool CardRegistry::has(const std::string& id) const {
    return cards.count(id) > 0;
}

std::vector<const CardDef*> CardRegistry::getAll() const {
    std::vector<const CardDef*> result;
    result.reserve(cards.size());
    for (const auto& [_, def] : cards) {
        result.push_back(&def);
    }
    std::sort(result.begin(), result.end(),
              [](const CardDef* a, const CardDef* b) { return a->id < b->id; });
    return result;
}
