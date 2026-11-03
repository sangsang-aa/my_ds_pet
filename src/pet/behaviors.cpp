#include "behaviors.h"

#include "core/config.h"

std::vector<pet_behavior::Spec>& MutableBehaviors() {
    using pet_behavior::Playback;
    using pet_behavior::Spec;
    using pet_behavior::Trigger;
    static std::vector<Spec> specs = {
        { L"walking_left",  Trigger::Auto, Playback::Loop,      kWalkMinTicks, kWalkMaxTicks, 1, -kWalkStepPx, 0, true },
        { L"walking_right", Trigger::Auto, Playback::Loop,      kWalkMinTicks, kWalkMaxTicks, 1,  kWalkStepPx, 0, true },
        { L"walking_up",    Trigger::Auto, Playback::Boomerang, kWalkUpMinTicks, kWalkUpMaxTicks, 1, 0, -kWalkStepPx, true },
        { L"walking_down",  Trigger::Auto, Playback::FullCycle, kWalkMinTicks, kWalkMaxTicks, 1, 0,  kWalkStepPx, true },
        { L"clicked", Trigger::OnClick,     Playback::OneShot, 0, 0, 1, 0, 0, false },
        { L"drag",    Trigger::OnDragStart, Playback::Loop,    0, 0, 1, 0, 0, false },
        { L"shy",     Trigger::HeadSweep,   Playback::OneShot,
          kShyCooldownTicks, kShyCooldownTicks, 1, 0, 0, false },
    };
    return specs;
}

const std::vector<pet_behavior::Spec>& Behaviors() {
    return MutableBehaviors();
}
