#include "scale_dialog.h"

#include <functional>
#include <string>
#include <utility>

#include <commctrl.h>

#include "core/config.h"
#include "tray_logic.h"

namespace {

constexpr wchar_t kScaleDialogClassName[] = L"DesktopPetScaleDialog";
constexpr int kDialogWidth = 340;
constexpr int kDialogHeight = 180;
constexpr int kOkButtonId = 1;
constexpr int kCancelButtonId = 2;

struct DialogState {
    HWND hwnd = nullptr;
    HWND track = nullptr;
    HWND value = nullptr;
    HINSTANCE inst = nullptr;
    int currentPercent = 0;
    std::function<void(int)> onApply;
};

bool RegisterDialogClass(HINSTANCE instance) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSW windowClass = {};
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc = [](HWND hwnd, UINT message, WPARAM wParam,
                                     LPARAM lParam) -> LRESULT {
            DialogState* state = reinterpret_cast<DialogState*>(
                GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            switch (message) {
            case WM_NCCREATE: {
                CREATESTRUCTW* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
                state = static_cast<DialogState*>(create->lpCreateParams);
                SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                                  reinterpret_cast<LONG_PTR>(state));
                break;
            }
            case WM_CREATE:
                if (state != nullptr) {
                    state->hwnd = hwnd;
                    const HFONT font = static_cast<HFONT>(
                        GetStockObject(DEFAULT_GUI_FONT));
                    HWND child = CreateWindowExW(
                        0, L"STATIC", L"桌宠大小", WS_CHILD | WS_VISIBLE, 16, 12,
                        308, 20, hwnd, nullptr, state->inst, nullptr);
                    SendMessageW(child, WM_SETFONT,
                                 reinterpret_cast<WPARAM>(font), TRUE);
                    child = CreateWindowExW(
                        0, TRACKBAR_CLASSW, L"",
                        WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_HORZ, 16, 38,
                        308, 44, hwnd, nullptr, state->inst, nullptr);
                    SendMessageW(child, TBM_SETRANGE, TRUE,
                                 MAKELPARAM(kScaleMinPercent, kScaleMaxPercent));
                    SendMessageW(child, TBM_SETTICFREQ, 10, 0);
                    SendMessageW(child, TBM_SETPOS, TRUE,
                                 state->currentPercent);
                    state->track = child;
                    child = CreateWindowExW(
                        0, L"STATIC",
                        (std::to_wstring(state->currentPercent) + L"%").c_str(),
                        WS_CHILD | WS_VISIBLE, 16, 88, 308, 22, hwnd, nullptr,
                        state->inst, nullptr);
                    SendMessageW(child, WM_SETFONT,
                                 reinterpret_cast<WPARAM>(font), TRUE);
                    state->value = child;
                    child = CreateWindowExW(
                        0, L"BUTTON", L"确定", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        216, 124, 100, 30, hwnd,
                        reinterpret_cast<HMENU>(kOkButtonId), state->inst,
                        nullptr);
                    SendMessageW(child, WM_SETFONT,
                                 reinterpret_cast<WPARAM>(font), TRUE);
                    child = CreateWindowExW(
                        0, L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        108, 124, 100, 30, hwnd,
                        reinterpret_cast<HMENU>(kCancelButtonId), state->inst,
                        nullptr);
                    SendMessageW(child, WM_SETFONT,
                                 reinterpret_cast<WPARAM>(font), TRUE);
                }
                return 0;
            case WM_HSCROLL:
                if (state != nullptr && state->track != nullptr) {
                    const int pos = static_cast<int>(
                        SendMessageW(state->track, TBM_GETPOS, 0, 0));
                    SetWindowTextW(state->value,
                                   (std::to_wstring(pos) + L"%").c_str());
                }
                return 0;
            case WM_COMMAND:
                if (LOWORD(wParam) == kOkButtonId && state != nullptr) {
                    const int pos = static_cast<int>(
                        SendMessageW(state->track, TBM_GETPOS, 0, 0));
                    if (state->onApply) {
                        state->onApply(pos);
                    }
                    DestroyWindow(hwnd);
                } else if (LOWORD(wParam) == kCancelButtonId) {
                    DestroyWindow(hwnd);
                }
                return 0;
            case WM_CLOSE:
                DestroyWindow(hwnd);
                return 0;
            case WM_KEYDOWN:
                if (wParam == VK_ESCAPE) {
                    DestroyWindow(hwnd);
                    return 0;
                }
                break;
            case WM_NCDESTROY:
                delete state;
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
                break;
            default:
                break;
            }
            return DefWindowProcW(hwnd, message, wParam, lParam);
        };
        windowClass.hInstance = instance;
        windowClass.hCursor =
            LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
        windowClass.hbrBackground =
            reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
        windowClass.lpszClassName = kScaleDialogClassName;
        registered = RegisterClassW(&windowClass) != 0;
    }
    return registered;
}

}  // namespace

namespace tray {

void ShowScaleDialog(HWND owner, HINSTANCE instance, int currentPercent,
                     std::function<void(int)> onApply) {
    if (!RegisterDialogClass(instance)) {
        return;
    }
    const int percent = tray_logic::ClampScalePercent(
        currentPercent, kScaleMinPercent, kScaleMaxPercent);
    RECT clientRect = { 0, 0, kDialogWidth, kDialogHeight };
    AdjustWindowRectEx(&clientRect, WS_POPUP | WS_CAPTION | WS_SYSMENU, FALSE,
                       WS_EX_TOPMOST | WS_EX_TOOLWINDOW);
    const int width = clientRect.right - clientRect.left;
    const int height = clientRect.bottom - clientRect.top;
    RECT ownerRect = {};
    GetWindowRect(owner, &ownerRect);
    const int x = ownerRect.left +
                  ((ownerRect.right - ownerRect.left) - width) / 2;
    const int y = ownerRect.top +
                  ((ownerRect.bottom - ownerRect.top) - height) / 2;
    DialogState* state = new DialogState{};
    state->inst = instance;
    state->currentPercent = percent;
    state->onApply = std::move(onApply);
    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW, kScaleDialogClassName, L"调整大小",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x, y, width, height,
        owner, nullptr, instance, state);
    if (hwnd == nullptr) {
        delete state;
        return;
    }
    SetForegroundWindow(hwnd);
}

}  // namespace tray
