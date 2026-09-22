// Desktop pet demo: a transparent, always-on-top floating window that plays a
// PNG frame sequence (desktop_pet_image\<action>\000.png ..) using the Win32
// layered-window API and GDI+. With no argument the base animation is a random
// idle pool ("idle"/"idle2"); a "clicked" action plays once per click and a
// "drag" action loops while dragging. Single file, self-contained, C++17.
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
#include <random>
#include <string>
#include <vector>

#include "bubble_logic.h"

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
constexpr int kWalkStepPx = 5;          // window movement per tick while walking
constexpr int kIdleMinTicks = 30;       // idle dwell before walking (~3s)
constexpr int kIdleMaxTicks = 100;      // ~10s
constexpr int kWalkMinTicks = 15;       // walk duration (~1.5s)
constexpr int kWalkMaxTicks = 45;       // ~4.5s
constexpr int kWaitMinTicks = 1200;     // post-drag idle-only wait (~2 min)
constexpr int kWaitMaxTicks = 1800;     // ~3 min
constexpr int kHeadBandPercent = 45;    // top 45% of the pet counts as the "head"
constexpr int kSweepThresholdPx = 50;   // horizontal travel over the head -> shy
constexpr int kMaxSweepDeltaPx = 80;    // ignore jumpy deltas (mouse re-entry)
constexpr int kShyCooldownTicks = 25;   // ~2.5s between shy reactions

struct Frame {
    HBITMAP hbmp = nullptr;
    bool hasAlpha = false;  // source PNG had an alpha channel?
};

std::vector<std::vector<Frame>> g_basePool;
std::vector<Frame> g_clickFrames;
std::vector<Frame> g_dragFrames;
std::vector<Frame> g_walkLeftFrames;
std::vector<Frame> g_walkRightFrames;
std::vector<Frame> g_walkUpFrames;
std::vector<Frame> g_walkDownFrames;
std::vector<Frame> g_shyFrames;

enum class WalkDir { Left, Right, Up, Down };

int g_basePoolIndex = 0;
int g_baseIndex = 0;
int g_clickIndex = 0;
int g_dragIndex = 0;
int g_walkIndex = 0;
int g_walkStep = 1;  // +1 / -1 frame step for boomerang playback
bool g_walkBoomerang = false;
WalkDir g_walkDir = WalkDir::Right;
int g_shyIndex = 0;

enum class PetState { Base, Clicked, Dragging, Walking, Shy };
PetState g_state = PetState::Base;

int g_behaviorTicks = 0;
int g_behaviorTarget = 0;
int g_screenW = 0;
int g_screenH = 0;
int g_waitTicks = 0;
int g_headSweepPx = 0;
int g_lastMouseX = 0;
int g_lastMouseY = 0;
int g_shyCooldownTicks = 0;

bool g_mouseDown = false;
bool g_dragging = false;
POINT g_mouseDownPos = {};
int g_dragOffsetX = 0;
int g_dragOffsetY = 0;

HDC g_memDC = nullptr;
int g_petW = 0;
int g_petH = 0;

std::vector<std::wstring> g_emojiPaths;
HWND g_petHwnd = nullptr;
HWND g_bubbleHwnd = nullptr;
HDC g_bubbleDC = nullptr;
HBITMAP g_bubbleHbmp = nullptr;
int g_bubbleW = 0;
int g_bubbleH = 0;
int g_bubbleCountdown = 0;
int g_bubbleShowLeft = 0;
bool g_bubbleVisible = false;

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

