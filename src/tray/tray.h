// System-tray icon and a horizontal toolbar for the desktop pet.
//
// The Tray owns a hidden message-only window that receives Shell_NotifyIcon
// callbacks, plus a small layered window that draws a horizontal button bar
// (GDI+, like the bubble). Right-clicking the pet or the tray icon shows the
// bar just below the pet; clicking a button runs an action supplied by WinMain
// as a callback. Keeping the bar self-drawn avoids the native TrackPopupMenu
// (which can only stack items vertically) and lets us control z-order so the
// pet's periodic topmost re-assert cannot cover it.
#ifndef PET_TRAY_H
#define PET_TRAY_H

#include <windows.h>
#include <shellapi.h>  // NOTIFYICONDATAW, Shell_NotifyIconW, NIF_*

#include <functional>
#include <string>

constexpr wchar_t kTrayClassName[] = L"DesktopPetTrayClass";
constexpr wchar_t kBarClassName[] = L"DesktopPetBarClass";
constexpr UINT kTrayNotifyMsg = WM_APP + 1;  // Shell_NotifyIcon callback message

// Toolbar button ids (same values as the old menu command ids).
constexpr UINT_PTR kTrayMenuToggle = 1;
constexpr UINT_PTR kTrayMenuAbout = 2;
constexpr UINT_PTR kTrayMenuActivity = 3;  // placeholder, drawn greyed out
constexpr UINT_PTR kTrayMenuQuit = 4;

struct TrayActions {
    std::function<void(bool show)> toggleVisible;  // show/hide pet (and bubble)
    std::function<void()> about;
    std::function<void()> quit;
    // Called when the bar opens/closes so the pet can pause/resume its periodic
    // HWND_TOPMOST re-assert (the root cause of the bar being covered).
    std::function<void(bool paused)> pauseTopmost;
};

struct Tray {
    HINSTANCE instance = nullptr;
    HWND hwnd = nullptr;      // hidden window receiving tray callback messages
    HWND petHwnd = nullptr;   // visible pet window, used to anchor the bar
    HICON icon = nullptr;
    NOTIFYICONDATAW nid = {};
    TrayActions actions;
    bool registered = false;
    bool ownsIcon = false;    // false when falling back to the shared app icon
    bool petVisible = true;   // mirrors the pet window's visibility

    // Horizontal toolbar (self-drawn layered window).
    HWND barHwnd = nullptr;
    HDC barDC = nullptr;
    HBITMAP barBitmap = nullptr;
    int barW = 0;
    int barH = 0;
    bool barShown = false;

    // Build the icon from the first frame PNG (alpha preserved), register the
    // tray icon and create both windows. Falls back to the shared application
    // icon when the frame cannot be loaded, so a bad asset never blocks startup.
    bool Init(HINSTANCE hInstance, HWND petWindow,
              const std::wstring& firstFramePng,
              const TrayActions& trayActions, std::wstring& error);

    void ShowBar();                         // show bar below the pet
    void HideBar();                         // hide bar and resume topmost
    void FollowPet();                       // reposition an open bar under the pet
    void PlaceBar();                        // draw the bar at its anchored spot
    void HandleBarClick(int x, int y);      // client coords -> button command
    void HandleCommand(UINT_PTR command);   // act on a button selection
    void SetPetVisible(bool visible) { petVisible = visible; }
    void Shutdown();                        // NIM_DELETE + free every resource
};

LRESULT CALLBACK TrayWndProc(HWND hwnd, UINT message, WPARAM wParam,
                             LPARAM lParam);
LRESULT CALLBACK BarWndProc(HWND hwnd, UINT message, WPARAM wParam,
                            LPARAM lParam);

#endif  // PET_TRAY_H
