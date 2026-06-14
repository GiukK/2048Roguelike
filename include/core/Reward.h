#pragma once

#include <functional>
#include <string>
#include <vector>

class GameRun;

// What is known about a just-won boss fight, handed to the reward-offer strategy
// so the offer can scale/branch on which boss fell and the run state. The
// malleable info seam (twin of MergeContext/AttackContext): add a field here and
// every call site keeps compiling — the strategy reads what it needs.
struct RewardContext {
    std::string bossDefId;  // which boss was defeated (read from the BossDefeated event)
    int ante = 1;           // the ante just cleared
    GameRun& run;           // run handle: registries, RNG, ownership queries
};

// One offered reward: display data + the grant it applies when chosen. Data +
// lambda, the ItemDef/CardDef shape — so a reward is NOT limited to items/cards;
// coins, a slot, a scar repair all fit this same struct later with no new option
// type and no change to the modal that shows them.
struct RewardOption {
    std::string id;          // identity, for debug/logging
    std::string textureId;   // icon shown in the picker
    std::string name;        // tooltip title
    std::string description; // tooltip body
    std::function<void(GameRun&)> apply;  // grants the reward to the run when picked
};

// Generates the options offered after a defeat (today: up to 3, drawn from cards
// and consumables). Swappable like ShopTileValueStrategy / BossForAnteStrategy,
// so reward CONTENT is policy, never control flow in the ante machine.
using RewardOfferStrategy = std::function<std::vector<RewardOption>(const RewardContext&)>;
