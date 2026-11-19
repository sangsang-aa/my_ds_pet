// System-tray icon and a horizontal, self-drawn toolbar for the desktop pet.
#include "tray.h"

#include <algorithm>
#include <cwchar>

#include "core/config.h"
#include "core/gdi.h"
#include "core/render.h"

namespace {

// One horizontally-laid-out button. `rect` is in bar client coordinates.
struct BarButton {
    UINT_PTR id;
    std::wstring label;
    RECT rect = {};
    bool enabled = true;
};

constexpr wchar_t kFontFamily[] = L"Microsoft YaHei";

// Load the first frame PNG directly so its alpha channel survives; wrapping an
// existing HBITMAP in a GDI+ Bitmap drops alpha on some GDI+ versions.
HICON IconFromFrame(const std::wstring& pngPath) {
    gdi::Bitmap source(pngPath.c_str());
    if (source.GetLastStatus() != gdi::Ok) {
        return nullptr;
    }
    constexpr int kIconSize = 32;
    gdi::Bitmap square(kIconSize, kIconSize, PixelFormat32bppARGB);
    if (square.GetLastStatus() != gdi::Ok) {
        return nullptr;
    }
    {
        gdi::Graphics graphics(&square);
        graphics.Clear(gdi::Color(0, 0, 0, 0));
        graphics.SetInterpolationMode(gdi::InterpolationModeHighQualityBicubic);
        const double sw = static_cast<double>(source.GetWidth());
        const double sh = static_cast<double>(source.GetHeight());
        if (sw <= 0.0 || sh <= 0.0) {
            return nullptr;
        }
        const double scale = std::min(kIconSize / sw, kIconSize / sh);
        const int w = std::max(1, static_cast<int>(sw * scale));
        const int h = std::max(1, static_cast<int>(sh * scale));
        graphics.DrawImage(&source, (kIconSize - w) / 2, (kIconSize - h) / 2,
                           w, h);
    }
    HICON icon = nullptr;
    if (square.GetHICON(&icon) != gdi::Ok || icon == nullptr) {
        return nullptr;
    }
    return icon;
}

// Build the four buttons and measure their widths with GDI+ so the text fits.
std::vector<BarButton> LayoutButtons(bool petVisible, gdi::Graphics& measure,
                                     gdi::Font& font, int barH) {
    std::vector<BarButton> buttons;
    buttons.push_back({ kTrayMenuToggle,
                        petVisible ? L"隐藏宠物" : L"显示宠物", {}, true });
    buttons.push_back({ kTrayMenuAbout, L"关于", {}, true });
    buttons.push_back({ kTrayMenuActivity, L"活动", {}, false });
    buttons.push_back({ kTrayMenuQuit, L"退出", {}, true });

    int x = 0;
    for (BarButton& button : buttons) {
        gdi::RectF bounds = {};
        measure.MeasureString(button.label.c_str(), -1, &font,
                              gdi::PointF(0.0f, 0.0f), &bounds);
        const int textW = static_cast<int>(bounds.Width) + 2;
        const int w = textW + 2 * kBarItemPadPx;
        button.rect.left = x;
        button.rect.top = 0;
        button.rect.right = x + w;
        button.rect.bottom = barH;
        x += w;
    }
    return buttons;
}

// Render the horizontal bar into a fresh 32bpp ARGB bitmap.
HBITMAP RenderBar(const std::vector<BarButton>& buttons, int barW, int barH) {
    gdi::Bitmap canvas(barW, barH, PixelFormat32bppARGB);
    if (canvas.GetLastStatus() != gdi::Ok) {
        return nullptr;
    }
    {
        gdi::Graphics graphics(&canvas);
        graphics.Clear(gdi::Color(0, 0, 0, 0));
        graphics.SetSmoothingMode(gdi::SmoothingModeAntiAlias);
        graphics.SetTextRenderingHint(gdi::TextRenderingHintAntiAlias);

        // Rounded white body with a soft grey border.
        gdi::GraphicsPath body;
        const int d = kBarRadiusPx * 2;
        body.AddArc(0, 0, d, d, 180.0f, 90.0f);
        body.AddArc(barW - d, 0, d, d, 270.0f, 90.0f);
        body.AddArc(barW - d, barH - d, d, d, 0.0f, 90.0f);
        body.AddArc(0, barH - d, d, d, 90.0f, 90.0f);
        body.CloseFigure();
        gdi::SolidBrush fill(gdi::Color(245, 255, 255, 255));
        gdi::Pen border(gdi::Color(255, 150, 150, 150), 1.0f);
        graphics.FillPath(&fill, &body);
        graphics.DrawPath(&border, &body);

        gdi::Font font(kFontFamily, static_cast<gdi::REAL>(kBarFontSizePx),
                       gdi::FontStyleRegular, gdi::UnitPoint);
        gdi::StringFormat format;
        format.SetAlignment(gdi::StringAlignmentCenter);
        format.SetLineAlignment(gdi::StringAlignmentCenter);

        for (size_t i = 0; i < buttons.size(); ++i) {
            const BarButton& button = buttons[i];
            if (i > 0) {
                gdi::Pen sep(gdi::Color(220, 200, 200, 200), 1.0f);
                graphics.DrawLine(&sep, button.rect.left, 6, button.rect.left,
                                  barH - 6);
            }
            gdi::SolidBrush text(button.enabled
                                     ? gdi::Color(255, 40, 40, 40)
                                     : gdi::Color(255, 175, 175, 175));
            graphics.DrawString(
                button.label.c_str(), -1, &font,
                gdi::RectF(static_cast<gdi::REAL>(button.rect.left), 0.0f,
                           static_cast<gdi::REAL>(button.rect.right -
                                                  button.rect.left),
                           static_cast<gdi::REAL>(barH)),
                &format, &text);
        }
    }
    HBITMAP hbmp = nullptr;
    if (canvas.GetHBITMAP(gdi::Color(0, 0, 0, 0), &hbmp) != gdi::Ok ||
        hbmp == nullptr) {
        return nullptr;
    }
    return hbmp;
}

}  // namespace

