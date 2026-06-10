#include "core/utils/MovementQueue.h"

void MovementQueue::pushBack(Slot* slot) {
    queue.push_back(slot);
}

Slot* MovementQueue::popFront() {
    if (queue.empty()) return nullptr;
    Slot* front = queue.front();
    queue.pop_front();
    return front;
}

bool MovementQueue::empty() const {
    return queue.empty();
}

void MovementQueue::clear() {
    queue.clear();
}

std::size_t MovementQueue::size() const {
    return queue.size();
}
