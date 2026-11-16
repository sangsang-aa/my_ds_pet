// Pure 1-bit coverage mask (no Windows headers, unit-tested).
//
// The pet window is a full 269x358 rectangle but the sprite inside it is not:
// a 384x512 frame at kScaleFactor 0.70 has large transparent margins. Without
// this mask the window swallows clicks on those margins — the user right-clicks
// "empty desktop" and the pet quits.
#ifndef PET_ALPHA_MASK_H
#define PET_ALPHA_MASK_H

#include <cstddef>
#include <cstdint>
#include <vector>

struct AlphaMask {
    int width = 0;
    int height = 0;
    std::vector<uint32_t> bits;  // 1 bit per pixel, row-major

    bool Empty() const { return bits.empty(); }

    void Reset(int w, int h) {
        width = w > 0 ? w : 0;
        height = h > 0 ? h : 0;
        const size_t count = static_cast<size_t>(width) * height;
        bits.assign((count + 31) / 32, 0u);
    }

    void Set(int x, int y) {
        if (x < 0 || y < 0 || x >= width || y >= height) {
            return;
        }
        const size_t index = static_cast<size_t>(y) * width + x;
        bits[index >> 5] |= (1u << (index & 31));
    }

    // Out-of-range points are not covered.
    bool Test(int x, int y) const {
        if (x < 0 || y < 0 || x >= width || y >= height) {
            return false;
        }
        const size_t index = static_cast<size_t>(y) * width + x;
        return ((bits[index >> 5] >> (index & 31)) & 1u) != 0;
    }
};

#endif  // PET_ALPHA_MASK_H
