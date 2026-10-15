// Unit tests for bubble_logic.h — pure logic, no network, no Windows.
// Build & run:  g++ -std=c++17 -Wall -Wextra -O2 tests/bubble_logic_test.cpp -o /tmp/bubble_test && /tmp/bubble_test
#include "../src/bubble/bubble_logic.h"

#include <cassert>
#include <cstdio>

namespace {

void test_centered_above_pet() {
    const pet_bubble::Rect pet{ 100, 200, 80, 80 };
    const pet_bubble::Placement p =
        pet_bubble::PlaceBubble(pet, 120, 100, 12, 1920, 1080, 8, 45);
    assert(p.side == pet_bubble::Side::Above);
    assert(p.at.x == 80);
    assert(p.at.y == 80);
}

void test_clamps_to_screen_edges() {
    const pet_bubble::Rect left{ 0, 200, 80, 80 };
    assert(pet_bubble::PlaceBubble(left, 120, 100, 12, 1920, 1080, 8, 45)
               .at.x == 0);

    const pet_bubble::Rect right{ 1900, 200, 80, 80 };
    assert(pet_bubble::PlaceBubble(right, 120, 100, 12, 1920, 1080, 8, 45)
               .at.x == 1788);
}

void test_goes_right_when_no_room_on_top() {
    const pet_bubble::Rect pet{ 100, 5, 80, 80 };
    const pet_bubble::Placement p =
        pet_bubble::PlaceBubble(pet, 120, 100, 12, 1920, 1080, 8, 45);
    assert(p.side == pet_bubble::Side::Right);
    assert(p.at.x == pet.x + pet.w + 8);
    assert(p.at.y == 0);  // besideY clamps to 0 with the pet near the top
}

void test_goes_left_when_right_side_does_not_fit() {
    const pet_bubble::Rect pet{ 1900, 5, 80, 80 };
    const pet_bubble::Placement p =
        pet_bubble::PlaceBubble(pet, 120, 100, 12, 1920, 1080, 8, 45);
    assert(p.side == pet_bubble::Side::Left);
    assert(p.at.x == pet.x - 132 - 8);
    assert(p.at.y == 0);
}

void test_side_clamps_y_on_a_short_screen() {
    const pet_bubble::Rect pet{ 0, 0, 10, 10 };
    const pet_bubble::Placement p =
        pet_bubble::PlaceBubble(pet, 100, 100, 12, 200, 90, 4, 45);
    assert(p.side == pet_bubble::Side::Right);
    assert(p.at.y == 0);
    assert(p.at.x == 14);
}

void test_below_is_last_resort_on_tiny_screen() {
    const pet_bubble::Rect pet{ 0, 0, 10, 10 };
    const pet_bubble::Placement p =
        pet_bubble::PlaceBubble(pet, 100, 100, 12, 100, 90, 4, 45);
    assert(p.side == pet_bubble::Side::Below);
    assert(p.at.x == 0);
    assert(p.at.y == 0);
}

void test_head_percent_centres_body_on_head_band() {
    const pet_bubble::Rect pet{ 100, 20, 80, 80 };
    const int bodyH = 100;
    const pet_bubble::Placement p =
        pet_bubble::PlaceBubble(pet, 120, bodyH, 12, 1920, 1080, 8, 100);
    assert(p.side == pet_bubble::Side::Right);
    assert(p.at.y == pet.y + (pet.h - bodyH) / 2);
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
    assert(pet_bubble::NextIntervalTicks(300, 100, 0.5) == 300);
    assert(pet_bubble::NextIntervalTicks(5, 5, 0.0) == 5);
}

}  // namespace

int main() {
    test_centered_above_pet();
    test_clamps_to_screen_edges();
    test_goes_right_when_no_room_on_top();
    test_goes_left_when_right_side_does_not_fit();
    test_side_clamps_y_on_a_short_screen();
    test_below_is_last_resort_on_tiny_screen();
    test_head_percent_centres_body_on_head_band();
    test_interval_stays_in_range();
    test_interval_degenerate_ranges();
    std::printf("bubble_logic: all tests passed\n");
    return 0;
}
