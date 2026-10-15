// Unit tests for drag_logic.h — pure geometry, no Windows.
// Build & run:  g++ -std=c++17 -Wall -Wextra -O2 tests/drag_logic_test.cpp -o /tmp/drag_test && /tmp/drag_test
#include "../src/pet/drag_logic.h"

#include <cassert>
#include <cstdio>

namespace {

void test_in_bounds_position_is_untouched() {
    int x = 100;
    int y = 60;
    pet_drag::ClampToScreen(x, y, 200, 300, 1000, 800);
    assert(x == 100);
    assert(y == 60);
}

void test_negative_overshoot_snaps_to_zero() {
    int x = -40;
    int y = -7;
    pet_drag::ClampToScreen(x, y, 200, 300, 1000, 800);
    assert(x == 0);
    assert(y == 0);
}

void test_past_right_and_bottom_snaps_flush_to_the_edge() {
    int x = 5000;
    int y = 5000;
    pet_drag::ClampToScreen(x, y, 200, 300, 1000, 800);
    assert(x == 800);
    assert(y == 500);
}

void test_exact_edge_contact_is_allowed() {
    int x = 800;
    int y = 500;
    pet_drag::ClampToScreen(x, y, 200, 300, 1000, 800);
    assert(x == 800);
    assert(y == 500);
}

void test_window_larger_than_screen_never_goes_negative() {
    int x = 50;
    int y = 50;
    pet_drag::ClampToScreen(x, y, 1200, 900, 1000, 800);
    assert(x == 0);
    assert(y == 0);
}

}  // namespace

int main() {
    test_in_bounds_position_is_untouched();
    test_negative_overshoot_snaps_to_zero();
    test_past_right_and_bottom_snaps_flush_to_the_edge();
    test_exact_edge_contact_is_allowed();
    test_window_larger_than_screen_never_goes_negative();
    std::printf("drag_logic: all tests passed\n");
    return 0;
}
