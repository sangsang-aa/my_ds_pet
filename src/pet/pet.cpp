#include "pet.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "walk_logic.h"
#include "core/render.h"
#include "core/util.h"

namespace {

void FreeFrames(std::vector<Frame>& frames) {
    for (const Frame& frame : frames) {
        if (frame.hbmp != nullptr) {
            DeleteObject(frame.hbmp);
        }
    }
    frames.clear();
}

}  // namespace

bool Pet::Load(const std::wstring& exeDir, const std::wstring& assetsDir,
               bool explicitAction, const std::string& actionUtf8,
               std::wstring& error) {
    const std::wstring assetsPath = exeDir + assetsDir;

    if (explicitAction) {
        std::wstring action = WideFromUtf8(actionUtf8.c_str());
        if (action.empty()) {
            action = kDefaultAction;
        }
        std::vector<Frame> frames = LoadFrames(exeDir, assetsDir, action);
        if (frames.empty()) {
            error = L"No frames found for action '" + action + L"'.\n\n"
                    L"Assets folder:\n" + assetsPath + L"\n\n"
                    L"Available actions: " + AvailableActions(assetsPath);
            return false;
        }
        std::printf("[pet] action=%s loaded=%zu/%zu frames\n", actionUtf8.c_str(),
                    frames.size(), kMaxFrames);
        basePool.push_back(std::move(frames));
    } else {
        for (const char* nameUtf8 : { "idle", "idle2" }) {
            const std::wstring name = WideFromUtf8(nameUtf8);
            std::vector<Frame> frames = LoadFrames(exeDir, assetsDir, name);
            if (frames.empty()) {
                std::printf("[pet] idle '%s' not found, skipped\n", nameUtf8);
                continue;
            }
            std::printf("[pet] idle '%s' loaded=%zu/%zu frames\n", nameUtf8,
                        frames.size(), kMaxFrames);
            basePool.push_back(std::move(frames));
        }
        if (basePool.empty()) {
            error = L"No idle frames found (looked for idle/ and idle2/).\n\n"
                    L"Assets folder:\n" + assetsPath + L"\n\n"
                    L"Available actions: " + AvailableActions(assetsPath);
            return false;
        }
        basePoolIndex = PickRandomBaseIndex(static_cast<int>(basePool.size()));
        std::printf("[pet] random idle enabled across %zu animations\n",
                    basePool.size());
    }

    clickFrames = LoadFrames(exeDir, assetsDir, L"clicked");
    if (clickFrames.empty()) {
        std::printf("[pet] no 'clicked' frames, click reaction disabled\n");
    } else {
        std::printf("[pet] clicked loaded=%zu/%zu frames\n", clickFrames.size(),
                    kMaxFrames);
    }

    dragFrames = LoadFrames(exeDir, assetsDir, L"drag");
    if (dragFrames.empty()) {
        std::printf("[pet] no 'drag' frames, drag animation disabled\n");
    } else {
        std::printf("[pet] drag loaded=%zu/%zu frames\n", dragFrames.size(),
                    kMaxFrames);
    }

    walkRightFrames = LoadFrames(exeDir, assetsDir, L"walking_right");
    walkLeftFrames = LoadFrames(exeDir, assetsDir, L"walking_left");
    walkUpFrames = LoadFrames(exeDir, assetsDir, L"walking_up");
    walkDownFrames = LoadFrames(exeDir, assetsDir, L"walking_down");
    if (walkRightFrames.empty() && walkLeftFrames.empty() &&
        walkUpFrames.empty() && walkDownFrames.empty()) {
        std::printf("[pet] no walking_* frames, walking disabled\n");
    } else {
        std::printf("[pet] walking loaded: left=%zu right=%zu up=%zu down=%zu "
                    "frames\n",
                    walkLeftFrames.size(), walkRightFrames.size(),
                    walkUpFrames.size(), walkDownFrames.size());
    }

    shyFrames = LoadFrames(exeDir, assetsDir, L"shy");
    if (shyFrames.empty()) {
        std::printf("[pet] no 'shy' frames, head-sweep reaction disabled\n");
    } else {
        std::printf("[pet] shy loaded=%zu/%zu frames\n", shyFrames.size(),
                    kMaxFrames);
    }

    behaviorTarget = RandomInRange(kIdleMinTicks, kIdleMaxTicks);
    return true;
}

