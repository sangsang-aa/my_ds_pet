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

}  // namespace pet_walk

#endif  // PET_WALK_LOGIC_H
