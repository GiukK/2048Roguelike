#pragma once

#include <deque>

class Slot;

class MovementQueue {
public:
    void pushBack(Slot* slot);
    Slot* popFront();
    bool empty() const;
    void clear();
    std::size_t size() const;

private:
    std::deque<Slot*> queue;
};