bool Pet::InitGeometry(int screenWidth, int screenHeight, std::wstring& error) {
    BITMAP firstFrame = {};
    if (GetObjectW(basePool[0][0].hbmp, static_cast<int>(sizeof(BITMAP)),
                   &firstFrame) == 0 ||
        firstFrame.bmWidth <= 0 || firstFrame.bmHeight <= 0) {
        error = L"Could not read the first frame's dimensions.";
        return false;
    }
    petW = firstFrame.bmWidth;
    petH = firstFrame.bmHeight;
    screenW = screenWidth;
    screenH = screenHeight;
    return true;
}

void Pet::PresentFrame(HWND hwnd, const Frame& frame) {
    if (memDC == nullptr || frame.hbmp == nullptr) {
        return;
    }
    RECT windowRect = {};
    GetWindowRect(hwnd, &windowRect);
    render::PresentLayered(hwnd, memDC, frame.hbmp, windowRect.left,
                           windowRect.top, petW, petH, frame.hasAlpha);
}

void Pet::PresentBase(HWND hwnd) {
    if (basePool.empty()) {
        return;
    }
    const std::vector<Frame>& base =
        basePool[static_cast<size_t>(basePoolIndex)];
    if (base.empty()) {
        return;
    }
    PresentFrame(hwnd, base[static_cast<size_t>(baseIndex) % base.size()]);
}

int Pet::PickRandomBaseIndex(int poolSize) const {
    return RandomInRange(0, poolSize - 1);
}

const std::vector<Frame>& Pet::WalkFrames(WalkDir dir) const {
    switch (dir) {
    case WalkDir::Left:
        return walkLeftFrames;
    case WalkDir::Up:
        return walkUpFrames;
    case WalkDir::Down:
        return walkDownFrames;
    case WalkDir::Right:
    default:
        return walkRightFrames;
    }
}

