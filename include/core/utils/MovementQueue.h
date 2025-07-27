
#pragma once

#include <deque>

class Slot;

class MovementQueue {
public:
    void pushBack(Slot* slot);
    void pushFront(Slot* slot);
    void insertAfter(Slot* reference, Slot* newSlot);
    Slot* popFront();
    bool empty() const;
    void clear();

private:
    std::deque<Slot*> queue;
};
