// Unit tests for bubble_logic.h — pure logic, no network, no Windows.
// Build & run:  g++ -std=c++17 -Wall -Wextra -O2 tests/bubble_logic_test.cpp -o /tmp/bubble_test && /tmp/bubble_test
#include "../bubble_logic.h"

#include <cassert>
#include <cstdio>

namespace {

void test_centered_above_pet() {
    const pet_bubble::Rect pet{ 100, 200, 80, 80 };
    const pet_bubble::Point at =
        pet_bubble::PlaceBubble(pet, 120, 100, 1920, 1080, 8);
    assert(at.x == 80);   // 100 + (80 - 120) / 2
    assert(at.y == 92);   // 200 - 100 - 8
}

void test_clamps_to_screen_edges() {
    const pet_bubble::Rect left{ 0, 200, 80, 80 };
    assert(pet_bubble::PlaceBubble(left, 120, 100, 1920, 1080, 8).x == 0);

    const pet_bubble::Rect right{ 1900, 200, 80, 80 };
    assert(pet_bubble::PlaceBubble(right, 120, 100, 1920, 1080, 8).x == 1800);
}

void test_flips_below_when_no_room_on_top() {
    const pet_bubble::Rect pet{ 100, 5, 80, 80 };
    const pet_bubble::Point at =
        pet_bubble::PlaceBubble(pet, 120, 100, 1920, 1080, 8);
    assert(at.y == 93);   // 5 + 80 + 8
}

void test_bottom_overlap_is_tight_on_a_short_screen() {
    // Bubble taller than the screen -> clamps to y = screenH - bubbleH.
    const pet_bubble::Rect pet{ 0, 0, 10, 10 };
    const pet_bubble::Point at =
        pet_bubble::PlaceBubble(pet, 100, 100, 200, 90, 4);
    assert(at.y == 0);    // 90 - 100 = -10 -> clamped to 0
    assert(at.x == 0);
}

void test_interval_stays_in_range() {
    for (double r = 0.0; r < 1.0; r += 0.05) {
        const int ticks = pet_bubble::NextIntervalTicks(200, 600, r);
        assert(ticks >= 200 && ticks <= 600);
    }
    assert(pet_bubble::NextIntervalTicks(200, 600, 0.0) == 200);
    assert(pet_bubble::NextIntervalTicks(200, 600, 0.999999) == 600);
    assert(pet_bubble::NextIntervalTicks(200, 600, 0.5) == 400);
}

void test_interval_degenerate_ranges() {
    assert(pet_bubble::NextIntervalTicks(300, 300, 0.5) == 300);
    assert(pet_bubble::NextIntervalTicks(300, 100, 0.5) == 300);  // inverted
    assert(pet_bubble::NextIntervalTicks(5, 5, 0.0) == 5);
}

}  // namespace

int main() {
    test_centered_above_pet();
    test_clamps_to_screen_edges();
    test_flips_below_when_no_room_on_top();
    test_bottom_overlap_is_tight_on_a_short_screen();
    test_interval_stays_in_range();
    test_interval_degenerate_ranges();
    std::printf("bubble_logic: all tests passed\n");
    return 0;
}
