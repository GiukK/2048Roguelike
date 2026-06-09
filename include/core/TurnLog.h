#pragma once

#include <algorithm>
#include <vector>

#include "core/TurnEvent.h"

// The ordered list of events that happened during ONE turn. Owned by Turn: it is
// created empty with each new turn and reset on replay (Turn::endTurn), so it is
// never carried into the next turn's board clone. That is what guarantees the
// invariant from the project's design notes — an event always belongs to the
// turn it was emitted in, and never "re-happens" in the following turn.
//
// Consumers (score, passive abilities) read events() or use the aggregate
// queries below instead of re-walking the vector with ad-hoc loops. Add more
// queries here as abilities need them — this class is the extension point.
class TurnLog {
public:
    void push(TurnEvent event) { events_.push_back(std::move(event)); }
    void clear() { events_.clear(); }

    const std::vector<TurnEvent>& events() const { return events_; }
    bool empty() const { return events_.empty(); }

    int countOf(TurnEvent::Type type) const {
        int n = 0;
        for (const auto& e : events_) {
            if (e.type == type) ++n;
        }
        return n;
    }

    int mergeCount() const { return countOf(TurnEvent::Type::TileMerged); }

    // Highest resulting value among this turn's merges (0 if none). Useful for a
    // future "biggest merge" score bonus.
    int highestMergeValue() const {
        int best = 0;
        for (const auto& e : events_) {
            if (e.type == TurnEvent::Type::TileMerged) best = std::max(best, e.valueA);
        }
        return best;
    }

private:
    std::vector<TurnEvent> events_;
};
