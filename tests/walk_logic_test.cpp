// Unit tests for walk_logic.h — pure logic, no Windows.
// Build & run:  g++ -std=c++17 -Wall -Wextra -O2 tests/walk_logic_test.cpp -o /tmp/walk_test && /tmp/walk_test
#include "../walk_logic.h"

#include <cassert>
#include <cstdio>

namespace {

void test_last_frame_is_the_end() {
    assert(pet_walk::AtEndFrame(29, 30));
    assert(pet_walk::AtEndFrame(1, 2));
    assert(pet_walk::AtEndFrame(0, 1));
}

void test_other_frames_are_not_the_end() {
    assert(!pet_walk::AtEndFrame(0, 30));
    assert(!pet_walk::AtEndFrame(15, 30));
    assert(!pet_walk::AtEndFrame(28, 30));
    assert(!pet_walk::AtEndFrame(0, 2));
}

void test_empty_clip_is_always_at_end() {
    assert(pet_walk::AtEndFrame(0, 0));
}

}  // namespace

int main() {
    test_last_frame_is_the_end();
    test_other_frames_are_not_the_end();
    test_empty_clip_is_always_at_end();
    std::printf("walk_logic: all tests passed\n");
    return 0;
}
