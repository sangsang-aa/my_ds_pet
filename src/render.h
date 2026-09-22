// Layered-window presentation and the GDI+ lifetime guard.
#ifndef PET_RENDER_H
#define PET_RENDER_H

#include <windows.h>

// RAII wrapper for the GdiplusStartup / GdiplusShutdown pair.
class GdiplusSession {
public:
    GdiplusSession();
    ~GdiplusSession();
    GdiplusSession(const GdiplusSession&) = delete;
    GdiplusSession& operator=(const GdiplusSession&) = delete;
    bool ok() const;

private:
    ULONG_PTR token_ = 0;
    bool ok_ = false;
};

namespace render {

// Composite an HBITMAP into a layered window via UpdateLayeredWindow, placing
// its top-left at (x, y). `perPixelAlpha` selects AC_SRC_ALPHA blending (RGBA
// frames blend per-pixel; RGB frames are drawn opaque via SourceConstantAlpha,
// otherwise their zeroed alpha bytes would render them fully transparent).
void PresentLayered(HWND hwnd, HDC memDC, HBITMAP hbmp, int x, int y, int w,
                    int h, bool perPixelAlpha);

}  // namespace render

#endif  // PET_RENDER_H
