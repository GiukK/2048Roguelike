#pragma once

class Slot;

// The mutable outcome of a single coin gain. Every positive coin award flows
// through GameRun::addCoins, which threads this through the in-scope effects'
// onCoinsResolving() hooks (modifiers may scale/alter it) BEFORE the coins are
// applied and logged — the coin twin of MergeContext.
//
// Spending (negative amounts) deliberately does NOT flow through: a "×2 coins"
// chip must amplify income, not double the player's expenses.
struct CoinContext {
    Slot* source;  // slot the gain originates over; nullptr = no board origin
                   // (e.g. an item like Coin Bag), so slot/tile chips don't apply
    int   amount;  // MUTABLE: the coins to award (starts at the base amount)
};
