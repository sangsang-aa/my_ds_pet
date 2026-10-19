// Pure stroke recognition for the head-sweep ("petting the head") gesture.
// Free of <windows.h> so tests/ can run anywhere.
#ifndef PET_SWEEP_LOGIC_H
#define PET_SWEEP_LOGIC_H

namespace pet_sweep {

struct State {
    int accum = 0;      // horizontal travel of the current stroke
    int dir = 0;        // -1 left, +1 right, 0 unknown
    int passes = 0;     // strokes completed so far
    int idleTicks = 0;  // ticks since the last accepted movement
};

// Feed one horizontal delta; the caller has already checked that it is inside
// the head band and mostly horizontal. A stroke counts when the direction
// reverses after at least `thresholdPx` of travel, so a single long sweep never
// counts. Returns true once `passesNeeded` strokes are done.
inline bool Feed(State& s, int dx, int thresholdPx, int passesNeeded) {
    if (dx == 0) {
        return false;
    }
    s.idleTicks = 0;
    const int dir = dx > 0 ? 1 : -1;
    if (s.dir != 0 && dir != s.dir && s.accum >= thresholdPx) {
        s.passes += 1;
    }
    if (dir != s.dir) {
        s.accum = 0;
        s.dir = dir;
    }
    s.accum += dx > 0 ? dx : -dx;
    if (s.passes >= passesNeeded) {
        s = State{};
        return true;
    }
    return false;
}

inline int HeadBandTop(int top, int height, int topPercent) {
    return top + height * topPercent / 100;
}

// Bottom edge of the head band: the top `bottomPercent` of the subject minus a
// proportional inset, and never above the band's own top edge. The inset is a
// percentage of the subject so the band keeps the same shape at every scale.
inline int HeadBandBottom(int top, int height, int topPercent,
                          int bottomPercent, int insetPercent) {
    const int bandTop = top + height * topPercent / 100;
    int bottom = top + height * bottomPercent / 100 -
                 height * insetPercent / 100;
    if (bottom < bandTop) {
        bottom = bandTop;
    }
    return bottom;
}

// Once per pet tick: decay a half-finished stroke and drop a gesture that has
// been idle for longer than `resetTicks`, so two casual strokes minutes apart
// do not add up.
inline void Idle(State& s, int resetTicks) {
    s.idleTicks += 1;
    s.accum -= s.accum / 2;
    if (s.idleTicks > resetTicks) {
        s = State{};
    }
}

}  // namespace pet_sweep

#endif  // PET_SWEEP_LOGIC_H