// GUI apps have no console, so a fatal startup error must be shown here to avoid
// the "double-click does nothing" silent exit.
void ShowFatalError(const std::wstring& message) {
    MessageBoxW(nullptr, message.c_str(), L"Desktop Pet",
                MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
}

// Comma-separated list of action folders under assetsPath that contain 000.png.
std::wstring AvailableActions(const std::wstring& assetsPath) {
    std::wstring result;
    WIN32_FIND_DATAW entry = {};
    const std::wstring pattern = assetsPath + L"\\*";
    HANDLE handle = FindFirstFileW(pattern.c_str(), &entry);
    if (handle == INVALID_HANDLE_VALUE) {
        return L"(assets folder not found)";
    }
    do {
        if ((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
            entry.cFileName[0] != L'.') {
            const std::wstring first =
                assetsPath + L"\\" + entry.cFileName + L"\\000.png";
            if (GetFileAttributesW(first.c_str()) != INVALID_FILE_ATTRIBUTES) {
                if (!result.empty()) {
                    result += L", ";
                }
                result += entry.cFileName;
            }
        }
    } while (FindNextFileW(handle, &entry) != 0);
    FindClose(handle);
    return result.empty() ? L"(none)" : result;
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

// Present the current frame of the active base (idle) animation.
void PresentBaseFrame(HWND hwnd) {
    if (g_basePool.empty()) {
        return;
    }
    const std::vector<Frame>& base =
        g_basePool[static_cast<size_t>(g_basePoolIndex)];
    if (base.empty()) {
        return;
    }
    PresentFrame(hwnd, base[static_cast<size_t>(g_baseIndex) % base.size()]);
}

std::mt19937& Rng() {
    static std::mt19937 rng(std::random_device{}());
    return rng;
}

int RandomInRange(int lo, int hi) {
    if (hi <= lo) {
        return lo;
    }
    std::uniform_int_distribution<int> dist(lo, hi);
    return dist(Rng());
}

// Random idle: pick the next idle animation in the pool (may repeat).
int PickRandomBaseIndex(int poolSize) {
    return RandomInRange(0, poolSize - 1);
}

const std::vector<Frame>& WalkFrames(WalkDir dir) {
    switch (dir) {
    case WalkDir::Left:
        return g_walkLeftFrames;
    case WalkDir::Up:
        return g_walkUpFrames;
    case WalkDir::Down:
        return g_walkDownFrames;
    case WalkDir::Right:
    default:
        return g_walkRightFrames;
    }
}

void WalkStep(WalkDir dir, int& dx, int& dy) {
    dx = 0;
    dy = 0;
    switch (dir) {
    case WalkDir::Left:
        dx = -kWalkStepPx;
        break;
    case WalkDir::Right:
        dx = kWalkStepPx;
        break;
    case WalkDir::Up:
        dy = -kWalkStepPx;
        break;
    case WalkDir::Down:
        dy = kWalkStepPx;
        break;
    }
}

// Uniformly pick among the directions that actually have frames loaded.
WalkDir PickRandomWalkDir() {
    const WalkDir all[4] = { WalkDir::Left, WalkDir::Right,
                             WalkDir::Up, WalkDir::Down };
    WalkDir pool[4];
    int count = 0;
    for (WalkDir dir : all) {
        if (!WalkFrames(dir).empty()) {
            pool[count++] = dir;
        }
    }
    if (count == 0) {
        return WalkDir::Right;
    }
    return pool[RandomInRange(0, count - 1)];
}

// Advance the walk frame index. One-way clips (up/down) play as a boomerang
// (forward then backward) so they loop with no visible jump back to the start.
void AdvanceWalkIndex(int size) {
    if (size <= 0) {
        return;
    }
    if (!g_walkBoomerang || size == 1) {
        g_walkIndex = (g_walkIndex + 1) % size;
        return;
    }
    g_walkIndex += g_walkStep;
    if (g_walkIndex >= size) {
        g_walkStep = -1;
        g_walkIndex = size - 2;
    } else if (g_walkIndex < 0) {
        g_walkStep = 1;
        g_walkIndex = 1;
    }
}

// Return to idle and schedule the next walk after a random dwell.
void EnterBase(HWND hwnd) {
    g_state = PetState::Base;
    g_behaviorTicks = 0;
    g_behaviorTarget = RandomInRange(kIdleMinTicks, kIdleMaxTicks);
    PresentBaseFrame(hwnd);
}

// After the user drops the pet: idle in place (no walking) for a random 2-3
// minutes, then resume the normal idle<->walk chain.
void EnterWaiting(HWND hwnd) {
    EnterBase(hwnd);
    g_waitTicks = RandomInRange(kWaitMinTicks, kWaitMaxTicks);
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

// Play the one-shot shy animation (mouse sweeping across the pet's head).
void TriggerShyReaction(HWND hwnd) {
    if (g_shyFrames.empty() || g_shyCooldownTicks > 0) {
        return;
    }
    g_state = PetState::Shy;
    g_shyIndex = 0;
    g_shyCooldownTicks = kShyCooldownTicks;
    PresentFrame(hwnd, g_shyFrames[0]);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        g_memDC = CreateCompatibleDC(nullptr);
        return 0;

    case WM_TIMER:
        if (wParam == kTimerId) {
            if (g_shyCooldownTicks > 0) {
                g_shyCooldownTicks -= 1;
            }
            g_headSweepPx -= g_headSweepPx / 2;  // decay, so slow drift never triggers
            if (g_state == PetState::Clicked) {
                g_clickIndex += 1;
                if (g_clickIndex >= static_cast<int>(g_clickFrames.size())) {
                    // clicked animation finished: resume the idle loop
                    EnterBase(hwnd);
                } else {
                    PresentFrame(hwnd, g_clickFrames[static_cast<size_t>(g_clickIndex)]);
                }
            } else if (g_state == PetState::Shy) {
                g_shyIndex += 1;
                if (g_shyIndex >= static_cast<int>(g_shyFrames.size())) {
                    EnterBase(hwnd);
                } else {
                    PresentFrame(hwnd, g_shyFrames[static_cast<size_t>(g_shyIndex)]);
                }
            } else if (g_state == PetState::Dragging) {
                // drag reaction loops while the pet is being dragged
                if (!g_dragFrames.empty()) {
                    g_dragIndex =
                        (g_dragIndex + 1) % static_cast<int>(g_dragFrames.size());
                    PresentFrame(hwnd, g_dragFrames[static_cast<size_t>(g_dragIndex)]);
                }
            } else if (g_state == PetState::Walking) {
                const std::vector<Frame>& walk = WalkFrames(g_walkDir);
                if (walk.empty()) {
                    EnterBase(hwnd);
                } else {
                    AdvanceWalkIndex(static_cast<int>(walk.size()));
                    PresentFrame(hwnd, walk[static_cast<size_t>(g_walkIndex)]);
                    RECT rect = {};
                    GetWindowRect(hwnd, &rect);
                    int dx = 0;
                    int dy = 0;
                    WalkStep(g_walkDir, dx, dy);
                    const int newLeft = rect.left + dx;
                    const int newTop = rect.top + dy;
                    const bool atEdge =
                        (newLeft < 0) || (newLeft + g_petW > g_screenW) ||
                        (newTop < 0) || (newTop + g_petH > g_screenH);
                    if (!atEdge) {
                        SetWindowPos(hwnd, nullptr, newLeft, newTop, 0, 0,
                                     SWP_NOSIZE | SWP_NOZORDER);
                    }
                    g_behaviorTicks += 1;
                    if (atEdge || g_behaviorTicks >= g_behaviorTarget) {
                        EnterBase(hwnd);
                    }
                }
            } else {  // Base
                if (!g_basePool.empty()) {
                    const int size = static_cast<int>(
                        g_basePool[static_cast<size_t>(g_basePoolIndex)].size());
                    if (size > 0) {
                        g_baseIndex += 1;
                        if (g_baseIndex >= size) {
                            // one idle loop finished: switch to a random idle
                            g_baseIndex = 0;
                            if (g_basePool.size() > 1) {
                                g_basePoolIndex = PickRandomBaseIndex(
                                    static_cast<int>(g_basePool.size()));
                            }
                        }
                        PresentBaseFrame(hwnd);
                    }
                    if (g_waitTicks > 0) {
                        // post-drag waiting: idle animations only, never walk
                        g_waitTicks -= 1;
                        if (g_waitTicks == 0) {
                            g_behaviorTicks = 0;
                            g_behaviorTarget =
                                RandomInRange(kIdleMinTicks, kIdleMaxTicks);
                        }
                    } else {
                        const bool canWalk =
                            !g_walkLeftFrames.empty() ||
                            !g_walkRightFrames.empty() ||
                            !g_walkUpFrames.empty() ||
                            !g_walkDownFrames.empty();
                        g_behaviorTicks += 1;
                        if (canWalk && g_behaviorTicks >= g_behaviorTarget) {
                            // idle dwell elapsed: walk in a random direction
                            g_walkDir = PickRandomWalkDir();
                            g_walkBoomerang = (g_walkDir == WalkDir::Up ||
                                               g_walkDir == WalkDir::Down);
                            g_walkIndex = 0;
                            g_walkStep = 1;
                            g_behaviorTicks = 0;
                            g_behaviorTarget =
                                RandomInRange(kWalkMinTicks, kWalkMaxTicks);
                            g_state = PetState::Walking;
                            PresentFrame(hwnd, WalkFrames(g_walkDir)[0]);
                        }
                    }
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
        // Mouse sweeping horizontally across the head -> shy reaction.
        {
            const int mx = static_cast<short>(LOWORD(lParam));
            const int my = static_cast<short>(HIWORD(lParam));
            if (!g_mouseDown && my >= 0 &&
                my < g_petH * kHeadBandPercent / 100 &&
                mx >= 0 && mx < g_petW) {
                const int dx = mx - g_lastMouseX;
                const int dy = my - g_lastMouseY;
                if (std::abs(dx) <= kMaxSweepDeltaPx &&
                    std::abs(dx) > std::abs(dy)) {
                    g_headSweepPx += std::abs(dx);
                }
                if (g_headSweepPx >= kSweepThresholdPx) {
                    g_headSweepPx = 0;
                    TriggerShyReaction(hwnd);
                }
            }
            g_lastMouseX = mx;
            g_lastMouseY = my;
        }
        return 0;

    case WM_LBUTTONUP:
        if (g_mouseDown && !g_dragging) {
            TriggerClickReaction(hwnd);
        } else if (g_dragging) {
            EnterWaiting(hwnd);
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
            EnterWaiting(hwnd);
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
        // Tear the bubble down first: its own WM_DESTROY frees g_bubbleDC and
        // g_bubbleHbmp.
        if (g_bubbleHwnd != nullptr) {
            DestroyWindow(g_bubbleHwnd);
        }
        // Delete the DC first so no frame HBITMAP is still selected into it.
        if (g_memDC != nullptr) {
            DeleteDC(g_memDC);
            g_memDC = nullptr;
        }
        for (const std::vector<Frame>& animation : g_basePool) {
            for (const Frame& frame : animation) {
                if (frame.hbmp != nullptr) {
                    DeleteObject(frame.hbmp);
                }
            }
        }
        g_basePool.clear();
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
        for (const Frame& frame : g_walkLeftFrames) {
            if (frame.hbmp != nullptr) {
                DeleteObject(frame.hbmp);
            }
        }
        g_walkLeftFrames.clear();
        for (const Frame& frame : g_walkRightFrames) {
            if (frame.hbmp != nullptr) {
                DeleteObject(frame.hbmp);
            }
        }
        g_walkRightFrames.clear();
        for (const Frame& frame : g_walkUpFrames) {
            if (frame.hbmp != nullptr) {
                DeleteObject(frame.hbmp);
            }
        }
        g_walkUpFrames.clear();
        for (const Frame& frame : g_walkDownFrames) {
            if (frame.hbmp != nullptr) {
                DeleteObject(frame.hbmp);
            }
        }
        g_walkDownFrames.clear();
        for (const Frame& frame : g_shyFrames) {
            if (frame.hbmp != nullptr) {
                DeleteObject(frame.hbmp);
            }
        }
        g_shyFrames.clear();
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

// -------------------------------------------------------------------------------
// Speech-bubble emoji module
//
// A second, independent layered window shows a random sticker from
// desktop_pet_image\emoji\emoji_NN.png above the pet's head every so often.
// It runs on its own timer and follows the pet by polling its window rect, so
// the pet's own timer and state machine are never touched. WS_EX_TRANSPARENT
// makes the bubble click-through, so mouse input still reaches the pet.
// -------------------------------------------------------------------------------

constexpr wchar_t kBubbleClassName[] = L"DesktopPetBubbleClass";
constexpr UINT_PTR kBubbleTimerId = 2;
constexpr UINT kBubbleTimerElapseMs = 100;
constexpr int kBubbleEmojiSizePx = 120;  // emoji is scaled to fit this box
constexpr int kBubblePadPx = 10;         // padding between emoji and bubble edge
constexpr int kBubbleRadiusPx = 10;      // rounded corner radius
constexpr int kBubbleTailW = 18;         // tail width at its base
constexpr int kBubbleTailH = 12;         // tail height (points down at the pet)
constexpr int kBubbleGapPx = 4;          // gap between tail tip and the pet
constexpr int kBubbleMinTicks = 200;     // ~20s at 100ms per tick
constexpr int kBubbleMaxTicks = 600;     // ~60s
constexpr int kBubbleShowTicks = 30;     // ~3s visible

std::wstring EmojiName(int index) {
    std::wstring name = std::to_wstring(index);
    if (name.size() == 1) {
        name = L"0" + name;
    }
    return L"emoji_" + name + L".png";
}

// emoji/emoji_01.png .. emoji_NN.png (stop at the first missing file).
std::vector<std::wstring> LoadEmojiPaths(const std::wstring& exeDir,
                                         const std::wstring& assetsDir) {
    std::vector<std::wstring> paths;
    for (int i = 1; i <= static_cast<int>(kMaxFrames); ++i) {
        const std::wstring path =
            exeDir + assetsDir + L"\\emoji\\" + EmojiName(i);
        if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
            break;
        }
        paths.push_back(path);
    }
    return paths;
}

void AddRoundedRect(gdi::GraphicsPath& path, int x, int y, int w, int h,
                    int radius) {
    const int d = radius * 2;
    path.AddArc(x, y, d, d, 180.0f, 90.0f);
    path.AddArc(x + w - d, y, d, d, 270.0f, 90.0f);
    path.AddArc(x + w - d, y + h - d, d, d, 0.0f, 90.0f);
    path.AddArc(x, y + h - d, d, d, 90.0f, 90.0f);
    path.CloseFigure();
}

// Render the bubble (rounded body + downward tail + emoji) into g_bubbleHbmp.
void RenderBubble(const std::wstring& emojiPath) {
    if (g_bubbleDC == nullptr) {
        return;
    }
    gdi::Bitmap canvas(g_bubbleW, g_bubbleH, PixelFormat32bppARGB);
    if (canvas.GetLastStatus() != gdi::Ok) {
        return;
    }
    const int bodyW = g_bubbleW;
    const int bodyH = g_bubbleH - kBubbleTailH;
    {
        gdi::Graphics graphics(&canvas);
        graphics.SetSmoothingMode(gdi::SmoothingModeAntiAlias);
        graphics.SetInterpolationMode(gdi::InterpolationModeHighQualityBicubic);

        gdi::SolidBrush fill(gdi::Color(255, 255, 255, 255));
        gdi::Pen border(gdi::Color(255, 130, 130, 130), 1.0f);
        gdi::GraphicsPath body;
        AddRoundedRect(body, 0, 0, bodyW - 1, bodyH - 1, kBubbleRadiusPx);
        graphics.FillPath(&fill, &body);
        graphics.DrawPath(&border, &body);

        const int cx = bodyW / 2;
        gdi::Point tail[3] = {
            gdi::Point(cx - kBubbleTailW / 2, bodyH - 2),
            gdi::Point(cx + kBubbleTailW / 2, bodyH - 2),
            gdi::Point(cx, g_bubbleH - 1),
        };
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
            graphics.DrawImage(&emoji, (bodyW - drawW) / 2, (bodyH - drawH) / 2,
                               drawW, drawH);
        }
    }
    HBITMAP hbmp = nullptr;
    if (canvas.GetHBITMAP(gdi::Color(0, 0, 0, 0), &hbmp) == gdi::Ok &&
        hbmp != nullptr) {
        if (g_bubbleHbmp != nullptr) {
            DeleteObject(g_bubbleHbmp);
        }
        g_bubbleHbmp = hbmp;
    }
}

// Push the rendered bubble to the layered window at (x, y).
void PresentBubble(int x, int y) {
    if (g_bubbleDC == nullptr || g_bubbleHbmp == nullptr ||
        g_bubbleHwnd == nullptr) {
        return;
    }
    POINT destPos = { x, y };
    SIZE size = { g_bubbleW, g_bubbleH };
    POINT srcPos = { 0, 0 };
    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.BlendFlags = 0;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    const HGDIOBJ previous = SelectObject(g_bubbleDC, g_bubbleHbmp);
    UpdateLayeredWindow(g_bubbleHwnd, nullptr, &destPos, &size, g_bubbleDC,
                        &srcPos, 0, &blend, ULW_ALPHA);
    SelectObject(g_bubbleDC, previous);
}

pet_bubble::Rect PetRect() {
    RECT rect = {};
    GetWindowRect(g_petHwnd, &rect);
    return pet_bubble::Rect{ rect.left, rect.top, g_petW, g_petH };
}

void FollowBubble() {
    if (!g_bubbleVisible || g_bubbleHwnd == nullptr) {
        return;
    }
    const pet_bubble::Point at = pet_bubble::PlaceBubble(
        PetRect(), g_bubbleW, g_bubbleH, g_screenW, g_screenH, kBubbleGapPx);
    PresentBubble(at.x, at.y);
}

void HideBubble() {
    if (g_bubbleHwnd != nullptr) {
        ShowWindow(g_bubbleHwnd, SW_HIDE);
    }
    g_bubbleVisible = false;
}

int NextBubbleInterval() {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return pet_bubble::NextIntervalTicks(kBubbleMinTicks, kBubbleMaxTicks,
                                         dist(Rng()));
}

void ShowBubble() {
    if (g_emojiPaths.empty() || g_bubbleHwnd == nullptr) {
        return;
    }
    const int pick = RandomInRange(0, static_cast<int>(g_emojiPaths.size()) - 1);
    RenderBubble(g_emojiPaths[static_cast<size_t>(pick)]);
    const pet_bubble::Point at = pet_bubble::PlaceBubble(
        PetRect(), g_bubbleW, g_bubbleH, g_screenW, g_screenH, kBubbleGapPx);
    PresentBubble(at.x, at.y);
    ShowWindow(g_bubbleHwnd, SW_SHOWNOACTIVATE);
    g_bubbleVisible = true;
    g_bubbleShowLeft = kBubbleShowTicks;
}

void BubbleTick() {
    if (g_bubbleVisible) {
        g_bubbleShowLeft -= 1;
        if (g_bubbleShowLeft <= 0) {
            HideBubble();
            g_bubbleCountdown = NextBubbleInterval();
        } else {
            FollowBubble();
        }
    } else {
        g_bubbleCountdown -= 1;
        if (g_bubbleCountdown <= 0) {
            ShowBubble();
        }
    }
}

LRESULT CALLBACK BubbleWndProc(HWND hwnd, UINT message, WPARAM wParam,
                               LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        g_bubbleDC = CreateCompatibleDC(nullptr);
        return 0;

    case WM_TIMER:
        if (wParam == kBubbleTimerId) {
            BubbleTick();
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
        if (g_bubbleDC != nullptr) {
            DeleteDC(g_bubbleDC);
            g_bubbleDC = nullptr;
        }
        if (g_bubbleHbmp != nullptr) {
            DeleteObject(g_bubbleHbmp);
            g_bubbleHbmp = nullptr;
        }
        g_bubbleHwnd = nullptr;
        return 0;

    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

}  // namespace

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    // argv[1] optionally forces a single base action; with no argument the pet
    // uses a random idle pool ("idle" + "idle2") and switches between them.
    const bool explicitAction =
        (__argc > 1 && __argv != nullptr && __argv[1] != nullptr);
    std::string actionUtf8 = explicitAction ? __argv[1] : "idle";
    // Optional argv[2] overrides the assets directory name (default
    // "desktop_pet_image"), resolved relative to the executable directory.
    std::string assetsUtf8 = "desktop_pet_image";
    if (__argc > 2 && __argv != nullptr && __argv[2] != nullptr) {
        assetsUtf8 = __argv[2];
    }

    // Narrow (UTF-8) arguments -> wide strings via MultiByteToWideChar.
    std::wstring assetsDir = WideFromUtf8(assetsUtf8.c_str());
    if (assetsDir.empty()) {
        assetsDir = kAssetsDir;
    }

    GdiplusSession gdiplus;
    if (!gdiplus.ok()) {
        ShowFatalError(L"GDI+ failed to initialize.");
        return 1;
    }

    const std::wstring exeDir = GetExecutableDir();
    const std::wstring assetsPath = exeDir + assetsDir;

    if (explicitAction) {
        std::wstring action = WideFromUtf8(actionUtf8.c_str());
        if (action.empty()) {
            action = kDefaultAction;
        }
        std::vector<Frame> frames = LoadFrames(exeDir, assetsDir, action);
        if (frames.empty()) {
            ShowFatalError(
                L"No frames found for action '" + action + L"'.\n\n"
                L"Assets folder:\n" + assetsPath + L"\n\n"
                L"Available actions: " + AvailableActions(assetsPath));
            return 1;
        }
        std::printf("[pet] action=%s loaded=%zu/%zu frames\n",
                    actionUtf8.c_str(), frames.size(), kMaxFrames);
        g_basePool.push_back(std::move(frames));
    } else {
        for (const char* nameUtf8 : { "idle", "idle2" }) {
            const std::wstring name = WideFromUtf8(nameUtf8);
            std::vector<Frame> frames = LoadFrames(exeDir, assetsDir, name);
            if (frames.empty()) {
                std::printf("[pet] idle '%s' not found, skipped\n", nameUtf8);
                continue;
            }
            std::printf("[pet] idle '%s' loaded=%zu/%zu frames\n",
                        nameUtf8, frames.size(), kMaxFrames);
            g_basePool.push_back(std::move(frames));
        }
        if (g_basePool.empty()) {
            ShowFatalError(
                L"No idle frames found (looked for idle/ and idle2/).\n\n"
                L"Assets folder:\n" + assetsPath + L"\n\n"
                L"Available actions: " + AvailableActions(assetsPath));
            return 1;
        }
        g_basePoolIndex = PickRandomBaseIndex(static_cast<int>(g_basePool.size()));
        std::printf("[pet] random idle enabled across %zu animations\n",
                    g_basePool.size());
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

    g_walkRightFrames = LoadFrames(exeDir, assetsDir, L"walking_right");
    g_walkLeftFrames = LoadFrames(exeDir, assetsDir, L"walking_left");
    g_walkUpFrames = LoadFrames(exeDir, assetsDir, L"walking_up");
    g_walkDownFrames = LoadFrames(exeDir, assetsDir, L"walking_down");
    if (g_walkRightFrames.empty() && g_walkLeftFrames.empty() &&
        g_walkUpFrames.empty() && g_walkDownFrames.empty()) {
        std::printf("[pet] no walking_* frames, walking disabled\n");
    } else {
        std::printf("[pet] walking loaded: left=%zu right=%zu up=%zu down=%zu "
                    "frames\n",
                    g_walkLeftFrames.size(), g_walkRightFrames.size(),
                    g_walkUpFrames.size(), g_walkDownFrames.size());
    }

    g_shyFrames = LoadFrames(exeDir, assetsDir, L"shy");
    if (g_shyFrames.empty()) {
        std::printf("[pet] no 'shy' frames, head-sweep reaction disabled\n");
    } else {
        std::printf("[pet] shy loaded=%zu/%zu frames\n",
                    g_shyFrames.size(), kMaxFrames);
    }

    g_emojiPaths = LoadEmojiPaths(exeDir, assetsDir);
    if (g_emojiPaths.empty()) {
        std::printf("[pet] no emoji stickers, bubble disabled\n");
    } else {
        std::printf("[pet] emoji loaded=%zu stickers\n", g_emojiPaths.size());
    }

    g_behaviorTarget = RandomInRange(kIdleMinTicks, kIdleMaxTicks);

    // The window size adapts to the first base frame's pixel dimensions.
    BITMAP firstFrame = {};
    if (GetObjectW(g_basePool[0][0].hbmp, static_cast<int>(sizeof(BITMAP)),
                   &firstFrame) == 0 ||
        firstFrame.bmWidth <= 0 || firstFrame.bmHeight <= 0) {
        ShowFatalError(L"Could not read the first frame's dimensions.");
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
        ShowFatalError(L"RegisterClassW failed (error " +
                       std::to_wstring(GetLastError()) + L").");
        return 1;
    }

    bool bubbleClassReady = false;
    {
        WNDCLASSW bubbleClass = {};
        bubbleClass.style = CS_HREDRAW | CS_VREDRAW;
        bubbleClass.lpfnWndProc = BubbleWndProc;
        bubbleClass.hInstance = hInstance;
        bubbleClass.hCursor =
            LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
        bubbleClass.lpszClassName = kBubbleClassName;
        bubbleClassReady = RegisterClassW(&bubbleClass) != 0;
    }

    g_screenW = GetSystemMetrics(SM_CXSCREEN);
    g_screenH = GetSystemMetrics(SM_CYSCREEN);
    const int startX = (g_screenW - g_petW) / 2;
    const int startY = (g_screenH - g_petH) / 2;

    HWND hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        kClassName,
        L"Desktop Pet",
        WS_POPUP,
        startX, startY, g_petW, g_petH,
        nullptr, nullptr, hInstance, nullptr);
    if (hwnd == nullptr) {
        ShowFatalError(L"CreateWindowExW failed (error " +
                       std::to_wstring(GetLastError()) + L").");
        return 1;
    }

    SetTimer(hwnd, kTimerId, kTimerElapseMs, nullptr);

    // Speech-bubble emoji window: an independent layered window owned by the pet.
    g_petHwnd = hwnd;
    if (bubbleClassReady && !g_emojiPaths.empty()) {
        g_bubbleW = kBubbleEmojiSizePx + 2 * kBubblePadPx;
        g_bubbleH = kBubbleEmojiSizePx + 2 * kBubblePadPx + kBubbleTailH;
        g_bubbleHwnd = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW |
                WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
            kBubbleClassName, L"Desktop Pet Bubble", WS_POPUP, 0, 0, g_bubbleW,
            g_bubbleH, hwnd, nullptr, hInstance, nullptr);
        if (g_bubbleHwnd != nullptr) {
            SetTimer(g_bubbleHwnd, kBubbleTimerId, kBubbleTimerElapseMs, nullptr);
            g_bubbleCountdown = NextBubbleInterval();
            std::printf("[pet] bubble enabled (%dx%d, %zu stickers)\n",
                        g_bubbleW, g_bubbleH, g_emojiPaths.size());
        } else {
            std::printf("[pet] bubble window create failed, bubble disabled\n");
        }
    }

    PresentBaseFrame(hwnd);
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    SetFocus(hwnd);

    MSG message = {};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}