bool Tray::Init(HINSTANCE hInstance, HWND petWindow,
                const std::wstring& firstFramePng,
                const TrayActions& trayActions, std::wstring& error) {
    instance = hInstance;
    petHwnd = petWindow;
    actions = trayActions;

    WNDCLASSW trayClass = {};
    trayClass.lpfnWndProc = TrayWndProc;
    trayClass.hInstance = hInstance;
    trayClass.hCursor =
        LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    trayClass.lpszClassName = kTrayClassName;
    if (RegisterClassW(&trayClass) == 0) {
        error = L"RegisterClassW for the tray window failed (error " +
                std::to_wstring(GetLastError()) + L").";
        return false;
    }

    // Message-only window: receives Shell_NotifyIcon callbacks.
    hwnd = CreateWindowExW(0, kTrayClassName, L"Desktop Pet Tray", 0, 0, 0, 0,
                           0, HWND_MESSAGE, nullptr, hInstance, this);
    if (hwnd == nullptr) {
        error = L"Creating the tray message window failed (error " +
                std::to_wstring(GetLastError()) + L").";
        return false;
    }
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    // Layered, topmost, no-activate toolbar window. It must receive clicks, so
    // it is NOT click-through.
    WNDCLASSW barClass = {};
    barClass.lpfnWndProc = BarWndProc;
    barClass.hInstance = hInstance;
    barClass.hCursor =
        LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    barClass.lpszClassName = kBarClassName;
    if (RegisterClassW(&barClass) == 0) {
        error = L"RegisterClassW for the bar window failed (error " +
                std::to_wstring(GetLastError()) + L").";
        return false;
    }
    barHwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kBarClassName, L"Desktop Pet Bar", WS_POPUP, 0, 0, 1, 1, nullptr,
        nullptr, hInstance, this);
    if (barHwnd == nullptr) {
        error = L"Creating the bar window failed (error " +
                std::to_wstring(GetLastError()) + L").";
        return false;
    }
    SetWindowLongPtrW(barHwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    barDC = CreateCompatibleDC(nullptr);
    if (barDC == nullptr) {
        error = L"CreateCompatibleDC for the bar failed.";
        return false;
    }

    icon = IconFromFrame(firstFramePng);
    if (icon != nullptr) {
        ownsIcon = true;
    } else {
        icon = LoadIconW(nullptr,
                         reinterpret_cast<LPCWSTR>(IDI_APPLICATION));
    }
    if (icon == nullptr) {
        error = L"Could not create a tray icon.";
        return false;
    }

    nid = {};
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = kTrayNotifyMsg;
    nid.hIcon = icon;
    wcsncpy(nid.szTip, L"桌面宠物", 127);
    nid.szTip[127] = L'\0';
    if (Shell_NotifyIconW(NIM_ADD, &nid) == FALSE) {
        error = L"Shell_NotifyIcon(NIM_ADD) failed (error " +
                std::to_wstring(GetLastError()) + L").";
        return false;
    }
    registered = true;
    return true;
}

