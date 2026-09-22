// Pure walk-cycle logic, kept free of <windows.h> so it can be unit-tested on
// any host (tests/walk_logic_test.cpp).
#ifndef PET_WALK_LOGIC_H
#define PET_WALK_LOGIC_H

namespace pet_walk {

// Whether `index` is an animation boundary (first or last frame) — a pose that
// is safe to leave a one-way walk clip from. Size <= 1 is always a boundary.
inline bool AtCycleBoundary(int index, int size) {
    return size <= 1 || index <= 0 || index >= size - 1;
}

}  // namespace pet_walk

#endif  // PET_WALK_LOGIC_H
