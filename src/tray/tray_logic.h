// Pure helpers for the tray icon and the scale dialog. Free of <windows.h> so
// tests/ can run anywhere.
#ifndef PET_TRAY_LOGIC_H
#define PET_TRAY_LOGIC_H

namespace tray_logic {

constexpr int kCommandResize = 1001;
constexpr int kCommandQuit = 1002;

inline int ClampScalePercent(int percent, int lo, int hi) {
    if (percent < lo) {
        return lo;
    }
    if (percent > hi) {
        return hi;
    }
    return percent;
}

inline double ScaleFactorFromPercent(int percent) {
    return percent / 100.0;
}

}  // namespace tray_logic

#endif  // PET_TRAY_LOGIC_H
