#pragma once

class Slot;

// The mutable outcome of a single merge. It is threaded through the in-scope
// effects' onMergeResolving() hooks (modifiers may change it) BEFORE the merge is
// applied and logged — this is the spine that makes a merge modifiable by chips
// and special slots, instead of a fixed "value doubles" rule.
//
// Grows as more outcome knobs are needed (e.g. a coin reward, a "destroy the
// resulting tile" flag) — those land with the chips that use them, to avoid
// carrying inert fields. See docs/effect-engine-design.md §4.
struct MergeContext {
    Slot* slot;        // the slot the merge resolves on (carries its effects/chips)
    int   sourceValue; // pre-merge value of each merging tile (immutable input)
    int   resultValue; // MUTABLE: the merged tile's value (default = the two summed).
                       // A modifier that changes it must keep it a real, texture-
                       // backed tile value (2..2048).
};
