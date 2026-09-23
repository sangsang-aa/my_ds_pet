// Small shared helpers: paths, error reporting, UTF-8 conversion, RNG.
#ifndef PET_UTIL_H
#define PET_UTIL_H

#include <random>
#include <string>

// Directory (with trailing backslash) of the running executable.
std::wstring GetExecutableDir();

// Fatal startup error. GUI apps have no console, so this must be shown here to
// avoid the "double-click does nothing" silent exit.
void ShowFatalError(const std::wstring& message);

// Comma-separated list of action folders under assetsPath that contain 000.png.
std::wstring AvailableActions(const std::wstring& assetsPath);

// Narrow UTF-8 string -> wide string, or empty on failure.
std::wstring WideFromUtf8(const char* utf8);

// Shared random engine and an inclusive [lo, hi] helper.
std::mt19937& Rng();
int RandomInRange(int lo, int hi);

#endif  // PET_UTIL_H
