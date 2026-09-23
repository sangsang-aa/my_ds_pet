// Desktop pet demo — WinMain wiring only.
//
// A transparent, always-on-top floating window plays a PNG frame sequence
// (desktop_pet_image\<action>\000.png ..). The pet's behaviour lives in Pet
// (pet.cpp), the emoji speech bubble in Bubble (bubble.cpp); this file just
// loads assets, creates the windows and runs the message loop.
//
// Build (MSVC):
//   cl /nologo /EHsc /std:c++17 /O2 src\*.cpp gdiplus.lib user32.lib gdi32.lib /link /SUBSYSTEM:WINDOWS /OUT:pet_demo.exe /I src
// Build (MinGW-w64):
//   x86_64-w64-mingw32-g++ -std=c++17 -O2 -mwindows src/*.cpp -lgdiplus -luser32 -lgdi32 -o pet_demo.exe

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include <string>

#include "bubble/bubble.h"
#include "core/config.h"
#include "core/render.h"
#include "core/util.h"
#include "pet/pet.h"

// __argc / __argv are exported by both the MSVC and the MinGW-w64 CRT, so the
// GUI entry point can still see the (narrow) command-line arguments.
extern "C" {
extern int __argc;
extern char** __argv;
}

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

    Pet pet;
    std::wstring error;
    if (!pet.Load(exeDir, assetsDir, explicitAction, actionUtf8, error)) {
        ShowFatalError(error);
        return 1;
    }
    if (!pet.InitGeometry(GetSystemMetrics(SM_CXSCREEN),
                          GetSystemMetrics(SM_CYSCREEN), error)) {
        ShowFatalError(error);
        return 1;
    }

    WNDCLASSW windowClass = {};
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = PetWndProc;
    windowClass.hInstance = hInstance;
    windowClass.hCursor =
        LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    windowClass.lpszClassName = kClassName;
    if (!RegisterClassW(&windowClass)) {
        ShowFatalError(L"RegisterClassW failed (error " +
                       std::to_wstring(GetLastError()) + L").");
        return 1;
    }

    const int startX = (pet.screenW - pet.petW) / 2;
    const int startY = (pet.screenH - pet.petH) / 2;
    HWND hwnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOPMOST |
                                    WS_EX_TOOLWINDOW,
                                kClassName, L"Desktop Pet", WS_POPUP, startX,
                                startY, pet.petW, pet.petH, nullptr, nullptr,
                                hInstance, &pet);
    if (hwnd == nullptr) {
        ShowFatalError(L"CreateWindowExW failed (error " +
                       std::to_wstring(GetLastError()) + L").");
        return 1;
    }

    SetTimer(hwnd, kTimerId, kTimerElapseMs, nullptr);

    Bubble bubble;
    bubble.Init(hInstance, hwnd, exeDir, assetsDir);

    pet.PresentBase(hwnd);
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    SetFocus(hwnd);

    MSG message = {};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}
