#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "core/Boss.h"

// The boss catalogue, mirroring CardRegistry (boss-design §5): mechanism in
// core, policy in content/BaseContent. The ante machine (slice 4) will pick
// from here; today the debug spawn key reads it directly.
class BossRegistry {
public:
    void registerBoss(BossDef def);
    const BossDef& get(const std::string& id) const;
    bool has(const std::string& id) const;

    // Every registered boss, sorted by id — deterministic order for future
    // ante picks, like the other registries' getAll.
    std::vector<const BossDef*> getAll() const;

private:
    std::unordered_map<std::string, BossDef> bosses;
};
