// The pet itself: animation pools, the idle<->walk behaviour state machine,
// drag/click/shy reactions, and its window procedure.
#ifndef PET_PET_H
#define PET_PET_H

#include <windows.h>

#include <string>
#include <vector>

#include "assets.h"
#include "config.h"

enum class WalkDir { Left, Right, Up, Down };
enum class PetState { Base, Clicked, Dragging, Walking, Shy };

struct Pet {
    std::vector<std::vector<Frame>> basePool;
    std::vector<Frame> clickFrames;
    std::vector<Frame> dragFrames;
    std::vector<Frame> walkLeftFrames;
    std::vector<Frame> walkRightFrames;
    std::vector<Frame> walkUpFrames;
    std::vector<Frame> walkDownFrames;
    std::vector<Frame> shyFrames;

    int basePoolIndex = 0;
    int baseIndex = 0;
    int clickIndex = 0;
    int dragIndex = 0;
    int walkIndex = 0;
    int walkStep = 1;  // +1 / -1 frame step for boomerang playback
    bool walkBoomerang = false;
    bool walkFinishing = false;
    WalkDir walkDir = WalkDir::Right;
    int shyIndex = 0;

    PetState state = PetState::Base;

    int behaviorTicks = 0;
    int behaviorTarget = 0;
    int screenW = 0;
    int screenH = 0;
    int waitTicks = 0;
    int headSweepPx = 0;
    int lastMouseX = 0;
    int lastMouseY = 0;
    int shyCooldownTicks = 0;

    bool mouseDown = false;
    bool dragging = false;
    POINT mouseDownPos = {};
    int dragOffsetX = 0;
    int dragOffsetY = 0;

    HDC memDC = nullptr;
    int petW = 0;
    int petH = 0;

    // Load every animation relative to (exeDir, assetsDir). On failure returns
    // false and fills `error` with the message to show the user.
    bool Load(const std::wstring& exeDir, const std::wstring& assetsDir,
              bool explicitAction, const std::string& actionUtf8,
              std::wstring& error);

    // Derive the window size from the first base frame and remember the screen.
    bool InitGeometry(int screenW, int screenH, std::wstring& error);

    void PresentFrame(HWND hwnd, const Frame& frame);
    void PresentBase(HWND hwnd);

    void OnTimer(HWND hwnd);
    void OnLButtonDown(HWND hwnd);
    void OnMouseMove(HWND hwnd, LPARAM lParam);
    void OnLButtonUp(HWND hwnd);
    void OnCaptureChanged(HWND hwnd);
    void DestroyResources();

private:
    const std::vector<Frame>& WalkFrames(WalkDir dir) const;
    WalkDir PickRandomWalkDir() const;
    int PickRandomBaseIndex(int poolSize) const;
    void WalkStep(WalkDir dir, int& dx, int& dy) const;
    void AdvanceWalkIndex(int size);
    void EnterBase(HWND hwnd);
    void EnterWaiting(HWND hwnd);
    void TriggerClickReaction(HWND hwnd);
    void TriggerShyReaction(HWND hwnd);
};

LRESULT CALLBACK PetWndProc(HWND hwnd, UINT message, WPARAM wParam,
                            LPARAM lParam);

#endif  // PET_PET_H
