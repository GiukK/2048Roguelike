#pragma once

struct Coord {
    int x, y;

    bool operator<(const Coord& other) const {
        return (x < other.x) || (x == other.x && y < other.y);
    }
};
