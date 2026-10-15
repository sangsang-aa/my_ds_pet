// Pure geometry for dragging the pet: keep its window on screen. Free of
// <windows.h> so tests/ can run anywhere.
#ifndef PET_DRAG_LOGIC_H
#define PET_DRAG_LOGIC_H

namespace pet_drag {

// Clamp a w x h window's top-left so it stays fully inside an sw x sh screen.
inline void ClampToScreen(int& x, int& y, int w, int h, int sw, int sh) {
    int maxX = sw - w;
    int maxY = sh - h;
    if (maxX < 0) {
        maxX = 0;
    }
    if (maxY < 0) {
        maxY = 0;
    }
    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }
    if (x > maxX) {
        x = maxX;
    }
    if (y > maxY) {
        y = maxY;
    }
}

}  // namespace pet_drag

#endif  // PET_DRAG_LOGIC_H
