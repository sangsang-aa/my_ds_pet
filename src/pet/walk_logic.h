// Pure walk-cycle logic, kept free of <windows.h> so it can be unit-tested on
// any host (tests/walk_logic_test.cpp).
#ifndef PET_WALK_LOGIC_H
#define PET_WALK_LOGIC_H

namespace pet_walk {

// Whether `index` is the last frame of the clip — the grounded pose a one-way
// vertical walk must reach before returning to idle (leaving mid-air, or at the
// pre-takeoff frame, would snap the pet). Size <= 1 is always the end.
inline bool AtEndFrame(int index, int size) {
    return size <= 1 || index >= size - 1;
}

// Whether `index` is either end of the clip (first or last frame) — the poses a
// boomerang clip (forward then backward) is safe to leave from.
// A boomerang clip plays forward to the last frame and back again, so it may
// only stop once it is back on its first frame; stopping at the far end would
// leave the subject mid-air with the backward half never shown.
inline bool AtStartFrame(int index) {
    return index <= 0;
}

}  // namespace pet_walk

#endif  // PET_WALK_LOGIC_H
