#pragma once

enum class Direction { None, Up, Down, Left, Right };

inline const char* dirToString(Direction dir) {
    switch (dir) {
    case Direction::Up:    return "Up";
    case Direction::Down:  return "Down";
    case Direction::Left:  return "Left";
    case Direction::Right: return "Right";
    default:               return "None";
    }
}
