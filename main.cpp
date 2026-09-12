// Desktop pet demo: a transparent, always-on-top floating window that plays a
// PNG frame sequence (desktop_pet_image\<action>\000.png ..) using the Win32
// layered-window API and GDI+. The base action loops forever; a "clicked"
// action plays once on each click. Single file, self-contained, C++17.
//
// Build (MSVC):
//   cl /nologo /EHsc /std:c++17 /O2 main.cpp gdiplus.lib user32.lib gdi32.lib /link /SUBSYSTEM:WINDOWS /OUT:pet_demo.exe
// Build (MinGW-w64):
//   x86_64-w64-mingw32-g++ -std=c++17 -O2 -mwindows main.cpp -lgdiplus -luser32 -lgdi32 -o pet_demo.exe

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <wtypes.h>   // PROPID, required by gdiplus.h
#include <gdiplus.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#ifdef _MSC_VER
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#endif

// __argc / __argv are exported by both the MSVC and the MinGW-w64 CRT, so the
// GUI entry point can still see the (narrow) command-line arguments.
extern "C" {
extern int __argc;
extern char** __argv;
}

namespace gdi = Gdiplus;

namespace {

constexpr UINT_PTR kTimerId = 1;
constexpr UINT kTimerElapseMs = 100;        // ~10 fps
constexpr size_t kMaxFrames = 120;          // convention: 000.png .. 119.png
constexpr float kScaleFactor = 0.70f;       // pet renders 30% smaller on screen
constexpr wchar_t kClassName[] = L"DesktopPetWindowClass";
constexpr wchar_t kAssetsDir[] = L"desktop_pet_image";
constexpr wchar_t kDefaultAction[] = L"idle";

struct Frame {
    HBITMAP hbmp = nullptr;
    bool hasAlpha = false;  // source PNG had an alpha channel?
};

std::vector<Frame> g_baseFrames;
std::vector<Frame> g_clickFrames;
std::vector<Frame> g_dragFrames;
int g_baseIndex = 0;
int g_clickIndex = 0;
int g_dragIndex = 0;

enum class PetState { Base, Clicked, Dragging };
PetState g_state = PetState::Base;

bool g_mouseDown = false;
bool g_dragging = false;
POINT g_mouseDownPos = {};
int g_dragOffsetX = 0;
int g_dragOffsetY = 0;

HDC g_memDC = nullptr;
int g_petW = 0;
int g_petH = 0;

// RAII wrapper for the GdiplusStartup / GdiplusShutdown pair.
class GdiplusSession {
public:
    GdiplusSession() {
        gdi::GdiplusStartupInput input;
        const gdi::Status status =
            gdi::GdiplusStartup(&token_, &input, nullptr);
        ok_ = (status == gdi::Ok);
    }
    ~GdiplusSession() {
        if (ok_) {
            gdi::GdiplusShutdown(token_);
        }
    }
    GdiplusSession(const GdiplusSession&) = delete;
    GdiplusSession& operator=(const GdiplusSession&) = delete;
    bool ok() const { return ok_; }

private:
    ULONG_PTR token_ = 0;
    bool ok_ = false;
};

// Directory (with trailing backslash) of the running executable. Assets are
// resolved relative to this directory, never relative to the CWD.
std::wstring GetExecutableDir() {
    wchar_t buffer[MAX_PATH];
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return std::wstring();
    }
    std::wstring dir(buffer, length);
    const size_t slash = dir.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        dir.resize(slash + 1);
    }
    return dir;
}

// Narrow UTF-8 string -> wide string, or empty on failure.
std::wstring WideFromUtf8(const char* utf8) {
    if (utf8 == nullptr || *utf8 == '\0') {
        return std::wstring();
    }
    const int needed = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    if (needed <= 1) {
        return std::wstring();
    }
    std::wstring result(static_cast<size_t>(needed) - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &result[0], needed);
    return result;
}

std::wstring FrameName(int index) {
    std::wstring name = std::to_wstring(index);
    if (name.size() == 1) {
        name = L"00" + name;
    } else if (name.size() == 2) {
        name = L"0" + name;
    }
    return name + L".png";
}

std::wstring FramePath(const std::wstring& exeDir,
                       const std::wstring& assetsDir,
                       const std::wstring& action,
                       int index) {
    return exeDir + assetsDir + L"\\" + action + L"\\" + FrameName(index);
}

