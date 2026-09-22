#include "util.h"

#include <windows.h>

namespace {

const wchar_t kFirstFrame[] = L"000.png";

}  // namespace

std::wstring GetExecutableDir() {
    wchar_t buffer[MAX_PATH];
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return std::wstring();
    }
    std::wstring dir(buffer, length);
    const size_t slash = dir.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        dir.resize(slash + 1);
    }
    return dir;
}

void ShowFatalError(const std::wstring& message) {
    MessageBoxW(nullptr, message.c_str(), L"Desktop Pet",
                MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
}

std::wstring AvailableActions(const std::wstring& assetsPath) {
    std::wstring result;
    WIN32_FIND_DATAW entry = {};
    const std::wstring pattern = assetsPath + L"\\*";
    HANDLE handle = FindFirstFileW(pattern.c_str(), &entry);
    if (handle == INVALID_HANDLE_VALUE) {
        return L"(assets folder not found)";
    }
    do {
        if ((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
            entry.cFileName[0] != L'.') {
            const std::wstring first =
                assetsPath + L"\\" + entry.cFileName + L"\\" + kFirstFrame;
            if (GetFileAttributesW(first.c_str()) != INVALID_FILE_ATTRIBUTES) {
                if (!result.empty()) {
                    result += L", ";
                }
                result += entry.cFileName;
            }
        }
    } while (FindNextFileW(handle, &entry) != 0);
    FindClose(handle);
    return result.empty() ? L"(none)" : result;
}

std::wstring WideFromUtf8(const char* utf8) {
    if (utf8 == nullptr || *utf8 == '\0') {
        return std::wstring();
    }
    const int needed = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    if (needed <= 1) {
        return std::wstring();
    }
    std::wstring result(static_cast<size_t>(needed) - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &result[0], needed);
    return result;
}

std::mt19937& Rng() {
    static std::mt19937 rng(std::random_device{}());
    return rng;
}

int RandomInRange(int lo, int hi) {
    if (hi <= lo) {
        return lo;
    }
    std::uniform_int_distribution<int> dist(lo, hi);
    return dist(Rng());
}
