#pragma once

#include <deque>
#include <algorithm>

class Slot;

class MovementQueue {
public:
    void pushBack(Slot* slot);
    void pushFront(Slot* slot);
    void insertAfter(Slot* reference, Slot* toInsert);

    Slot* popFront();
    bool empty() const;
    void clear();
    std::size_t size() const;

private:
    std::deque<Slot*> queue;
};
