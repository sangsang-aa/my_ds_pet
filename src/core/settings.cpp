#include "settings.h"

#include <windows.h>

#include "core/config.h"
#include "core/inifile.h"
#include "pet/behaviors.h"

namespace {

Settings g_settings = {
    kIdleMinTicks,      kIdleMaxTicks,     kWaitMinTicks,   kWaitMaxTicks,
    kHeadBandPercent,   kHeadBandTopPercent,
    kSweepThresholdPx,  kMaxSweepDeltaPx,  kScaleFactor,
    kBubbleEmojiSizePx, kBubblePadPx,      kBubbleRadiusPx, kBubbleTailW,
    kBubbleTailH,       kBubbleGapPx,      kBubbleMinTicks, kBubbleMaxTicks,
    kBubbleShowTicks,
};

std::string AsciiFromWide(const std::wstring& text) {
    std::string out;
    out.reserve(text.size());
    for (wchar_t ch : text) {
        out.push_back(static_cast<unsigned>(ch) < 128 ? static_cast<char>(ch)
                                                      : '?');
    }
    return out;
}

}  // namespace

const Settings& GetSettings() {
    return g_settings;
}

Settings& MutableSettings() {
    return g_settings;
}

void ApplySettingsText(const std::string& iniText) {
    const pet_ini::File ini = pet_ini::Parse(iniText);
    Settings& settings = g_settings;

    const auto set_int = [&ini](const char* section, const char* key,
                                int& target) {
        if (const std::string* value = pet_ini::Find(ini, section, key)) {
            target = pet_ini::IntOr(*value, target);
        }
    };

    set_int("pet", "idle_min_ticks", settings.idleMinTicks);
    set_int("pet", "idle_max_ticks", settings.idleMaxTicks);
    set_int("pet", "wait_min_ticks", settings.waitMinTicks);
    set_int("pet", "wait_max_ticks", settings.waitMaxTicks);
    set_int("pet", "head_band_percent", settings.headBandPercent);
    set_int("pet", "head_band_top_percent", settings.headBandTopPercent);
    set_int("pet", "sweep_threshold_px", settings.sweepThresholdPx);
    set_int("pet", "max_sweep_delta_px", settings.maxSweepDeltaPx);
    if (const std::string* value = pet_ini::Find(ini, "pet", "scale_factor")) {
        settings.scaleFactor = pet_ini::DoubleOr(*value, settings.scaleFactor);
    }

    set_int("bubble", "emoji_size_px", settings.bubbleEmojiSizePx);
    set_int("bubble", "pad_px", settings.bubblePadPx);
    set_int("bubble", "radius_px", settings.bubbleRadiusPx);
    set_int("bubble", "tail_w", settings.bubbleTailW);
    set_int("bubble", "tail_h", settings.bubbleTailH);
    set_int("bubble", "gap_px", settings.bubbleGapPx);
    set_int("bubble", "min_ticks", settings.bubbleMinTicks);
    set_int("bubble", "max_ticks", settings.bubbleMaxTicks);
    set_int("bubble", "show_ticks", settings.bubbleShowTicks);

    for (pet_behavior::Spec& spec : MutableBehaviors()) {
        const std::string name = AsciiFromWide(spec.action);
        if (const std::string* value = pet_ini::Find(ini, name, "min_ticks")) {
            spec.minTicks = pet_ini::IntOr(*value, spec.minTicks);
        }
        if (const std::string* value = pet_ini::Find(ini, name, "max_ticks")) {
            spec.maxTicks = pet_ini::IntOr(*value, spec.maxTicks);
        }
        if (const std::string* value = pet_ini::Find(ini, name, "weight")) {
            spec.weight = pet_ini::IntOr(*value, spec.weight);
        }
        if (const std::string* value = pet_ini::Find(ini, name, "dx")) {
            spec.dx = pet_ini::IntOr(*value, spec.dx);
        }
        if (const std::string* value = pet_ini::Find(ini, name, "dy")) {
            spec.dy = pet_ini::IntOr(*value, spec.dy);
        }
    }
}

bool ApplySettingsFile(const std::wstring& path) {
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                                    nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    std::string text;
    char buffer[4096];
    DWORD read = 0;
    while (ReadFile(file, buffer, sizeof(buffer), &read, nullptr) != 0 &&
           read > 0) {
        text.append(buffer, read);
    }
    CloseHandle(file);
    ApplySettingsText(text);
    return true;
}
