#pragma once

struct Coord {
    int x, y;

    //make Coord comparable so it can be used as a key in the map
    bool operator<(const Coord& other) const {
        return (x < other.x) || (x == other.x && y < other.y);
    }


};
