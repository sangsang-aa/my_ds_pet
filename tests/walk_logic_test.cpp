// Unit tests for walk_logic.h — pure logic, no Windows.
// Build & run:  g++ -std=c++17 -Wall -Wextra -O2 tests/walk_logic_test.cpp -o /tmp/walk_test && /tmp/walk_test
#include "../walk_logic.h"

#include <cassert>
#include <cstdio>

namespace {

void test_boundaries_are_safe() {
    assert(pet_walk::AtCycleBoundary(0, 30));
    assert(pet_walk::AtCycleBoundary(29, 30));
}

void test_middle_frames_are_not_safe() {
    assert(!pet_walk::AtCycleBoundary(1, 30));
    assert(!pet_walk::AtCycleBoundary(15, 30));
    assert(!pet_walk::AtCycleBoundary(28, 30));
}

void test_degenerate_sizes() {
    assert(pet_walk::AtCycleBoundary(0, 2));
    assert(pet_walk::AtCycleBoundary(1, 2));
    assert(pet_walk::AtCycleBoundary(0, 1));
    assert(pet_walk::AtCycleBoundary(0, 0));
}

}  // namespace

int main() {
    test_boundaries_are_safe();
    test_middle_frames_are_not_safe();
    test_degenerate_sizes();
    std::printf("walk_logic: all tests passed\n");
    return 0;
}
