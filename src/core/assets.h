// Frame loading: PNG frame sequences and emoji stickers.
#ifndef PET_ASSETS_H
#define PET_ASSETS_H

#include <windows.h>

#include <string>
#include <vector>

#include "alpha_mask.h"

struct Frame {
    HBITMAP hbmp = nullptr;
    bool hasAlpha = false;  // source PNG had an alpha channel?
    // Subject box from bounds.txt, in SOURCE canvas coordinates (0,0,0,0 =
    // unknown). Pet::PresentFrame scales it to the displayed window.
    int bx = 0;
    int by = 0;
    int bw = 0;
    int bh = 0;
    // 1 bit per pixel: alpha >= kHitAlpha. Empty means "treat as fully opaque",
    // so frames without an alpha channel keep the whole rectangle clickable.
    AlphaMask mask;
};

std::wstring FrameName(int index);
std::wstring FramePath(const std::wstring& exeDir,
                       const std::wstring& assetsDir,
                       const std::wstring& action,
                       int index);

// Load consecutive frames starting at 000.png; stop at the first missing file.
std::vector<Frame> LoadFrames(const std::wstring& exeDir,
                              const std::wstring& assetsDir,
                              const std::wstring& action);

std::wstring EmojiName(int index);

// emoji/emoji_01.png .. emoji_NN.png (stop at the first missing file).
std::vector<std::wstring> LoadEmojiPaths(const std::wstring& exeDir,
                                         const std::wstring& assetsDir);

#endif  // PET_ASSETS_H
