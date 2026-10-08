// Runtime-tunable settings. Defaults come from core/config.h; an optional
// desktop_pet_image/pet.ini overrides them (and per-behavior parameters) at
// startup, so behaviour can be tuned without recompiling.
#ifndef PET_SETTINGS_H
#define PET_SETTINGS_H

#include <string>

struct Settings {
    int idleMinTicks;
    int idleMaxTicks;
    int waitMinTicks;
    int waitMaxTicks;
    int headBandPercent;
    int sweepThresholdPx;
    int maxSweepDeltaPx;
    double scaleFactor;

    int bubbleEmojiSizePx;
    int bubblePadPx;
    int bubbleRadiusPx;
    int bubbleTailW;
    int bubbleTailH;
    int bubbleGapPx;
    int bubbleMinTicks;
    int bubbleMaxTicks;
    int bubbleShowTicks;
};

const Settings& GetSettings();
Settings& MutableSettings();

// Apply INI text: [pet] / [bubble] override the globals, and a section named
// after a behavior action (e.g. [walking_up]) overrides that behavior's
// min_ticks / max_ticks / weight / dx / dy.
void ApplySettingsText(const std::string& iniText);

// Read `path` if it exists and apply it; returns true when a file was read.
bool ApplySettingsFile(const std::wstring& path);

#endif  // PET_SETTINGS_H
