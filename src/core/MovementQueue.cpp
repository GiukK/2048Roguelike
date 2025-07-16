#include "core/MovementQueue.h"

void MovementQueue::pushBack(Slot* slot) {
    queue.push_back(slot);
}

void MovementQueue::pushFront(Slot* slot) {
    queue.push_front(slot);
}

void MovementQueue::insertAfter(Slot* reference, Slot* newSlot) {
    auto it = std::find(queue.begin(), queue.end(), reference);
    if (it != queue.end()) {
        queue.insert(it + 1, newSlot);
    }
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
