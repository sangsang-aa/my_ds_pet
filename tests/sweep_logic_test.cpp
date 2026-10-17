// Unit tests for sweep_logic.h — pure logic, no Windows.
// Build & run:  g++ -std=c++17 -Wall -Wextra -O2 tests/sweep_logic_test.cpp -o /tmp/sweep_test && /tmp/sweep_test
#include "../src/pet/sweep_logic.h"

#include <cassert>
#include <cstdio>

namespace {

constexpr int kThreshold = 50;
constexpr int kPasses = 2;
constexpr int kReset = 20;

void test_single_stroke_does_not_count() {
    pet_sweep::State s;
    assert(!pet_sweep::Feed(s, 60, kThreshold, kPasses));
    assert(s.passes == 0);
}

void test_one_reversal_is_one_pass_but_no_trigger() {
    pet_sweep::State s;
    assert(!pet_sweep::Feed(s, 60, kThreshold, kPasses));
    assert(!pet_sweep::Feed(s, -60, kThreshold, kPasses));
    assert(s.passes == 1);
}

void test_two_reversals_fire_and_reset() {
    pet_sweep::State s;
    assert(!pet_sweep::Feed(s, 60, kThreshold, kPasses));
    assert(!pet_sweep::Feed(s, -60, kThreshold, kPasses));
    assert(pet_sweep::Feed(s, 60, kThreshold, kPasses));
    assert(s.passes == 0);
    assert(s.accum == 0);
    assert(s.dir == 0);
}

void test_long_sweep_without_reversal_never_fires() {
    pet_sweep::State s;
    for (int i = 0; i < 5; ++i) {
        assert(!pet_sweep::Feed(s, 40, kThreshold, kPasses));
    }
    assert(s.passes == 0);
}

void test_reversal_without_enough_travel_is_not_a_pass() {
    pet_sweep::State s;
    assert(!pet_sweep::Feed(s, 30, kThreshold, kPasses));
    assert(!pet_sweep::Feed(s, -60, kThreshold, kPasses));
    assert(s.passes == 0);
}

void test_zero_delta_is_ignored() {
    pet_sweep::State s;
    assert(!pet_sweep::Feed(s, 30, kThreshold, kPasses));
    assert(!pet_sweep::Feed(s, 0, kThreshold, kPasses));
    assert(s.accum == 30);
    assert(s.dir == 1);
    assert(s.idleTicks == 0);
}

void test_idle_drops_a_stale_gesture() {
    pet_sweep::State s;
    assert(!pet_sweep::Feed(s, 60, kThreshold, kPasses));
    assert(!pet_sweep::Feed(s, -60, kThreshold, kPasses));
    assert(s.passes == 1);
    for (int i = 0; i < kReset; ++i) {
        pet_sweep::Idle(s, kReset);
    }
    assert(s.passes == 1);
    pet_sweep::Idle(s, kReset);
    assert(s.passes == 0);
}

}  // namespace

int main() {
    test_single_stroke_does_not_count();
    test_one_reversal_is_one_pass_but_no_trigger();
    test_two_reversals_fire_and_reset();
    test_long_sweep_without_reversal_never_fires();
    test_reversal_without_enough_travel_is_not_a_pass();
    test_zero_delta_is_ignored();
    test_idle_drops_a_stale_gesture();
    std::printf("sweep_logic: all tests passed\n");
    return 0;
}
