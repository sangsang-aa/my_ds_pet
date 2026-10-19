#include "tray.h"

#include <utility>

#include "core/config.h"
#include "core/gdi.h"
#include "tray_logic.h"

namespace {

constexpr UINT kTrayIconId = 1;
constexpr wchar_t kTrayTip[] = L"桌面桌宠";

HICON g_icon = nullptr;
HWND g_owner = nullptr;
bool g_added = false;
tray::Callbacks g_callbacks;

void ShowMenu(HWND owner) {
    HMENU menu = CreatePopupMenu();
    if (menu == nullptr) {
        return;
    }
    AppendMenuW(menu, MF_STRING, tray_logic::kCommandResize, L"调整大小...");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, tray_logic::kCommandQuit, L"退出桌宠");
    POINT pt = {};
    GetCursorPos(&pt);
    // SetForegroundWindow before TrackPopupMenu: otherwise the first click on
    // a menu that failed to take the foreground only dismisses it.
    SetForegroundWindow(owner);
    const BOOL command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                        pt.x, pt.y, 0, owner, nullptr);
    PostMessageW(owner, WM_NULL, 0, 0);
    DestroyMenu(menu);
    if (command == tray_logic::kCommandResize && g_callbacks.onResize) {
        g_callbacks.onResize();
    } else if (command == tray_logic::kCommandQuit && g_callbacks.onQuit) {
        g_callbacks.onQuit();
    }
}

}  // namespace

namespace tray {

bool Init(HWND owner, HINSTANCE instance, const std::wstring& iconPath,
          Callbacks callbacks) {
    Destroy();
    gdi::Bitmap bitmap(iconPath.c_str());
    HICON icon = nullptr;
    bitmap.GetHICON(&icon);
    if (icon == nullptr) {
        // The PNG may be missing at runtime; the tray entry must still exist
        // so the control panel can quit the app.
        icon = LoadIconW(instance, reinterpret_cast<LPCWSTR>(IDI_APPLICATION));
    }
    if (icon == nullptr) {
        return false;
    }
    NOTIFYICONDATAW data = {};
    data.cbSize = sizeof(data);
    data.hWnd = owner;
    data.uID = kTrayIconId;
    data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    data.uCallbackMessage = kTrayCallbackMessage;
    data.hIcon = icon;
    lstrcpynW(data.szTip, kTrayTip, ARRAYSIZE(data.szTip));
    if (!Shell_NotifyIconW(NIM_ADD, &data)) {
        DestroyIcon(icon);
        return false;
    }
    g_icon = icon;
    g_owner = owner;
    g_added = true;
    g_callbacks = std::move(callbacks);
    return true;
}

void OnCallback(HWND owner, LPARAM lParam) {
    switch (LOWORD(lParam)) {
    case WM_RBUTTONUP:
    case WM_CONTEXTMENU:
        ShowMenu(owner);
        break;
    default:
        break;
    }
}

void Destroy() {
    if (g_added && g_owner != nullptr) {
        NOTIFYICONDATAW data = {};
        data.cbSize = sizeof(data);
        data.hWnd = g_owner;
        data.uID = kTrayIconId;
        Shell_NotifyIconW(NIM_DELETE, &data);
    }
    if (g_icon != nullptr) {
        DestroyIcon(g_icon);
        g_icon = nullptr;
    }
    g_added = false;
    g_owner = nullptr;
    g_callbacks = {};
}

}  // namespace tray
