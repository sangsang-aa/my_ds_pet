// Speech-bubble emoji window: shows a random sticker from
// desktop_pet_image\emoji\emoji_NN.png above the pet's head every so often.
//
// It runs on its own timer and follows the pet by polling the pet window's
// rect, so the pet's own timer and state machine are never touched.
// WS_EX_TRANSPARENT makes it click-through: mouse input still reaches the pet.
#ifndef PET_BUBBLE_H
#define PET_BUBBLE_H

#include <windows.h>

#include <string>
#include <vector>

struct Bubble {
    std::vector<std::wstring> emojiPaths;
    std::wstring shownEmoji;
    bool tailUp = false;
    HWND hwnd = nullptr;
    HWND petHwnd = nullptr;
    HDC dc = nullptr;
    HBITMAP hbmp = nullptr;
    int w = 0;
    int h = 0;
    int countdown = 0;
    int showLeft = 0;
    bool visible = false;

    // Loads the emoji list, registers the class and creates the hidden bubble
    // window (owned by petHwnd). Returns false when there is nothing to show.
    bool Init(HINSTANCE hInstance, HWND petWindow, const std::wstring& exeDir,
              const std::wstring& assetsDir);

    void OnTimer();
    void OnDestroy();
};

LRESULT CALLBACK BubbleWndProc(HWND hwnd, UINT message, WPARAM wParam,
                               LPARAM lParam);

#endif  // PET_BUBBLE_H
