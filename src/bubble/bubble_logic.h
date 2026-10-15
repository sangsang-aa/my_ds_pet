// Pure layout/scheduling logic for the desktop-pet speech bubble.
//
// Kept free of <windows.h> so it can be unit-tested on any host with a plain
// C++ compiler (tests/bubble_logic_test.cpp); main.cpp includes it and feeds in
// Win32 ints.
#ifndef PET_BUBBLE_LOGIC_H
#define PET_BUBBLE_LOGIC_H

#include <algorithm>

namespace pet_bubble {

struct Rect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

struct Point {
    int x = 0;
    int y = 0;
};

enum class Side { Above, Below, Right, Left };

struct Placement {
    Side side = Side::Above;
    Point at;
};

inline int clampX(int v, int winW, int screenW) {
    return std::max(0, std::min(v, screenW - winW));
}

inline int clampY(int v, int winH, int screenH) {
    return std::max(0, std::min(v, screenH - winH));
}

// Where to put the whole (body + tail) bubble. `bodyW`/`bodyH` are the rounded
// body; the window footprint is (bodyW+tailPx) x (bodyH+tailPx) for every side,
// with the tail on one edge and the body inset by tailPx on that edge.
// Prefers above; with no room above it sits beside the head (right, else left),
// vertically centred on the head band; below is the last resort.
inline Placement PlaceBubble(const Rect& pet, int bodyW, int bodyH, int tailPx,
                             int screenW, int screenH, int gapPx,
                             int headPercent) {
    const int winW = bodyW + tailPx;
    const int winH = bodyH + tailPx;
    const int cx = pet.x + (pet.w - bodyW) / 2;  // centres the tail on the pet
    // 1) above (tail at the bottom)
    int y = pet.y - winH - gapPx;
    if (y >= 0) {
        return Placement{ Side::Above,
                          Point{ clampX(cx, winW, screenW),
                                 clampY(y, winH, screenH) } };
    }
    // 2) beside the head (tail on the facing edge), centred on the head band
    const int headH = pet.h * headPercent / 100;
    const int besideY = pet.y + (headH - bodyH) / 2;
    int x = pet.x + pet.w + gapPx;
    if (x + winW <= screenW) {
        return Placement{ Side::Right,
                          Point{ x, clampY(besideY, winH, screenH) } };
    }
    x = pet.x - winW - gapPx;
    if (x >= 0) {
        return Placement{ Side::Left,
                          Point{ x, clampY(besideY, winH, screenH) } };
    }
    // 3) last resort: below (tail at the top)
    return Placement{ Side::Below,
                      Point{ clampX(cx, winW, screenW),
                             clampY(pet.y + pet.h + gapPx, winH, screenH) } };
}

// Number of ticks until the next bubble. `rand01` must be uniform in [0, 1).
// Returns minTicks when the range is empty or inverted.
inline int NextIntervalTicks(int minTicks, int maxTicks, double rand01) {
    if (maxTicks <= minTicks) {
        return minTicks;
    }
    const int span = maxTicks - minTicks + 1;
    int offset = static_cast<int>(rand01 * static_cast<double>(span));
    if (offset < 0) {
        offset = 0;
    }
    if (offset >= span) {
        offset = span - 1;
    }
    return minTicks + offset;
}

}  // namespace pet_bubble

#endif  // PET_BUBBLE_LOGIC_H
