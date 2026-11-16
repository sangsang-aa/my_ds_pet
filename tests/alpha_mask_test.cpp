// Unit tests for alpha_mask.h — pure logic, no Windows.
// Build & run:  g++ -std=c++17 -Wall -Wextra -O2 tests/alpha_mask_test.cpp -o build/alpha_mask_test.exe
#include "../src/core/alpha_mask.h"

#include <cassert>
#include <cstdio>

namespace {

void test_empty_mask_is_empty() {
    AlphaMask mask;
    assert(mask.Empty());
    assert(!mask.Test(0, 0));
}

void test_set_and_test() {
    AlphaMask mask;
    mask.Reset(8, 4);
    assert(!mask.Empty());
    mask.Set(0, 0);
    mask.Set(7, 3);
    mask.Set(3, 2);
    assert(mask.Test(0, 0));
    assert(mask.Test(7, 3));
    assert(mask.Test(3, 2));
    assert(!mask.Test(1, 0));
    assert(!mask.Test(7, 2));
}

void test_out_of_range_is_not_covered() {
    AlphaMask mask;
    mask.Reset(8, 4);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 8; ++x) {
            mask.Set(x, y);
        }
    }
    assert(mask.Test(0, 0));
    assert(mask.Test(7, 3));
    assert(!mask.Test(-1, 0));
    assert(!mask.Test(0, -1));
    assert(!mask.Test(8, 0));
    assert(!mask.Test(0, 4));
}

void test_reset_clears_previous_contents() {
    AlphaMask mask;
    mask.Reset(8, 4);
    mask.Set(2, 2);
    assert(mask.Test(2, 2));
    mask.Reset(8, 4);
    assert(!mask.Test(2, 2));
    assert(!mask.Empty());
}

void test_bit_packing_crosses_word_boundaries() {
    // 33 pixels per row exercises the last partial word and the row boundary.
    AlphaMask mask;
    mask.Reset(33, 3);
    mask.Set(31, 0);
    mask.Set(32, 0);
    mask.Set(0, 1);
    mask.Set(32, 2);
    assert(mask.Test(31, 0));
    assert(mask.Test(32, 0));
    assert(mask.Test(0, 1));
    assert(mask.Test(32, 2));
    assert(!mask.Test(32, 1));
    assert(!mask.Test(31, 1));
    assert(!mask.Test(0, 2));
}

void test_zero_size_mask_is_safe() {
    AlphaMask mask;
    mask.Reset(0, 0);
    assert(mask.Empty());
    mask.Set(0, 0);
    assert(!mask.Test(0, 0));
}

}  // namespace

int main() {
    test_empty_mask_is_empty();
    test_set_and_test();
    test_out_of_range_is_not_covered();
    test_reset_clears_previous_contents();
    test_bit_packing_crosses_word_boundaries();
    test_zero_size_mask_is_safe();
    std::printf("alpha_mask: all tests passed\n");
    return 0;
}
