#pragma once
#include <cwchar>

namespace pw {
struct LaunchResolution {
    int resolution = 0;
    int upscale = 0;
    int slot = -1;
    bool valid = false;
};

// Audited build 25052315 uses max(resolution, upscale), not desktop height.
// Accept only unambiguous canonical arguments. The game accepts some other
// spellings by comparing their first character; those are intentionally refused.
inline LaunchResolution launch_resolution(int argc, const wchar_t* const* argv) {
    LaunchResolution mode;
    if (!argv || argc < 1 || argc > 32) return mode;
    bool seen_resolution = false, seen_upscale = false;
    for (int i = 1; i < argc; ++i) {
        if (!argv[i]) return mode;
        const bool resolution = std::wcscmp(argv[i], L"-resolution") == 0;
        const bool upscale = std::wcscmp(argv[i], L"-upscale") == 0;
        if (!resolution && !upscale) continue;
        bool& seen = resolution ? seen_resolution : seen_upscale;
        if (seen || i + 1 >= argc || !argv[i + 1]) return mode;
        seen = true;
        const wchar_t* value = argv[i + 1];
        const wchar_t maximum = resolution ? L'1' : L'3';
        if (value[0] < L'0' || value[0] > maximum || value[1] != L'\0') return mode;
        (resolution ? mode.resolution : mode.upscale) = value[0] - L'0';
    }
    mode.slot = mode.resolution > mode.upscale ? mode.resolution : mode.upscale;
    mode.valid = true;
    return mode;
}
} // namespace pw