// Load consecutive frames starting at 000.png; stop at the first missing file.
std::vector<Frame> LoadFrames(const std::wstring& exeDir,
                              const std::wstring& assetsDir,
                              const std::wstring& action) {
    std::vector<Frame> frames;
    for (int i = 0; i < static_cast<int>(kMaxFrames); ++i) {
        const std::wstring path = FramePath(exeDir, assetsDir, action, i);
        gdi::Bitmap source(path.c_str());
        if (source.GetLastStatus() != gdi::Ok) {
            break;  // first missing frame -> stop loading
        }
        gdi::Bitmap* frameBitmap = &source;
        std::unique_ptr<gdi::Bitmap> scaled;
        if (kScaleFactor != 1.0f) {
            const int scaledW = std::max(
                1, static_cast<int>(std::lround(source.GetWidth() * kScaleFactor)));
            const int scaledH = std::max(
                1, static_cast<int>(std::lround(source.GetHeight() * kScaleFactor)));
            // Scale in the source's alpha mode: RGB frames stay opaque (no
            // alpha bits), RGBA frames keep per-pixel alpha.
            scaled.reset(new gdi::Bitmap(
                scaledW, scaledH,
                (source.GetPixelFormat() & PixelFormatAlpha) != 0
                    ? PixelFormat32bppARGB
                    : PixelFormat24bppRGB));
            if (scaled->GetLastStatus() != gdi::Ok) {
                break;
            }
            gdi::Graphics graphics(scaled.get());
            graphics.SetInterpolationMode(gdi::InterpolationModeHighQualityBicubic);
            graphics.DrawImage(&source, 0, 0, scaledW, scaledH);
            frameBitmap = scaled.get();
        }
        HBITMAP hbmp = nullptr;
        if (frameBitmap->GetHBITMAP(gdi::Color(0, 0, 0, 0), &hbmp) != gdi::Ok ||
            hbmp == nullptr) {
            if (hbmp != nullptr) {
                DeleteObject(hbmp);
            }
            break;
        }
        Frame frame;
        frame.hbmp = hbmp;
        frame.hasAlpha = (frameBitmap->GetPixelFormat() & PixelFormatAlpha) != 0;
        frames.push_back(frame);
    }
    return frames;
}

// Composite the given frame into the layered window. Position and size keep the
// window where it currently is (the window may have been dragged around).
void PresentFrame(HWND hwnd, const Frame& frame) {
    if (g_memDC == nullptr || frame.hbmp == nullptr) {
        return;
    }
    RECT windowRect = {};
    GetWindowRect(hwnd, &windowRect);
    POINT destPos = { windowRect.left, windowRect.top };
    SIZE windowSize = { g_petW, g_petH };
    POINT srcPos = { 0, 0 };

    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.BlendFlags = 0;
    blend.SourceConstantAlpha = 255;
    // RGBA frames blend per-pixel; RGB frames have no per-pixel alpha, so the
    // whole frame is drawn opaque via SourceConstantAlpha (otherwise the zeroed
    // alpha bytes would render it fully transparent).
    blend.AlphaFormat = frame.hasAlpha ? AC_SRC_ALPHA : 0;

    const HGDIOBJ previous = SelectObject(g_memDC, frame.hbmp);
    UpdateLayeredWindow(hwnd, nullptr, &destPos, &windowSize, g_memDC,
                        &srcPos, 0, &blend, ULW_ALPHA);
    SelectObject(g_memDC, previous);
}

