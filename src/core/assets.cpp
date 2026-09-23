#include "assets.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include "config.h"
#include "gdi.h"

std::wstring FrameName(int index) {
    std::wstring name = std::to_wstring(index);
    if (name.size() == 1) {
        name = L"00" + name;
    } else if (name.size() == 2) {
        name = L"0" + name;
    }
    return name + L".png";
}

std::wstring FramePath(const std::wstring& exeDir,
                       const std::wstring& assetsDir,
                       const std::wstring& action,
                       int index) {
    return exeDir + assetsDir + L"\\" + action + L"\\" + FrameName(index);
}

std::vector<Frame> LoadFrames(const std::wstring& exeDir,
                              const std::wstring& assetsDir,
                              const std::wstring& action) {
    std::vector<Frame> frames;
    for (int i = 0; i < static_cast<int>(kMaxFrames); ++i) {
        const std::wstring path = FramePath(exeDir, assetsDir, action, i);
        gdi::Bitmap source(path.c_str());
        if (source.GetLastStatus() != gdi::Ok) {
            break;  // first missing frame -> stop loading
        }
        gdi::Bitmap* frameBitmap = &source;
        std::unique_ptr<gdi::Bitmap> scaled;
        if (kScaleFactor != 1.0f) {
            const int scaledW = std::max(
                1, static_cast<int>(std::lround(source.GetWidth() * kScaleFactor)));
            const int scaledH = std::max(
                1, static_cast<int>(std::lround(source.GetHeight() * kScaleFactor)));
            // Scale in the source's alpha mode: RGB frames stay opaque (no
            // alpha bits), RGBA frames keep per-pixel alpha.
            scaled.reset(new gdi::Bitmap(
                scaledW, scaledH,
                (source.GetPixelFormat() & PixelFormatAlpha) != 0
                    ? PixelFormat32bppARGB
                    : PixelFormat24bppRGB));
            if (scaled->GetLastStatus() != gdi::Ok) {
                break;
            }
            gdi::Graphics graphics(scaled.get());
            graphics.SetInterpolationMode(gdi::InterpolationModeHighQualityBicubic);
            graphics.DrawImage(&source, 0, 0, scaledW, scaledH);
            frameBitmap = scaled.get();
        }
        HBITMAP hbmp = nullptr;
        if (frameBitmap->GetHBITMAP(gdi::Color(0, 0, 0, 0), &hbmp) != gdi::Ok ||
            hbmp == nullptr) {
            if (hbmp != nullptr) {
                DeleteObject(hbmp);
            }
            break;
        }
        Frame frame;
        frame.hbmp = hbmp;
        frame.hasAlpha = (frameBitmap->GetPixelFormat() & PixelFormatAlpha) != 0;
        frames.push_back(frame);
    }
    return frames;
}

std::wstring EmojiName(int index) {
    std::wstring name = std::to_wstring(index);
    if (name.size() == 1) {
        name = L"0" + name;
    }
    return L"emoji_" + name + L".png";
}

std::vector<std::wstring> LoadEmojiPaths(const std::wstring& exeDir,
                                         const std::wstring& assetsDir) {
    std::vector<std::wstring> paths;
    for (int i = 1; i <= static_cast<int>(kMaxFrames); ++i) {
        const std::wstring path =
            exeDir + assetsDir + L"\\emoji\\" + EmojiName(i);
        if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
            break;
        }
        paths.push_back(path);
    }
    return paths;
}
