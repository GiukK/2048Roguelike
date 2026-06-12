#include "core/BossRegistry.h"

#include <algorithm>
#include <stdexcept>

void BossRegistry::registerBoss(BossDef def) {
    std::string id = def.id;
    bosses[id] = std::move(def);
}

const BossDef& BossRegistry::get(const std::string& id) const {
    auto it = bosses.find(id);
    if (it == bosses.end()) {
        throw std::runtime_error("Unknown boss: " + id);
    }
    return it->second;
}

bool BossRegistry::has(const std::string& id) const {
    return bosses.count(id) > 0;
}

std::vector<const BossDef*> BossRegistry::getAll() const {
    std::vector<const BossDef*> result;
    result.reserve(bosses.size());
    for (const auto& [_, def] : bosses) {
        result.push_back(&def);
    }
    std::sort(result.begin(), result.end(),
              [](const BossDef* a, const BossDef* b) { return a->id < b->id; });
    return result;
}
