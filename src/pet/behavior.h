// Pure behavior-registry logic for the pet: what a behavior is and how the next
// autonomous one is chosen. Kept free of <windows.h> so tests/ can run anywhere.
#ifndef PET_BEHAVIOR_H
#define PET_BEHAVIOR_H

#include <cstddef>
#include <string>
#include <vector>

namespace pet_behavior {

enum class Trigger { Auto, OnClick, OnDragStart, OnDragEnd, HeadSweep };
enum class Playback { Loop, OneShot, FullCycle };

struct Spec {
    std::wstring action;                 // asset folder under desktop_pet_image/
    Trigger trigger = Trigger::Auto;
    Playback playback = Playback::Loop;
    int minTicks = 0;                    // Auto: dwell before firing; event: cooldown
    int maxTicks = 0;                    // <= 0 => no time limit (ended by an event)
    int weight = 1;                      // used when pickable among Auto specs
    int dx = 0;                          // window movement per tick while playing
    int dy = 0;
    bool gatedByWait = false;            // suppressed during the post-drag wait
};

// Weighted random pick among the Auto specs that are eligible (frames loaded,
// not gated) and have weight > 0. `rand01` must be uniform in [0, 1). Returns
// an index into `specs`, or -1 when nothing is selectable.
inline int PickAuto(const std::vector<Spec>& specs,
                    const std::vector<bool>& eligible, double rand01) {
    double total = 0.0;
    for (std::size_t i = 0; i < specs.size(); ++i) {
        if (i < eligible.size() && eligible[i] &&
            specs[i].trigger == Trigger::Auto && specs[i].weight > 0) {
            total += specs[i].weight;
        }
    }
    if (total <= 0.0) {
        return -1;
    }
    double pick = rand01 * total;
    for (std::size_t i = 0; i < specs.size(); ++i) {
        if (i < eligible.size() && eligible[i] &&
            specs[i].trigger == Trigger::Auto && specs[i].weight > 0) {
            pick -= specs[i].weight;
            if (pick < 0.0) {
                return static_cast<int>(i);
            }
        }
    }
    return -1;
}

// Index of the first spec carrying the given trigger, or -1.
inline int FindByTrigger(const std::vector<Spec>& specs, Trigger trigger) {
    for (std::size_t i = 0; i < specs.size(); ++i) {
        if (specs[i].trigger == trigger) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

}  // namespace pet_behavior

#endif  // PET_BEHAVIOR_H
