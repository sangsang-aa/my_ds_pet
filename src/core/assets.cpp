#include "assets.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include <windows.h>

#include "config.h"
#include "gdi.h"
#include "settings.h"

namespace {

// Read a UTF-8 text file through the wide path, or return "" when missing.
std::string ReadTextFile(const std::wstring& path) {
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                                    nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return std::string();
    }
    std::string text;
    char buffer[4096];
    DWORD read = 0;
    while (ReadFile(file, buffer, sizeof(buffer), &read, nullptr) != 0 &&
           read > 0) {
        text.append(buffer, read);
    }
    CloseHandle(file);
    return text;
}

// `bounds.txt` holds one `x y w h` subject box per frame, written by
// MCPS/prepare_action.py; without it every frame counts as fully opaque.
void LoadFrameBounds(const std::wstring& path, std::vector<Frame>& frames) {
    const std::string text = ReadTextFile(path);
    if (text.empty()) {
        return;
    }
    size_t index = 0;
    size_t pos = 0;
    while (pos < text.size() && index < frames.size()) {
        const size_t newline = text.find('\n', pos);
        const std::string line = text.substr(
            pos, newline == std::string::npos ? std::string::npos : newline - pos);
        pos = (newline == std::string::npos) ? text.size() : newline + 1;
        int bx = 0;
        int by = 0;
        int bw = 0;
        int bh = 0;
        if (std::sscanf(line.c_str(), "%d %d %d %d", &bx, &by, &bw, &bh) == 4 &&
            bw > 0 && bh > 0) {
            frames[index].bx = bx;
            frames[index].by = by;
            frames[index].bw = bw;
            frames[index].bh = bh;
            index += 1;
        }
    }
}

}  // namespace

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
        if (GetSettings().scaleFactor != 1.0f) {
            const int scaledW = std::max(
                1, static_cast<int>(std::lround(source.GetWidth() * GetSettings().scaleFactor)));
            const int scaledH = std::max(
                1, static_cast<int>(std::lround(source.GetHeight() * GetSettings().scaleFactor)));
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
    LoadFrameBounds(exeDir + assetsDir + L"\\" + action + L"\\bounds.txt", frames);
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
