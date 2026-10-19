// Notification-area tray icon with a two-entry control-panel menu.
#ifndef PET_TRAY_H
#define PET_TRAY_H

#include <windows.h>

#include <functional>
#include <string>

namespace tray {

struct Callbacks {
    std::function<void()> onResize;  // open the scale dialog
    std::function<void()> onQuit;    // quit the app
};

// Add the tray icon (using iconPath as the icon). Returns false when the icon
// cannot be decoded or the shell rejects the entry; safe to call once.
bool Init(HWND owner, HINSTANCE instance, const std::wstring& iconPath,
          Callbacks callbacks);

// Forward kTrayCallbackMessage from the owner's window procedure.
void OnCallback(HWND owner, LPARAM lParam);

// Remove the icon and free it. Idempotent, safe when Init failed or was never
// called.
void Destroy();

}  // namespace tray

#endif  // PET_TRAY_H
