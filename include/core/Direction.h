#pragma once

enum class Direction {
    None,  // Represents an unset or no move state
    Up,
    Down,
    Left,
    Right
};

// Converts a Direction to its string representation
inline const char* toString(Direction dir) {
    switch (dir) {
    case Direction::None:  return "None";
    case Direction::Up:    return "Up";
    case Direction::Down:  return "Down";
    case Direction::Left:  return "Left";
    case Direction::Right: return "Right";
    default:               return "Unknown";
    }
}
