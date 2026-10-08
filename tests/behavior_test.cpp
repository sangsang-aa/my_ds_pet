// Unit tests for behavior.h — pure logic, no Windows.
// Build & run:  g++ -std=c++17 -Wall -Wextra -O2 tests/behavior_test.cpp -o /tmp/behavior_test && /tmp/behavior_test
#include "../src/pet/behavior.h"

#include <cassert>
#include <cstdio>
#include <vector>

namespace {

pet_behavior::Spec auto_spec(int weight) {
    pet_behavior::Spec s;
    s.trigger = pet_behavior::Trigger::Auto;
    s.weight = weight;
    return s;
}

void test_picks_only_eligible() {
    const std::vector<pet_behavior::Spec> specs = { auto_spec(1), auto_spec(1), auto_spec(1) };
    const std::vector<bool> eligible = { true, false, true };
    for (double r = 0.0; r < 1.0; r += 0.05) {
        const int i = pet_behavior::PickAuto(specs, eligible, r);
        assert(i == 0 || i == 2);
    }
}

void test_zero_weight_is_never_picked() {
    const std::vector<pet_behavior::Spec> specs = { auto_spec(0), auto_spec(5) };
    const std::vector<bool> eligible = { true, true };
    for (double r = 0.0; r < 1.0; r += 0.05) {
        assert(pet_behavior::PickAuto(specs, eligible, r) == 1);
    }
}

void test_weighting_is_proportional() {
    const std::vector<pet_behavior::Spec> specs = { auto_spec(1), auto_spec(3) };
    const std::vector<bool> eligible = { true, true };
    int first = 0;
    int second = 0;
    for (int k = 0; k < 1000; ++k) {
        const double r = (k + 0.5) / 1000.0;
        if (pet_behavior::PickAuto(specs, eligible, r) == 0) {
            ++first;
        } else {
            ++second;
        }
    }
    assert(first == 250);
    assert(second == 750);
}

void test_nothing_selectable_returns_minus_one() {
    const std::vector<pet_behavior::Spec> none_eligible = { auto_spec(1) };
    assert(pet_behavior::PickAuto(none_eligible, { false }, 0.5) == -1);

    const std::vector<pet_behavior::Spec> zero_weight = { auto_spec(0) };
    assert(pet_behavior::PickAuto(zero_weight, { true }, 0.5) == -1);

    pet_behavior::Spec event;
    event.trigger = pet_behavior::Trigger::OnClick;
    const std::vector<pet_behavior::Spec> only_event = { event };
    assert(pet_behavior::PickAuto(only_event, { true }, 0.5) == -1);
}

void test_find_by_trigger() {
    pet_behavior::Spec click;
    click.trigger = pet_behavior::Trigger::OnClick;
    pet_behavior::Spec sweep;
    sweep.trigger = pet_behavior::Trigger::HeadSweep;
    const std::vector<pet_behavior::Spec> specs = { click, sweep };
    assert(pet_behavior::FindByTrigger(specs, pet_behavior::Trigger::OnClick) == 0);
    assert(pet_behavior::FindByTrigger(specs, pet_behavior::Trigger::HeadSweep) == 1);
    assert(pet_behavior::FindByTrigger(specs, pet_behavior::Trigger::OnDragStart) == -1);
}

}  // namespace

int main() {
    test_picks_only_eligible();
    test_zero_weight_is_never_picked();
    test_weighting_is_proportional();
    test_nothing_selectable_returns_minus_one();
    test_find_by_trigger();
    std::printf("behavior: all tests passed\n");
    return 0;
}
