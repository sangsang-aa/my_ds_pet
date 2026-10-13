#include "pet.h"

#include <cmath>
#include <cstdio>
#include <random>

#include "behaviors.h"
#include "core/render.h"
#include "core/settings.h"
#include "core/util.h"
#include "walk_logic.h"

namespace {

void FreeFrames(std::vector<Frame>& frames) {
    for (const Frame& frame : frames) {
        if (frame.hbmp != nullptr) {
            DeleteObject(frame.hbmp);
        }
    }
    frames.clear();
}

bool IsMoving(const pet_behavior::Spec& spec) {
    return spec.dx != 0 || spec.dy != 0;
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
        // idle2 is deliberately disabled for now, while its V2 animation is
        // being evaluated; drop the extra name here to bring it back.
        for (const char* nameUtf8 : { "idle" }) {
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

    clips.clear();
    for (const pet_behavior::Spec& spec : Behaviors()) {
        Clip clip;
        clip.spec = &spec;
        clip.frames = LoadFrames(exeDir, assetsDir, spec.action);
        std::printf("[pet] %ls loaded=%zu/%zu frames\n", spec.action.c_str(),
                    clip.frames.size(), kMaxFrames);
        clips.push_back(std::move(clip));
    }
    cooldowns.assign(clips.size(), 0);

    behaviorTarget = RandomInRange(GetSettings().idleMinTicks,
                                   GetSettings().idleMaxTicks);
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

// Pick the next autonomous behavior by weight; -1 when none is available
// (frames missing, or gated during the post-drag wait).
int Pet::PickNextAuto() const {
    const std::vector<pet_behavior::Spec>& specs = Behaviors();
    std::vector<bool> eligible(specs.size(), false);
    for (size_t i = 0; i < specs.size(); ++i) {
        eligible[i] = specs[i].trigger == pet_behavior::Trigger::Auto &&
                      !clips[i].frames.empty() &&
                      (!specs[i].gatedByWait || waitTicks == 0);
    }
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return pet_behavior::PickAuto(specs, eligible, dist(Rng()));
}

bool Pet::CanStart(int clipIndex) const {
    return clipIndex >= 0 && clipIndex < static_cast<int>(clips.size()) &&
           !clips[static_cast<size_t>(clipIndex)].frames.empty() &&
           cooldowns[static_cast<size_t>(clipIndex)] <= 0;
}

void Pet::StartBehavior(HWND hwnd, int clipIndex) {
    const Clip& clip = clips[static_cast<size_t>(clipIndex)];
    const pet_behavior::Spec& spec = *clip.spec;
    state = PetState::Acting;
    activeClip = clipIndex;
    activeIndex = 0;
    activeStep = 1;
    activeTicks = 0;
    activeTarget = spec.maxTicks > 0 ? RandomInRange(spec.minTicks, spec.maxTicks)
                                     : 0;
    cooldowns[static_cast<size_t>(clipIndex)] = spec.minTicks;
    PresentFrame(hwnd, clip.frames[0]);
}

// Return to idle and schedule the next autonomous behavior after a random dwell.
void Pet::EnterBase(HWND hwnd) {
    state = PetState::Base;
    behaviorTicks = 0;
    behaviorTarget = RandomInRange(GetSettings().idleMinTicks,
                                   GetSettings().idleMaxTicks);
    PresentBase(hwnd);
}

// After the user drops the pet: idle in place (no autonomous behaviors) for a
// random 2-3 minutes, then resume the normal chain.
void Pet::EnterWaiting(HWND hwnd) {
    EnterBase(hwnd);
    waitTicks = RandomInRange(GetSettings().waitMinTicks,
                              GetSettings().waitMaxTicks);
}

void Pet::OnTimer(HWND hwnd) {
    for (int& cooldown : cooldowns) {
        if (cooldown > 0) {
            cooldown -= 1;
        }
    }
    headSweepPx -= headSweepPx / 2;  // decay, so slow drift never triggers

    if (state == PetState::Acting) {
        const Clip& clip = clips[static_cast<size_t>(activeClip)];
        const std::vector<Frame>& frames = clip.frames;
        const pet_behavior::Spec& spec = *clip.spec;
        const int size = static_cast<int>(frames.size());
        if (size <= 0) {
            EnterBase(hwnd);
            return;
        }

        bool ended = false;
        if (spec.playback == pet_behavior::Playback::Boomerang && size > 2) {
            // Forward then backward, so the clip loops with no jump back to the
            // start (one-way takeoff/landing clips).
            activeIndex += activeStep;
            if (activeIndex >= size) {
                activeStep = -1;
                activeIndex = size - 2;
            } else if (activeIndex < 0) {
                activeStep = 1;
                activeIndex = 1;
            }
        } else {
            activeIndex += 1;
            if (activeIndex >= size) {
                if (spec.playback == pet_behavior::Playback::OneShot) {
                    ended = true;  // the last frame was shown on the previous tick
                } else {
                    activeIndex %= size;
                }
            }
        }
        if (!ended) {
            PresentFrame(hwnd, frames[static_cast<size_t>(activeIndex)]);
        }

        bool atEdge = false;
        if (IsMoving(spec)) {
            RECT rect = {};
            GetWindowRect(hwnd, &rect);
            const int newLeft = rect.left + spec.dx;
            const int newTop = rect.top + spec.dy;
            atEdge = (newLeft < 0) || (newLeft + petW > screenW) ||
                     (newTop < 0) || (newTop + petH > screenH);
            if (!atEdge) {
                SetWindowPos(hwnd, nullptr, newLeft, newTop, 0, 0,
                             SWP_NOSIZE | SWP_NOZORDER);
            }
        }

        activeTicks += 1;
        if (ended) {
            EnterBase(hwnd);
            return;
        }
        if (activeTarget > 0) {
            const bool done = (IsMoving(spec) && atEdge) ||
                              activeTicks >= activeTarget;
            // A full-cycle clip (takeoff..landing) must reach its last, grounded
            // frame before returning to idle; stopping mid-air — or on the
            // pre-takeoff frame — reads as a jump or a missing landing.
            const bool safeToStop =
                spec.playback == pet_behavior::Playback::Loop ||
                (spec.playback == pet_behavior::Playback::FullCycle &&
                 pet_walk::AtEndFrame(activeIndex, size)) ||
                (spec.playback == pet_behavior::Playback::Boomerang &&
                 pet_walk::AtCycleBoundary(activeIndex, size));
            if (done && safeToStop) {
                EnterBase(hwnd);
            }
        }
        return;
    }

    // Base: play the idle pool and, once the dwell elapses, start a behavior.
    if (basePool.empty()) {
        return;
    }
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
        // post-drag waiting: idle animations only, never wander
        waitTicks -= 1;
        if (waitTicks == 0) {
            behaviorTicks = 0;
            behaviorTarget = RandomInRange(GetSettings().idleMinTicks,
                                           GetSettings().idleMaxTicks);
        }
    } else {
        behaviorTicks += 1;
        if (behaviorTicks >= behaviorTarget) {
            const int pick = PickNextAuto();
            if (pick >= 0) {
                behaviorTicks = 0;
                StartBehavior(hwnd, pick);
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
            const int dragClip = pet_behavior::FindByTrigger(
                Behaviors(), pet_behavior::Trigger::OnDragStart);
            if (CanStart(dragClip)) {
                StartBehavior(hwnd, dragClip);
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
        if (!mouseDown && my >= petH * GetSettings().headBandTopPercent / 100 &&
            my < petH * GetSettings().headBandPercent / 100 && mx >= 0 &&
            mx < petW) {
            const int dx = mx - lastMouseX;
            const int dy = my - lastMouseY;
            if (std::abs(dx) <= GetSettings().maxSweepDeltaPx &&
                std::abs(dx) > std::abs(dy)) {
                headSweepPx += std::abs(dx);
            }
            if (headSweepPx >= GetSettings().sweepThresholdPx) {
                headSweepPx = 0;
                const int shyClip = pet_behavior::FindByTrigger(
                    Behaviors(), pet_behavior::Trigger::HeadSweep);
                if (CanStart(shyClip)) {
                    StartBehavior(hwnd, shyClip);
                }
            }
        }
        lastMouseX = mx;
        lastMouseY = my;
    }
}

void Pet::OnLButtonUp(HWND hwnd) {
    if (mouseDown && !dragging) {
        const int clickClip = pet_behavior::FindByTrigger(
            Behaviors(), pet_behavior::Trigger::OnClick);
        if (CanStart(clickClip)) {
            StartBehavior(hwnd, clickClip);
        }
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
    if (state == PetState::Acting && activeClip >= 0 &&
        clips[static_cast<size_t>(activeClip)].spec->trigger ==
            pet_behavior::Trigger::OnDragStart) {
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
    for (Clip& clip : clips) {
        FreeFrames(clip.frames);
    }
    clips.clear();
    cooldowns.clear();
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