// Anchor just below the pet, horizontally centred, clamped to the main screen.
void Tray::PlaceBar() {
    if (barHwnd == nullptr || barDC == nullptr || barBitmap == nullptr) {
        return;
    }
    RECT petRect = {};
    GetWindowRect(petHwnd, &petRect);
    int x = petRect.left + (petRect.right - petRect.left - barW) / 2;
    int y = petRect.bottom + kBarGapPx;
    const int screenW = GetSystemMetrics(SM_CXSCREEN);
    const int screenH = GetSystemMetrics(SM_CYSCREEN);
    x = std::clamp(x, 0, std::max(0, screenW - barW));
    y = std::clamp(y, 0, std::max(0, screenH - barH));
    render::PresentLayered(barHwnd, barDC, barBitmap, x, y, barW, barH, true);
}

// Keep an open bar glued under a moving pet (drag or autonomous walk), so it
// never gets stranded at the old spot when the pet is dragged away.
void Tray::FollowPet() {
    if (barShown) {
        PlaceBar();
    }
}

void Tray::ShowBar() {
    if (barHwnd == nullptr || barDC == nullptr) {
        return;
    }
    if (barShown) {
        HideBar();  // trigger again toggles the bar closed
        return;
    }

    barH = kBarHeightPx;
    std::vector<BarButton> buttons;
    {
        gdi::Bitmap probe(1, 1, PixelFormat32bppARGB);
        gdi::Graphics measure(&probe);
        gdi::Font font(kFontFamily, static_cast<gdi::REAL>(kBarFontSizePx),
                       gdi::FontStyleRegular, gdi::UnitPoint);
        buttons = LayoutButtons(petVisible, measure, font, barH);
    }
    barW = 2 * kBarItemPadPx;
    for (const BarButton& button : buttons) {
        barW += button.rect.right - button.rect.left;
    }

    HBITMAP fresh = RenderBar(buttons, barW, barH);
    if (fresh == nullptr) {
        return;
    }
    if (barBitmap != nullptr) {
        DeleteObject(barBitmap);
    }
    barBitmap = fresh;

    PlaceBar();
    ShowWindow(barHwnd, SW_SHOWNOACTIVATE);
    // Re-assert topmost now; the pet's periodic re-assert is paused so it can
    // never climb above the bar while the bar is open.
    SetWindowPos(barHwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    if (actions.pauseTopmost) {
        actions.pauseTopmost(true);
    }
    barShown = true;
}

void Tray::HideBar() {
    if (!barShown) {
        return;
    }
    barShown = false;
    ShowWindow(barHwnd, SW_HIDE);
    if (actions.pauseTopmost) {
        actions.pauseTopmost(false);
    }
}

void Tray::HandleBarClick(int x, int y) {
    if (barDC == nullptr) {
        HideBar();
        return;
    }
    gdi::Bitmap probe(1, 1, PixelFormat32bppARGB);
    gdi::Graphics measure(&probe);
    gdi::Font font(kFontFamily, static_cast<gdi::REAL>(kBarFontSizePx),
                   gdi::FontStyleRegular, gdi::UnitPoint);
    const std::vector<BarButton> buttons =
        LayoutButtons(petVisible, measure, font, barH);
    for (const BarButton& button : buttons) {
        if (button.enabled && x >= button.rect.left && x < button.rect.right &&
            y >= button.rect.top && y < button.rect.bottom) {
            HideBar();
            HandleCommand(button.id);
            return;
        }
    }
    HideBar();  // click outside any button: just dismiss
}

void Tray::HandleCommand(UINT_PTR command) {
    switch (command) {
    case kTrayMenuToggle:
        petVisible = !petVisible;
        if (actions.toggleVisible) {
            actions.toggleVisible(petVisible);
        }
        break;
    case kTrayMenuAbout:
        if (actions.about) {
            actions.about();
        }
        break;
    case kTrayMenuActivity:
        // Placeholder button is greyed out; no action yet.
        break;
    case kTrayMenuQuit:
        if (actions.quit) {
            actions.quit();
        }
        break;
    default:
        break;
    }
}

void Tray::Shutdown() {
    if (registered) {
        Shell_NotifyIconW(NIM_DELETE, &nid);  // avoid a stale tray icon
        registered = false;
    }
    barShown = false;
    if (barBitmap != nullptr) {
        DeleteObject(barBitmap);
        barBitmap = nullptr;
    }
    if (barDC != nullptr) {
        DeleteDC(barDC);
        barDC = nullptr;
    }
    if (barHwnd != nullptr) {
        DestroyWindow(barHwnd);
        barHwnd = nullptr;
    }
    if (hwnd != nullptr) {
        DestroyWindow(hwnd);
        hwnd = nullptr;
    }
    if (icon != nullptr && ownsIcon) {
        DestroyIcon(icon);  // never destroy the shared IDI_APPLICATION icon
        icon = nullptr;
        ownsIcon = false;
    }
    if (instance != nullptr) {
        UnregisterClassW(kTrayClassName, instance);
        UnregisterClassW(kBarClassName, instance);
        instance = nullptr;
    }
}

LRESULT CALLBACK TrayWndProc(HWND hwnd, UINT message, WPARAM wParam,
                             LPARAM lParam) {
    Tray* tray = reinterpret_cast<Tray*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (message) {
    case WM_CREATE: {
        CREATESTRUCTW* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        tray = static_cast<Tray*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(tray));
        return 0;
    }

    case WM_APP + 1: {
        // Shell_NotifyIcon callback: lParam holds the mouse message.
        if (tray == nullptr) {
            return 0;
        }
        const UINT notify = LOWORD(lParam);
        if (notify == WM_RBUTTONUP || notify == WM_LBUTTONDBLCLK) {
            tray->ShowBar();
        } else if (notify == WM_LBUTTONUP) {
            // A single left-click toggles visibility.
            tray->HandleCommand(kTrayMenuToggle);
        }
        return 0;
    }

    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK BarWndProc(HWND hwnd, UINT message, WPARAM wParam,
                            LPARAM lParam) {
    Tray* tray = reinterpret_cast<Tray*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (message) {
    case WM_CREATE: {
        CREATESTRUCTW* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        tray = static_cast<Tray*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(tray));
        return 0;
    }

    case WM_LBUTTONUP:
        if (tray != nullptr) {
            const int x = static_cast<short>(LOWORD(lParam));
            const int y = static_cast<short>(HIWORD(lParam));
            tray->HandleBarClick(x, y);
        }
        return 0;

    case WM_RBUTTONUP:
        if (tray != nullptr) {
            tray->HideBar();
        }
        return 0;

    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}
