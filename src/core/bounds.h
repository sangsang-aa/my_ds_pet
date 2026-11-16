// The subject's rectangle in screen coordinates. The pet publishes it every
// frame so the speech bubble and the head-sweep band follow the character
// instead of the (larger) canvas frame it sits in.
#ifndef PET_BOUNDS_H
#define PET_BOUNDS_H

#include <cmath>

struct SubjectBounds {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    bool valid = false;
};

// Convert a subject box from bounds.txt (source-canvas coordinates, e.g.
// 384x512) to the scaled frame the window displays (e.g. 269x358), then clamp
// it into that frame. Returns valid=false for an empty or fully off-frame box.
// Kept inline and free of <windows.h> so tests can cover it anywhere.
inline SubjectBounds ScaleSubjectBounds(int bx, int by, int bw, int bh,
                                        double scale, int frameW, int frameH) {
    if (bw <= 0 || bh <= 0 || scale <= 0.0 || frameW <= 0 || frameH <= 0) {
        return SubjectBounds{};
    }
    int x = static_cast<int>(std::lround(static_cast<double>(bx) * scale));
    int y = static_cast<int>(std::lround(static_cast<double>(by) * scale));
    int w = static_cast<int>(std::lround(static_cast<double>(bw) * scale));
    int h = static_cast<int>(std::lround(static_cast<double>(bh) * scale));
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x >= frameW || y >= frameH || w <= 0 || h <= 0) {
        return SubjectBounds{};
    }
    if (x + w > frameW) {
        w = frameW - x;
    }
    if (y + h > frameH) {
        h = frameH - y;
    }
    if (w <= 0 || h <= 0) {
        return SubjectBounds{};
    }
    return SubjectBounds{ x, y, w, h, true };
}

void SetSubjectBounds(const SubjectBounds& bounds);
SubjectBounds GetSubjectBounds();

#endif  // PET_BOUNDS_H