void Pet::WalkStep(WalkDir dir, int& dx, int& dy) const {
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
WalkDir Pet::PickRandomWalkDir() const {
    const WalkDir all[4] = { WalkDir::Left, WalkDir::Right, WalkDir::Up,
                             WalkDir::Down };
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

void Pet::AdvanceWalkIndex(int size) {
    if (size <= 0) {
        return;
    }
    walkIndex = (walkIndex + 1) % size;
}

// Return to idle and schedule the next walk after a random dwell.
void Pet::EnterBase(HWND hwnd) {
    state = PetState::Base;
    behaviorTicks = 0;
    behaviorTarget = RandomInRange(kIdleMinTicks, kIdleMaxTicks);
    PresentBase(hwnd);
}

// After the user drops the pet: idle in place (no walking) for a random 2-3
// minutes, then resume the normal idle<->walk chain.
void Pet::EnterWaiting(HWND hwnd) {
    EnterBase(hwnd);
    waitTicks = RandomInRange(kWaitMinTicks, kWaitMaxTicks);
}

// Restart the one-shot clicked animation from its first frame.
void Pet::TriggerClickReaction(HWND hwnd) {
    if (clickFrames.empty()) {
        return;  // click reaction was disabled at startup
    }
    state = PetState::Clicked;
    clickIndex = 0;
    PresentFrame(hwnd, clickFrames[0]);
}

// Play the one-shot shy animation (mouse sweeping across the pet's head).
void Pet::TriggerShyReaction(HWND hwnd) {
    if (shyFrames.empty() || shyCooldownTicks > 0) {
        return;
    }
    state = PetState::Shy;
    shyIndex = 0;
    shyCooldownTicks = kShyCooldownTicks;
    PresentFrame(hwnd, shyFrames[0]);
}

void Pet::OnTimer(HWND hwnd) {
    if (shyCooldownTicks > 0) {
        shyCooldownTicks -= 1;
    }
    headSweepPx -= headSweepPx / 2;  // decay, so slow drift never triggers
    if (state == PetState::Clicked) {
        clickIndex += 1;
        if (clickIndex >= static_cast<int>(clickFrames.size())) {
            // clicked animation finished: resume the idle loop
            EnterBase(hwnd);
        } else {
            PresentFrame(hwnd, clickFrames[static_cast<size_t>(clickIndex)]);
        }
    } else if (state == PetState::Shy) {
        shyIndex += 1;
        if (shyIndex >= static_cast<int>(shyFrames.size())) {
            EnterBase(hwnd);
        } else {
            PresentFrame(hwnd, shyFrames[static_cast<size_t>(shyIndex)]);
        }
    } else if (state == PetState::Dragging) {
        // drag reaction loops while the pet is being dragged
        if (!dragFrames.empty()) {
            dragIndex = (dragIndex + 1) % static_cast<int>(dragFrames.size());
            PresentFrame(hwnd, dragFrames[static_cast<size_t>(dragIndex)]);
        }
    } else if (state == PetState::Walking) {
        const std::vector<Frame>& walk = WalkFrames(walkDir);
        if (walk.empty()) {
            EnterBase(hwnd);
        } else {
            AdvanceWalkIndex(static_cast<int>(walk.size()));
            PresentFrame(hwnd, walk[static_cast<size_t>(walkIndex)]);
            RECT rect = {};
            GetWindowRect(hwnd, &rect);
            int dx = 0;
            int dy = 0;
            WalkStep(walkDir, dx, dy);
            const int newLeft = rect.left + dx;
            const int newTop = rect.top + dy;
            const bool atEdge =
                (newLeft < 0) || (newLeft + petW > screenW) ||
                (newTop < 0) || (newTop + petH > screenH);
            if (!atEdge) {
                SetWindowPos(hwnd, nullptr, newLeft, newTop, 0, 0,
                             SWP_NOSIZE | SWP_NOZORDER);
            }
            behaviorTicks += 1;
            const bool walkDone = atEdge || behaviorTicks >= behaviorTarget;
            // A full-cycle vertical clip (takeoff..landing) must reach its last,
            // grounded frame before returning to idle; stopping mid-air — or on
            // the pre-takeoff frame — reads as a jump or a missing landing.
            const bool safeToStop =
                !walkFullCycle ||
                pet_walk::AtEndFrame(walkIndex, static_cast<int>(walk.size()));
            if (walkDone && safeToStop) {
                EnterBase(hwnd);
            }
        }
    } else {  // Base
        if (!basePool.empty()) {
            const int size = static_cast<int>(
                basePool[static_cast<size_t>(basePoolIndex)].size());
            if (size > 0) {
                baseIndex += 1;
                if (baseIndex >= size) {
                    // one idle loop finished: switch to a random idle
                    baseIndex = 0;
                    if (basePool.size() > 1) {
                        basePoolIndex = PickRandomBaseIndex(
                            static_cast<int>(basePool.size()));
                    }
                }
                PresentBase(hwnd);
            }
            if (waitTicks > 0) {
                // post-drag waiting: idle animations only, never walk
                waitTicks -= 1;
                if (waitTicks == 0) {
                    behaviorTicks = 0;
                    behaviorTarget = RandomInRange(kIdleMinTicks, kIdleMaxTicks);
                }
            } else {
                const bool canWalk = !walkLeftFrames.empty() ||
                                     !walkRightFrames.empty() ||
                                     !walkUpFrames.empty() ||
                                     !walkDownFrames.empty();
                behaviorTicks += 1;
                if (canWalk && behaviorTicks >= behaviorTarget) {
                    // idle dwell elapsed: walk in a random direction
                    walkDir = PickRandomWalkDir();
                    walkFullCycle = (walkDir == WalkDir::Up ||
                                     walkDir == WalkDir::Down);
                    walkIndex = 0;
                    behaviorTicks = 0;
                    behaviorTarget = RandomInRange(kWalkMinTicks, kWalkMaxTicks);
                    state = PetState::Walking;
                    PresentFrame(hwnd, WalkFrames(walkDir)[0]);
                }
            }
        }
    }
}

void Pet::OnLButtonDown(HWND hwnd) {
    SetFocus(hwnd);  // make sure the ESC key reaches this window
    mouseDown = true;
    dragging = false;
    GetCursorPos(&mouseDownPos);
    SetCapture(hwnd);
}

void Pet::OnMouseMove(HWND hwnd, LPARAM lParam) {
    if (mouseDown && !dragging) {
        POINT cursor = {};
        GetCursorPos(&cursor);
        const int dx = cursor.x - mouseDownPos.x;
        const int dy = cursor.y - mouseDownPos.y;
        // Small movements are clicks; past the system drag threshold it becomes
        // a drag and the grab point stays under the cursor.
        if (std::abs(dx) >= GetSystemMetrics(SM_CXDRAG) ||
            std::abs(dy) >= GetSystemMetrics(SM_CYDRAG)) {
            dragging = true;
            RECT windowRect = {};
            GetWindowRect(hwnd, &windowRect);
            dragOffsetX = cursor.x - windowRect.left;
            dragOffsetY = cursor.y - windowRect.top;
            // start the drag reaction from its first frame
            if (!dragFrames.empty()) {
                state = PetState::Dragging;
                dragIndex = 0;
                PresentFrame(hwnd, dragFrames[0]);
            }
        }
    }
    if (dragging) {
        POINT cursor = {};
        GetCursorPos(&cursor);
        SetWindowPos(hwnd, nullptr, cursor.x - dragOffsetX,
                     cursor.y - dragOffsetY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }
    // Mouse sweeping horizontally across the head -> shy reaction.
    {
        const int mx = static_cast<short>(LOWORD(lParam));
        const int my = static_cast<short>(HIWORD(lParam));
        if (!mouseDown && my >= 0 && my < petH * kHeadBandPercent / 100 &&
            mx >= 0 && mx < petW) {
            const int dx = mx - lastMouseX;
            const int dy = my - lastMouseY;
            if (std::abs(dx) <= kMaxSweepDeltaPx &&
                std::abs(dx) > std::abs(dy)) {
                headSweepPx += std::abs(dx);
            }
            if (headSweepPx >= kSweepThresholdPx) {
                headSweepPx = 0;
                TriggerShyReaction(hwnd);
            }
        }
        lastMouseX = mx;
        lastMouseY = my;
    }
}

void Pet::OnLButtonUp(HWND hwnd) {
    if (mouseDown && !dragging) {
        TriggerClickReaction(hwnd);
    } else if (dragging) {
        EnterWaiting(hwnd);
    }
    mouseDown = false;
    dragging = false;
    ReleaseCapture();
}

void Pet::OnCaptureChanged(HWND hwnd) {
    // Capture was lost (released or stolen): cancel any in-flight drag.
    mouseDown = false;
    dragging = false;
    if (state == PetState::Dragging) {
        EnterWaiting(hwnd);
    }
}

void Pet::DestroyResources() {
    // Delete the DC first so no frame HBITMAP is still selected into it.
    if (memDC != nullptr) {
        DeleteDC(memDC);
        memDC = nullptr;
    }
    for (std::vector<Frame>& animation : basePool) {
        FreeFrames(animation);
    }
    basePool.clear();
    FreeFrames(clickFrames);
    FreeFrames(dragFrames);
    FreeFrames(walkLeftFrames);
    FreeFrames(walkRightFrames);
    FreeFrames(walkUpFrames);
    FreeFrames(walkDownFrames);
    FreeFrames(shyFrames);
}

LRESULT CALLBACK PetWndProc(HWND hwnd, UINT message, WPARAM wParam,
                            LPARAM lParam) {
    Pet* pet = reinterpret_cast<Pet*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (message) {
    case WM_CREATE: {
        CREATESTRUCTW* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pet = static_cast<Pet*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(pet));
        if (pet != nullptr) {
            pet->memDC = CreateCompatibleDC(nullptr);
        }
        return 0;
    }

    case WM_TIMER:
        if (wParam == kTimerId && pet != nullptr) {
            pet->OnTimer(hwnd);
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
        if (pet != nullptr) {
            pet->OnLButtonDown(hwnd);
        }
        return 0;

    case WM_MOUSEMOVE:
        if (pet != nullptr) {
            pet->OnMouseMove(hwnd, lParam);
        }
        return 0;

    case WM_LBUTTONUP:
        if (pet != nullptr) {
            pet->OnLButtonUp(hwnd);
        }
        return 0;

    case WM_CAPTURECHANGED:
        if (pet != nullptr) {
            pet->OnCaptureChanged(hwnd);
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
        if (pet != nullptr) {
            pet->DestroyResources();
        }
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}
