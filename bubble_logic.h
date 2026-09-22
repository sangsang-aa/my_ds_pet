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

// Where to put a bubbleW x bubbleH bubble so its bottom-center tail points at
// the top-center of the pet, offset by gapPx. Prefers above the pet; if that
// would clip the top of the screen, flips below. The result is clamped to
// [0, screenW - bubbleW] x [0, screenH - bubbleH].
inline Point PlaceBubble(const Rect& pet, int bubbleW, int bubbleH,
                         int screenW, int screenH, int gapPx) {
    int x = pet.x + (pet.w - bubbleW) / 2;
    int y = pet.y - bubbleH - gapPx;
    if (y < 0) {
        y = pet.y + pet.h + gapPx;
    }
    x = std::max(0, std::min(x, screenW - bubbleW));
    y = std::max(0, std::min(y, screenH - bubbleH));
    return Point{ x, y };
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
