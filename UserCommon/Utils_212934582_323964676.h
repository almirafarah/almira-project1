// Utils.h - Direction utilities shared between projects
#ifndef USERCOMMON_UTILS_212934582_323964676_H
#define USERCOMMON_UTILS_212934582_323964676_H

#include <utility>
#include <array>

namespace UserCommon_212934582_323964676 {

// Represents one of the eight cardinal and intercardinal directions, using up/down/left/right terminology
enum class Direction : int {
    UP = 0,
    UP_RIGHT,
    RIGHT,
    DOWN_RIGHT,
    DOWN,
    DOWN_LEFT,
    LEFT,
    UP_LEFT
};

// Utility functions for rotating and converting Direction values
class DirectionUtils {
public:
    // Rotate by N * 45° steps (positive = clockwise, negative = counterclockwise)
    static Direction rotate45(Direction d, int steps) {
        int v = (static_cast<int>(d) + steps) % 8;
        if (v < 0) v += 8;
        return static_cast<Direction>(v);
    }

    // Rotate by 90° (clockwise if cw=true, else counterclockwise)
    static Direction rotate90(Direction d, bool cw = true) {
        return rotate45(d, cw ? 2 : -2);
    }

    // Rotate by 45° (clockwise if cw=true, else counterclockwise)
    static Direction rotate45cw(Direction d) { 
        return rotate45(d, 1); }
    static Direction rotate45ccw(Direction d) { 
        return rotate45(d, -1); }
    // rotate by 180° helper
    static Direction rotate180(Direction d) {
        return rotate45(d, +4);
    }

    // Convert Direction to a (dx, dy) vector: x is row (down-positive), y is col (right-positive)
    static std::pair<int,int> toVector(Direction d) {
        static constexpr std::array<std::pair<int,int>, 8> deltas = {{
            {-1,  0}, // UP
            {-1,  1}, // UP_RIGHT
            { 0,  1}, // RIGHT
            { 1,  1}, // DOWN_RIGHT
            { 1,  0}, // DOWN
            { 1, -1}, // DOWN_LEFT
            { 0, -1}, // LEFT
            {-1, -1}  // UP_LEFT
        }};
        return deltas[static_cast<int>(d)];
    }
};

} // namespace UserCommon_212934582_323964676

#endif // USERCOMMON_UTILS_212934582_323964676_H
