// All tunable constants for the desktop pet. Behaviour is driven entirely by
// these values; new features should add their own block here.
#ifndef PET_CONFIG_H
#define PET_CONFIG_H

#include <windows.h>

#include <cstddef>

// Window / timer
constexpr UINT_PTR kTimerId = 1;
constexpr UINT kTimerElapseMs = 100;        // ~10 fps
constexpr size_t kMaxFrames = 120;          // convention: 000.png .. 119.png
constexpr float kScaleFactor = 0.70f;       // pet renders 30% smaller on screen
constexpr wchar_t kClassName[] = L"DesktopPetWindowClass";
constexpr wchar_t kAssetsDir[] = L"desktop_pet_image";
constexpr wchar_t kDefaultAction[] = L"idle";

// Idle <-> walk behaviour
constexpr int kWalkStepPx = 5;          // window movement per tick while walking
constexpr int kIdleMinTicks = 30;       // idle dwell before walking (~3s)
constexpr int kIdleMaxTicks = 100;      // ~10s
constexpr int kWalkMinTicks = 15;       // walk duration (~1.5s)
constexpr int kWalkMaxTicks = 45;       // ~4.5s
constexpr int kWaitMinTicks = 1200;     // post-drag idle-only wait (~2 min)
constexpr int kWaitMaxTicks = 1800;     // ~3 min

// Reactions
constexpr int kHeadBandPercent = 45;    // top 45% of the pet counts as the "head"
constexpr int kSweepThresholdPx = 50;   // horizontal travel over the head -> shy
constexpr int kMaxSweepDeltaPx = 80;    // ignore jumpy deltas (mouse re-entry)
constexpr int kShyCooldownTicks = 25;   // ~2.5s between shy reactions

// Speech-bubble emoji
constexpr wchar_t kBubbleClassName[] = L"DesktopPetBubbleClass";
constexpr UINT_PTR kBubbleTimerId = 2;
constexpr UINT kBubbleTimerElapseMs = 100;
constexpr int kBubbleEmojiSizePx = 220;  // emoji is scaled to fit this box
constexpr int kBubblePadPx = 10;         // padding between emoji and bubble edge
constexpr int kBubbleRadiusPx = 10;      // rounded corner radius
constexpr int kBubbleTailW = 18;         // tail width at its base
constexpr int kBubbleTailH = 12;         // tail height (points down at the pet)
constexpr int kBubbleGapPx = 4;          // gap between tail tip and the pet
constexpr int kBubbleMinTicks = 200;     // ~20s at 100ms per tick
constexpr int kBubbleMaxTicks = 600;     // ~60s
constexpr int kBubbleShowTicks = 30;     // ~3s visible

#endif  // PET_CONFIG_H
