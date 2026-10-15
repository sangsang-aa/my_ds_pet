#include "bubble.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "bubble_logic.h"
#include "core/assets.h"
#include "core/bounds.h"
#include "core/config.h"
#include "core/gdi.h"
#include "core/render.h"
#include "core/settings.h"
#include "core/util.h"

namespace {

void AddRoundedRect(gdi::GraphicsPath& path, int x, int y, int w, int h,
                    int radius) {
    const int d = radius * 2;
    path.AddArc(x, y, d, d, 180.0f, 90.0f);
    path.AddArc(x + w - d, y, d, d, 270.0f, 90.0f);
    path.AddArc(x + w - d, y + h - d, d, d, 0.0f, 90.0f);
    path.AddArc(x, y + h - d, d, d, 90.0f, 90.0f);
    path.CloseFigure();
}

// Render the bubble (rounded body + tail + emoji) into b.hbmp.
void RenderBubble(Bubble& b, const std::wstring& emojiPath,
                  pet_bubble::Side side) {
    if (b.dc == nullptr) {
        return;
    }
    gdi::Bitmap canvas(b.w, b.h, PixelFormat32bppARGB);
    if (canvas.GetLastStatus() != gdi::Ok) {
        return;
    }
    const int tailH = GetSettings().bubbleTailH;
    const int bodyW = b.w - tailH;
    const int bodyH = b.h - tailH;
    int bodyLeft = 0;
    int bodyTop = 0;
    if (side == pet_bubble::Side::Below) {
        bodyTop = tailH;  // tail strip on the top edge
    } else if (side == pet_bubble::Side::Right) {
        bodyLeft = tailH;  // tail strip on the left edge
    }
    {
        gdi::Graphics graphics(&canvas);
        graphics.SetSmoothingMode(gdi::SmoothingModeAntiAlias);
        graphics.SetInterpolationMode(gdi::InterpolationModeHighQualityBicubic);

        gdi::SolidBrush fill(gdi::Color(255, 255, 255, 255));
        gdi::Pen border(gdi::Color(255, 130, 130, 130), 1.0f);
        gdi::GraphicsPath body;
        AddRoundedRect(body, bodyLeft, bodyTop, bodyW - 1, bodyH - 1,
                       GetSettings().bubbleRadiusPx);
        graphics.FillPath(&fill, &body);
        graphics.DrawPath(&border, &body);

        const int tailW = GetSettings().bubbleTailW;
        const int cx = bodyW / 2;
        gdi::Point tail[3];
        switch (side) {
        case pet_bubble::Side::Above:
            tail[0] = gdi::Point(bodyLeft + cx - tailW / 2,
                                 bodyTop + bodyH - 2);
            tail[1] = gdi::Point(bodyLeft + cx + tailW / 2,
                                 bodyTop + bodyH - 2);
            tail[2] = gdi::Point(bodyLeft + cx, b.h - 1);
            break;
        case pet_bubble::Side::Below:
            tail[0] = gdi::Point(bodyLeft + cx - tailW / 2, bodyTop + 1);
            tail[1] = gdi::Point(bodyLeft + cx + tailW / 2, bodyTop + 1);
            tail[2] = gdi::Point(bodyLeft + cx, 0);
            break;
        case pet_bubble::Side::Right:
            tail[0] = gdi::Point(bodyLeft + 1,
                                 bodyTop + bodyH / 2 - tailW / 2);
            tail[1] = gdi::Point(bodyLeft + 1,
                                 bodyTop + bodyH / 2 + tailW / 2);
            tail[2] = gdi::Point(0, bodyTop + bodyH / 2);
            break;
        case pet_bubble::Side::Left:
            tail[0] = gdi::Point(bodyLeft + bodyW - 2,
                                 bodyTop + bodyH / 2 - tailW / 2);
            tail[1] = gdi::Point(bodyLeft + bodyW - 2,
                                 bodyTop + bodyH / 2 + tailW / 2);
            tail[2] = gdi::Point(b.w - 1, bodyTop + bodyH / 2);
            break;
        }
        graphics.FillPolygon(&fill, tail, 3);
        graphics.DrawLine(&border, tail[0].X, tail[0].Y, tail[2].X, tail[2].Y);
        graphics.DrawLine(&border, tail[1].X, tail[1].Y, tail[2].X, tail[2].Y);

        gdi::Bitmap emoji(emojiPath.c_str());
        if (emoji.GetLastStatus() == gdi::Ok) {
            const int emojiW = std::max(1, static_cast<int>(emoji.GetWidth()));
            const int emojiH = std::max(1, static_cast<int>(emoji.GetHeight()));
            const double scale = std::min(
                static_cast<double>(GetSettings().bubbleEmojiSizePx) / emojiW,
                static_cast<double>(GetSettings().bubbleEmojiSizePx) / emojiH);
            const int drawW =
                std::max(1, static_cast<int>(std::lround(emojiW * scale)));
            const int drawH =
                std::max(1, static_cast<int>(std::lround(emojiH * scale)));
            graphics.DrawImage(&emoji, bodyLeft + (bodyW - drawW) / 2,
                               bodyTop + (bodyH - drawH) / 2, drawW, drawH);
        }
    }
    HBITMAP hbmp = nullptr;
    if (canvas.GetHBITMAP(gdi::Color(0, 0, 0, 0), &hbmp) == gdi::Ok &&
        hbmp != nullptr) {
        if (b.hbmp != nullptr) {
            DeleteObject(b.hbmp);
        }
        b.hbmp = hbmp;
    }
    b.side = side;
    b.shownEmoji = emojiPath;
}

void PresentBubble(Bubble& b, int x, int y) {
    render::PresentLayered(b.hwnd, b.dc, b.hbmp, x, y, b.w, b.h, true);
}

pet_bubble::Rect BubblePetRect(const Bubble& b) {
    const SubjectBounds bounds = GetSubjectBounds();
    if (bounds.valid && bounds.w > 0 && bounds.h > 0) {
        return pet_bubble::Rect{ bounds.x, bounds.y, bounds.w, bounds.h };
    }
    RECT rect = {};
    GetWindowRect(b.petHwnd, &rect);
    return pet_bubble::Rect{ rect.left, rect.top, rect.right - rect.left,
                             rect.bottom - rect.top };
}

// The pet re-asserts topmost about once a second, which can land it above its
// owned bubble. Walks the z-order upward from the pet: the bubble is still on
// top when we reach it before the end of the chain.
bool BubbleIsBehindPet(HWND bubble, HWND pet) {
    if (bubble == nullptr || pet == nullptr) {
        return false;
    }
    for (HWND w = GetWindow(pet, GW_HWNDPREV); w != nullptr;
         w = GetWindow(w, GW_HWNDPREV)) {
        if (w == bubble) {
            return false;
        }
    }
    return true;
}

void FollowBubble(Bubble& b) {
    if (!b.visible || b.hwnd == nullptr) {
        return;
    }
    const int tailH = GetSettings().bubbleTailH;
    const pet_bubble::Placement place = pet_bubble::PlaceBubble(
        BubblePetRect(b), b.w - tailH, b.h - tailH, tailH,
        GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
        GetSettings().bubbleGapPx, GetSettings().headBandPercent);
    if (place.side != b.side && !b.shownEmoji.empty()) {
        RenderBubble(b, b.shownEmoji, place.side);
    }
    PresentBubble(b, place.at.x, place.at.y);
    if (BubbleIsBehindPet(b.hwnd, b.petHwnd)) {
        SetWindowPos(b.hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

void HideBubble(Bubble& b) {
    if (b.hwnd != nullptr) {
        ShowWindow(b.hwnd, SW_HIDE);
    }
    b.visible = false;
}

int NextBubbleInterval(Bubble& b) {
    (void)b;
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return pet_bubble::NextIntervalTicks(GetSettings().bubbleMinTicks, GetSettings().bubbleMaxTicks,
                                         dist(Rng()));
}

void ShowBubble(Bubble& b) {
    if (b.emojiPaths.empty() || b.hwnd == nullptr) {
        return;
    }
    const int pick = RandomInRange(0, static_cast<int>(b.emojiPaths.size()) - 1);
    const std::wstring& emoji = b.emojiPaths[static_cast<size_t>(pick)];
    const int tailH = GetSettings().bubbleTailH;
    const pet_bubble::Placement place = pet_bubble::PlaceBubble(
        BubblePetRect(b), b.w - tailH, b.h - tailH, tailH,
        GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
        GetSettings().bubbleGapPx, GetSettings().headBandPercent);
    RenderBubble(b, emoji, place.side);
    PresentBubble(b, place.at.x, place.at.y);
    ShowWindow(b.hwnd, SW_SHOWNOACTIVATE);
    b.visible = true;
    b.showLeft = GetSettings().bubbleShowTicks;
}

}  // namespace

bool Bubble::Init(HINSTANCE hInstance, HWND petWindow,
                  const std::wstring& exeDir, const std::wstring& assetsDir) {
    emojiPaths = LoadEmojiPaths(exeDir, assetsDir);
    if (emojiPaths.empty()) {
        std::printf("[pet] no emoji stickers, bubble disabled\n");
        return false;
    }
    std::printf("[pet] emoji loaded=%zu stickers\n", emojiPaths.size());
    petHwnd = petWindow;

    WNDCLASSW bubbleClass = {};
    bubbleClass.style = CS_HREDRAW | CS_VREDRAW;
    bubbleClass.lpfnWndProc = BubbleWndProc;
    bubbleClass.hInstance = hInstance;
    bubbleClass.hCursor =
        LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    bubbleClass.lpszClassName = kBubbleClassName;
    if (RegisterClassW(&bubbleClass) == 0) {
        return false;
    }

    w = GetSettings().bubbleEmojiSizePx + 2 * GetSettings().bubblePadPx +
        GetSettings().bubbleTailH;
    h = w;
    hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE |
            WS_EX_TRANSPARENT,
        kBubbleClassName, L"Desktop Pet Bubble", WS_POPUP, 0, 0, w, h, petHwnd,
        nullptr, hInstance, this);
    if (hwnd == nullptr) {
        std::printf("[pet] bubble window create failed, bubble disabled\n");
        return false;
    }
    SetTimer(hwnd, kBubbleTimerId, kBubbleTimerElapseMs, nullptr);
    countdown = NextBubbleInterval(*this);
    std::printf("[pet] bubble enabled (%dx%d, %zu stickers)\n", w, h,
                emojiPaths.size());
    return true;
}

void Bubble::OnTimer() {
    if (visible) {
        showLeft -= 1;
        if (showLeft <= 0) {
            HideBubble(*this);
            countdown = NextBubbleInterval(*this);
        } else {
            FollowBubble(*this);
        }
    } else {
        countdown -= 1;
        if (countdown <= 0) {
            ShowBubble(*this);
        }
    }
}

void Bubble::OnDestroy() {
    if (dc != nullptr) {
        DeleteDC(dc);
        dc = nullptr;
    }
    if (hbmp != nullptr) {
        DeleteObject(hbmp);
        hbmp = nullptr;
    }
    hwnd = nullptr;
}

LRESULT CALLBACK BubbleWndProc(HWND hwnd, UINT message, WPARAM wParam,
                               LPARAM lParam) {
    Bubble* bubble = reinterpret_cast<Bubble*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (message) {
    case WM_CREATE: {
        CREATESTRUCTW* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        bubble = static_cast<Bubble*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(bubble));
        if (bubble != nullptr) {
            bubble->dc = CreateCompatibleDC(nullptr);
        }
        return 0;
    }

    case WM_TIMER:
        if (wParam == kBubbleTimerId && bubble != nullptr) {
            bubble->OnTimer();
        }
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT paint = {};
        BeginPaint(hwnd, &paint);
        EndPaint(hwnd, &paint);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;  // layered window: there is no background to erase

    case WM_DESTROY:
        if (bubble != nullptr) {
            bubble->OnDestroy();
        }
        return 0;

    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}
