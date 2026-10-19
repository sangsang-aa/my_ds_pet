// Unit tests for tray_logic.h — pure logic, no Windows.
// Build & run:  g++ -std=c++17 -Wall -Wextra -O2 tests/tray_logic_test.cpp -o /tmp/tray_logic_test && /tmp/tray_logic_test
#include "../src/tray/tray_logic.h"

#include <cassert>
#include <cstdio>

namespace {

void test_clamp_below_range_snaps_to_lo() {
    assert(tray_logic::ClampScalePercent(-1, 30, 150) == 30);
    assert(tray_logic::ClampScalePercent(29, 30, 150) == 30);
}

void test_clamp_inside_range_is_untouched() {
    assert(tray_logic::ClampScalePercent(70, 30, 150) == 70);
}

void test_clamp_above_range_snaps_to_hi() {
    assert(tray_logic::ClampScalePercent(151, 30, 150) == 150);
    assert(tray_logic::ClampScalePercent(999, 30, 150) == 150);
}

void test_scale_factor_from_percent() {
    assert(tray_logic::ScaleFactorFromPercent(70) == 0.7);
    assert(tray_logic::ScaleFactorFromPercent(100) == 1.0);
    assert(tray_logic::ScaleFactorFromPercent(30) == 0.3);
}

void test_command_ids_are_distinct() {
    assert(tray_logic::kCommandResize != tray_logic::kCommandQuit);
}

}  // namespace

int main() {
    test_clamp_below_range_snaps_to_lo();
    test_clamp_inside_range_is_untouched();
    test_clamp_above_range_snaps_to_hi();
    test_scale_factor_from_percent();
    test_command_ids_are_distinct();
    std::printf("tray_logic: all tests passed\n");
    return 0;
}
