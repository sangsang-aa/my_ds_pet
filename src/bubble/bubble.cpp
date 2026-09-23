#include "bubble.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "bubble_logic.h"
#include "core/assets.h"
#include "core/config.h"
#include "core/gdi.h"
#include "core/render.h"
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
void RenderBubble(Bubble& b, const std::wstring& emojiPath, bool tailUp) {
    if (b.dc == nullptr) {
        return;
    }
    gdi::Bitmap canvas(b.w, b.h, PixelFormat32bppARGB);
    if (canvas.GetLastStatus() != gdi::Ok) {
        return;
    }
    const int bodyW = b.w;
    const int bodyH = b.h - kBubbleTailH;
    const int bodyTop = tailUp ? kBubbleTailH : 0;
    {
        gdi::Graphics graphics(&canvas);
        graphics.SetSmoothingMode(gdi::SmoothingModeAntiAlias);
        graphics.SetInterpolationMode(gdi::InterpolationModeHighQualityBicubic);

        gdi::SolidBrush fill(gdi::Color(255, 255, 255, 255));
        gdi::Pen border(gdi::Color(255, 130, 130, 130), 1.0f);
        gdi::GraphicsPath body;
        AddRoundedRect(body, 0, bodyTop, bodyW - 1, bodyH - 1, kBubbleRadiusPx);
        graphics.FillPath(&fill, &body);
        graphics.DrawPath(&border, &body);

        const int cx = bodyW / 2;
        gdi::Point tail[3];
        if (tailUp) {
            tail[0] = gdi::Point(cx - kBubbleTailW / 2, bodyTop + 1);
            tail[1] = gdi::Point(cx + kBubbleTailW / 2, bodyTop + 1);
            tail[2] = gdi::Point(cx, 0);
        } else {
            tail[0] = gdi::Point(cx - kBubbleTailW / 2, bodyTop + bodyH - 2);
            tail[1] = gdi::Point(cx + kBubbleTailW / 2, bodyTop + bodyH - 2);
            tail[2] = gdi::Point(cx, b.h - 1);
        }
        graphics.FillPolygon(&fill, tail, 3);
        graphics.DrawLine(&border, tail[0].X, tail[0].Y, tail[2].X, tail[2].Y);
        graphics.DrawLine(&border, tail[1].X, tail[1].Y, tail[2].X, tail[2].Y);

        gdi::Bitmap emoji(emojiPath.c_str());
        if (emoji.GetLastStatus() == gdi::Ok) {
            const int emojiW = std::max(1, static_cast<int>(emoji.GetWidth()));
            const int emojiH = std::max(1, static_cast<int>(emoji.GetHeight()));
            const double scale = std::min(
                static_cast<double>(kBubbleEmojiSizePx) / emojiW,
                static_cast<double>(kBubbleEmojiSizePx) / emojiH);
            const int drawW =
                std::max(1, static_cast<int>(std::lround(emojiW * scale)));
            const int drawH =
                std::max(1, static_cast<int>(std::lround(emojiH * scale)));
            graphics.DrawImage(&emoji, (bodyW - drawW) / 2,
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
    b.tailUp = tailUp;
    b.shownEmoji = emojiPath;
}

void PresentBubble(Bubble& b, int x, int y) {
    render::PresentLayered(b.hwnd, b.dc, b.hbmp, x, y, b.w, b.h, true);
}

pet_bubble::Rect BubblePetRect(const Bubble& b) {
    RECT rect = {};
    GetWindowRect(b.petHwnd, &rect);
    return pet_bubble::Rect{ rect.left, rect.top, rect.right - rect.left,
                             rect.bottom - rect.top };
}

void FollowBubble(Bubble& b) {
    if (!b.visible || b.hwnd == nullptr) {
        return;
    }
    const pet_bubble::Placement place = pet_bubble::PlaceBubble(
        BubblePetRect(b), b.w, b.h, GetSystemMetrics(SM_CXSCREEN),
        GetSystemMetrics(SM_CYSCREEN), kBubbleGapPx);
    const bool tailUp = place.side == pet_bubble::Side::Below;
    if (tailUp != b.tailUp && !b.shownEmoji.empty()) {
        RenderBubble(b, b.shownEmoji, tailUp);
    }
    PresentBubble(b, place.at.x, place.at.y);
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
    return pet_bubble::NextIntervalTicks(kBubbleMinTicks, kBubbleMaxTicks,
                                         dist(Rng()));
}

void ShowBubble(Bubble& b) {
    if (b.emojiPaths.empty() || b.hwnd == nullptr) {
        return;
    }
    const int pick = RandomInRange(0, static_cast<int>(b.emojiPaths.size()) - 1);
    const std::wstring& emoji = b.emojiPaths[static_cast<size_t>(pick)];
    const pet_bubble::Placement place = pet_bubble::PlaceBubble(
        BubblePetRect(b), b.w, b.h, GetSystemMetrics(SM_CXSCREEN),
        GetSystemMetrics(SM_CYSCREEN), kBubbleGapPx);
    RenderBubble(b, emoji, place.side == pet_bubble::Side::Below);
    PresentBubble(b, place.at.x, place.at.y);
    ShowWindow(b.hwnd, SW_SHOWNOACTIVATE);
    b.visible = true;
    b.showLeft = kBubbleShowTicks;
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

    w = kBubbleEmojiSizePx + 2 * kBubblePadPx;
    h = kBubbleEmojiSizePx + 2 * kBubblePadPx + kBubbleTailH;
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
