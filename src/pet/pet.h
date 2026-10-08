// The pet: an idle pool plus a registry-driven behavior player, input handling,
// and its window procedure.
#ifndef PET_PET_H
#define PET_PET_H

#include <windows.h>

#include <string>
#include <vector>

#include "behavior.h"
#include "core/assets.h"
#include "core/config.h"

enum class PetState { Base, Acting };

struct Pet {
    struct Clip {
        const pet_behavior::Spec* spec = nullptr;
        std::vector<Frame> frames;
    };

    std::vector<std::vector<Frame>> basePool;  // idle / idle2 pool
    std::vector<Clip> clips;                   // one per registry spec
    std::vector<int> cooldowns;                // per-spec cooldown ticks

    int basePoolIndex = 0;
    int baseIndex = 0;

    int activeClip = -1;
    int activeIndex = 0;
    int activeTicks = 0;
    int activeTarget = 0;  // 0 => no time limit (ended by an event)

    PetState state = PetState::Base;

    int behaviorTicks = 0;
    int behaviorTarget = 0;
    int screenW = 0;
    int screenH = 0;
    int waitTicks = 0;
    int headSweepPx = 0;
    int lastMouseX = 0;
    int lastMouseY = 0;

    bool mouseDown = false;
    bool dragging = false;
    POINT mouseDownPos = {};
    int dragOffsetX = 0;
    int dragOffsetY = 0;

    HDC memDC = nullptr;
    int petW = 0;
    int petH = 0;

    // Load every behavior in the registry (plus the idle pool) relative to
    // (exeDir, assetsDir). On failure returns false and fills `error`.
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
    int PickNextAuto() const;
    bool CanStart(int clipIndex) const;
    void StartBehavior(HWND hwnd, int clipIndex);
    int PickRandomBaseIndex(int poolSize) const;
    void EnterBase(HWND hwnd);
    void EnterWaiting(HWND hwnd);
};

LRESULT CALLBACK PetWndProc(HWND hwnd, UINT message, WPARAM wParam,
                            LPARAM lParam);

#endif  // PET_PET_H
