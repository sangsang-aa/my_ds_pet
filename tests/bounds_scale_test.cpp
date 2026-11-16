// Unit tests for ScaleSubjectBounds in bounds.h — pure logic, no Windows.
// Build & run:  g++ -std=c++17 -Wall -Wextra -O2 tests/bounds_scale_test.cpp -o build/bounds_scale_test.exe
#include "../src/core/bounds.h"

#include <cassert>
#include <cstdio>

namespace {

void test_idle_source_box_scales_to_window() {
    // desktop_pet_image/idle/bounds.txt first row, 384x512 source -> 269x358.
    const SubjectBounds b =
        ScaleSubjectBounds(61, 110, 262, 353, 0.70, 269, 358);
    assert(b.valid);
    assert(b.x == 43);
    assert(b.y == 77);
    assert(b.w == 183);
    assert(b.h == 247);
}

void test_scale_one_is_identity() {
    const SubjectBounds b = ScaleSubjectBounds(10, 20, 30, 40, 1.0, 100, 100);
    assert(b.valid);
    assert(b.x == 10);
    assert(b.y == 20);
    assert(b.w == 30);
    assert(b.h == 40);
}

void test_negative_origin_is_clipped() {
    const SubjectBounds b =
        ScaleSubjectBounds(-10, -5, 20, 20, 1.0, 100, 100);
    assert(b.valid);
    assert(b.x == 0);
    assert(b.y == 0);
    assert(b.w == 10);
    assert(b.h == 15);
}

void test_oversized_box_is_clipped_to_frame() {
    const SubjectBounds b =
        ScaleSubjectBounds(0, 0, 200, 200, 1.0, 100, 100);
    assert(b.valid);
    assert(b.x == 0);
    assert(b.y == 0);
    assert(b.w == 100);
    assert(b.h == 100);
}

void test_box_entirely_off_frame_is_invalid() {
    const SubjectBounds b =
        ScaleSubjectBounds(150, 0, 10, 10, 1.0, 100, 100);
    assert(!b.valid);
}

void test_empty_and_invalid_inputs() {
    assert(!ScaleSubjectBounds(0, 0, 0, 10, 1.0, 100, 100).valid);
    assert(!ScaleSubjectBounds(0, 0, 10, 0, 1.0, 100, 100).valid);
    assert(!ScaleSubjectBounds(0, 0, 10, 10, 0.0, 100, 100).valid);
    assert(!ScaleSubjectBounds(0, 0, 10, 10, 1.0, 0, 100).valid);
    assert(!ScaleSubjectBounds(0, 0, 10, 10, 1.0, 100, 0).valid);
}

void test_half_scale_rounds() {
    const SubjectBounds b = ScaleSubjectBounds(10, 20, 30, 40, 0.5, 100, 100);
    assert(b.valid);
    assert(b.x == 5);
    assert(b.y == 10);
    assert(b.w == 15);
    assert(b.h == 20);
}

}  // namespace

int main() {
    test_idle_source_box_scales_to_window();
    test_scale_one_is_identity();
    test_negative_origin_is_clipped();
    test_oversized_box_is_clipped_to_frame();
    test_box_entirely_off_frame_is_invalid();
    test_empty_and_invalid_inputs();
    test_half_scale_rounds();
    std::printf("bounds_scale: all tests passed\n");
    return 0;
}