// Restart the one-shot clicked animation from its first frame.
void TriggerClickReaction(HWND hwnd) {
    if (g_clickFrames.empty()) {
        return;  // click reaction was disabled at startup
    }
    g_state = PetState::Clicked;
    g_clickIndex = 0;
    PresentFrame(hwnd, g_clickFrames[0]);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        g_memDC = CreateCompatibleDC(nullptr);
        return 0;

    case WM_TIMER:
        if (wParam == kTimerId) {
            if (g_state == PetState::Clicked) {
                g_clickIndex += 1;
                if (g_clickIndex >= static_cast<int>(g_clickFrames.size())) {
                    // clicked animation finished: resume the base loop
                    g_state = PetState::Base;
                    if (!g_baseFrames.empty()) {
                        PresentFrame(hwnd, g_baseFrames[static_cast<size_t>(g_baseIndex)]);
                    }
                } else {
                    PresentFrame(hwnd, g_clickFrames[static_cast<size_t>(g_clickIndex)]);
                }
            } else if (g_state == PetState::Dragging) {
                // drag reaction loops while the pet is being dragged
                if (!g_dragFrames.empty()) {
                    g_dragIndex =
                        (g_dragIndex + 1) % static_cast<int>(g_dragFrames.size());
                    PresentFrame(hwnd, g_dragFrames[static_cast<size_t>(g_dragIndex)]);
                }
            } else {  // Base
                if (!g_baseFrames.empty()) {
                    g_baseIndex =
                        (g_baseIndex + 1) % static_cast<int>(g_baseFrames.size());
                    PresentFrame(hwnd, g_baseFrames[static_cast<size_t>(g_baseIndex)]);
                }
            }
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

    case WM_LBUTTONDOWN:
        SetFocus(hwnd);  // make sure the ESC key reaches this window
        g_mouseDown = true;
        g_dragging = false;
        GetCursorPos(&g_mouseDownPos);
        SetCapture(hwnd);
        return 0;

    case WM_MOUSEMOVE:
        if (g_mouseDown && !g_dragging) {
            POINT cursor = {};
            GetCursorPos(&cursor);
            const int dx = cursor.x - g_mouseDownPos.x;
            const int dy = cursor.y - g_mouseDownPos.y;
            // Small movements are clicks; past the system drag threshold it
            // becomes a drag and the grab point stays under the cursor.
            if (std::abs(dx) >= GetSystemMetrics(SM_CXDRAG) ||
                std::abs(dy) >= GetSystemMetrics(SM_CYDRAG)) {
                g_dragging = true;
                RECT windowRect = {};
                GetWindowRect(hwnd, &windowRect);
                g_dragOffsetX = cursor.x - windowRect.left;
                g_dragOffsetY = cursor.y - windowRect.top;
                // start the drag reaction from its first frame
                if (!g_dragFrames.empty()) {
                    g_state = PetState::Dragging;
                    g_dragIndex = 0;
                    PresentFrame(hwnd, g_dragFrames[0]);
                }
            }
        }
        if (g_dragging) {
            POINT cursor = {};
            GetCursorPos(&cursor);
            SetWindowPos(hwnd, nullptr, cursor.x - g_dragOffsetX,
                         cursor.y - g_dragOffsetY, 0, 0,
                         SWP_NOSIZE | SWP_NOZORDER);
        }
        return 0;

    case WM_LBUTTONUP:
        if (g_mouseDown && !g_dragging) {
            TriggerClickReaction(hwnd);
        } else if (g_dragging) {
            g_state = PetState::Base;
            if (!g_baseFrames.empty()) {
                PresentFrame(hwnd, g_baseFrames[static_cast<size_t>(g_baseIndex)]);
            }
        }
        g_mouseDown = false;
        g_dragging = false;
        ReleaseCapture();
        return 0;

    case WM_CAPTURECHANGED:
        // Capture was lost (released or stolen): cancel any in-flight drag.
        g_mouseDown = false;
        g_dragging = false;
        if (g_state == PetState::Dragging) {
            g_state = PetState::Base;
        }
        return 0;

    case WM_RBUTTONUP:
        DestroyWindow(hwnd);
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            DestroyWindow(hwnd);
            return 0;
        }
        break;

    case WM_DESTROY:
        // Delete the DC first so no frame HBITMAP is still selected into it.
        if (g_memDC != nullptr) {
            DeleteDC(g_memDC);
            g_memDC = nullptr;
        }
        for (const Frame& frame : g_baseFrames) {
            if (frame.hbmp != nullptr) {
                DeleteObject(frame.hbmp);
            }
        }
        g_baseFrames.clear();
        for (const Frame& frame : g_clickFrames) {
            if (frame.hbmp != nullptr) {
                DeleteObject(frame.hbmp);
            }
        }
        g_clickFrames.clear();
        for (const Frame& frame : g_dragFrames) {
            if (frame.hbmp != nullptr) {
                DeleteObject(frame.hbmp);
            }
        }
        g_dragFrames.clear();
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

}  // namespace

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    // Base action name comes from argv[1]; no argument defaults to "idle".
    std::string actionUtf8 = "idle";
    if (__argc > 1 && __argv != nullptr && __argv[1] != nullptr) {
        actionUtf8 = __argv[1];
    }
    // Optional argv[2] overrides the assets directory name (default
    // "desktop_pet_image"), resolved relative to the executable directory.
    std::string assetsUtf8 = "desktop_pet_image";
    if (__argc > 2 && __argv != nullptr && __argv[2] != nullptr) {
        assetsUtf8 = __argv[2];
    }

    // Narrow (UTF-8) arguments -> wide strings via MultiByteToWideChar.
    std::wstring action = WideFromUtf8(actionUtf8.c_str());
    if (action.empty()) {
        action = kDefaultAction;
    }
    std::wstring assetsDir = WideFromUtf8(assetsUtf8.c_str());
    if (assetsDir.empty()) {
        assetsDir = kAssetsDir;
    }

    GdiplusSession gdiplus;
    if (!gdiplus.ok()) {
        std::printf("[pet] GDI+ failed to initialize\n");
        return 1;
    }

    const std::wstring exeDir = GetExecutableDir();
    g_baseFrames = LoadFrames(exeDir, assetsDir, action);

    std::printf("[pet] action=%s loaded=%zu/%zu frames\n",
                actionUtf8.c_str(), g_baseFrames.size(), kMaxFrames);

    if (g_baseFrames.empty()) {
        std::printf("[pet] no frames found for action '%s' "
                    "(looked for %s\\%s\\000.png)\n",
                    actionUtf8.c_str(), assetsUtf8.c_str(), actionUtf8.c_str());
        return 1;
    }

    g_clickFrames = LoadFrames(exeDir, assetsDir, L"clicked");
    if (g_clickFrames.empty()) {
        std::printf("[pet] no 'clicked' frames, click reaction disabled\n");
    } else {
        std::printf("[pet] clicked loaded=%zu/%zu frames\n",
                    g_clickFrames.size(), kMaxFrames);
    }

    g_dragFrames = LoadFrames(exeDir, assetsDir, L"drag");
    if (g_dragFrames.empty()) {
        std::printf("[pet] no 'drag' frames, drag animation disabled\n");
    } else {
        std::printf("[pet] drag loaded=%zu/%zu frames\n",
                    g_dragFrames.size(), kMaxFrames);
    }

    // The window size adapts to the first base frame's pixel dimensions.
    BITMAP firstFrame = {};
    if (GetObjectW(g_baseFrames[0].hbmp, static_cast<int>(sizeof(BITMAP)),
                   &firstFrame) == 0 ||
        firstFrame.bmWidth <= 0 || firstFrame.bmHeight <= 0) {
        std::printf("[pet] could not read the first frame's dimensions\n");
        return 1;
    }
    g_petW = firstFrame.bmWidth;
    g_petH = firstFrame.bmHeight;

    WNDCLASSW windowClass = {};
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WndProc;
    windowClass.hInstance = hInstance;
    windowClass.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    windowClass.lpszClassName = kClassName;
    if (!RegisterClassW(&windowClass)) {
        std::printf("[pet] RegisterClassW failed (error %lu)\n",
                    static_cast<unsigned long>(GetLastError()));
        return 1;
    }

    const int screenW = GetSystemMetrics(SM_CXSCREEN);
    const int screenH = GetSystemMetrics(SM_CYSCREEN);
    const int startX = (screenW - g_petW) / 2;
    const int startY = (screenH - g_petH) / 2;

    HWND hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        kClassName,
        L"Desktop Pet",
        WS_POPUP,
        startX, startY, g_petW, g_petH,
        nullptr, nullptr, hInstance, nullptr);
    if (hwnd == nullptr) {
        std::printf("[pet] CreateWindowExW failed (error %lu)\n",
                    static_cast<unsigned long>(GetLastError()));
        return 1;
    }

    SetTimer(hwnd, kTimerId, kTimerElapseMs, nullptr);

    PresentFrame(hwnd, g_baseFrames[0]);
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    SetFocus(hwnd);

    MSG message = {};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}
