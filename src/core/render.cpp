#include "render.h"

#include "gdi.h"

GdiplusSession::GdiplusSession() {
    gdi::GdiplusStartupInput input;
    const gdi::Status status = gdi::GdiplusStartup(&token_, &input, nullptr);
    ok_ = (status == gdi::Ok);
}

GdiplusSession::~GdiplusSession() {
    if (ok_) {
        gdi::GdiplusShutdown(token_);
    }
}

bool GdiplusSession::ok() const {
    return ok_;
}

namespace render {

void PresentLayered(HWND hwnd, HDC memDC, HBITMAP hbmp, int x, int y, int w,
                    int h, bool perPixelAlpha) {
    if (memDC == nullptr || hbmp == nullptr) {
        return;
    }
    POINT destPos = { x, y };
    SIZE size = { w, h };
    POINT srcPos = { 0, 0 };

    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.BlendFlags = 0;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = perPixelAlpha ? AC_SRC_ALPHA : 0;

    const HGDIOBJ previous = SelectObject(memDC, hbmp);
    UpdateLayeredWindow(hwnd, nullptr, &destPos, &size, memDC, &srcPos, 0, &blend,
                        ULW_ALPHA);
    SelectObject(memDC, previous);
}

}  // namespace render
