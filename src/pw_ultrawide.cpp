// Peace Walker UltraWide Fix.
//
// The game renders the world at 1920x1088 (four times the PSP resolution) into
// a separate target, then composites it to the output inside a 16:9 frame with
// black bars. The fix has three independent, individually configurable parts:
//
//   1. A resolution table in .rdata defines the composited size. Replacing one
//      entry with the real output width removes the bars.
//   2. The projection matrix is static global data, and the port never adjusts
//      its aspect ratio. Expanding the output without changing it merely
//      stretches the image. The correction happens INSIDE the function that
//      builds the matrix, rather than from a thread, because the game rebuilds
//      it once per frame and racing that writer causes flicker.
//   3. The 2D interface is drawn into the same internal target as the world. To
//      keep it at 16:9 while the world fills the display, its viewport changes
//      from the call where 2D rendering begins. That call is selected per frame
//      according to whether a world image must be composited.
//
// All addresses were measured in the Master Collection Vol.1 executable, and
// the PE header is validated before any memory is changed.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>

#include "pw_hud_island.h"
#include "pw_projection_island.h"

#ifndef PWUWFIX_VERSION
#define PWUWFIX_VERSION "development"
#endif

namespace {

// Supported executable: METAL GEAR SOLID PEACE WALKER.exe from Master Collection
// Vol.1 (Steam AppID 2492660), packed or unpacked; its PE header is unchanged.
constexpr DWORD kTimeDateStamp = 0x6a6cc50e;
constexpr DWORD kSizeOfImage   = 0x0199e000;

// Resolution table. Four 8-byte 16:9 entries: (width, height).
//   [0] 1280x720   [1] 1920x1080   [2] 2560x1440   [3] 3840x2160
constexpr std::uintptr_t kResolutionTableRva = 0xd8f1b8;
constexpr int kResolutionSlots = 4;

// Projection matrix, 4x4 floats. PSP perspective signature:
// m11 = -1.0, m15 = 0.
constexpr std::uintptr_t kProjectionRva = 0x10f4330;
constexpr std::uintptr_t kProjM0 = 0x00, kProjM5 = 0x14;
constexpr std::uintptr_t kProjM11 = 0x2c, kProjM15 = 0x3c;

// Epilogue of the matrix builder and the instruction that writes the matrix.
// Both are validated before patching.
constexpr std::uintptr_t kProjEpilogueRva = 0x11a4b4;
constexpr std::uintptr_t kProjStoreRva    = 0x11a49d;
const unsigned char kProjEpilogueBytes[] = {0x48, 0x81, 0xc4, 0x00, 0x01,
                                            0x00, 0x00, 0x5d, 0xc3};
const unsigned char kProjStoreBytes[] = {0x0f, 0x29, 0x00};

// Viewport wrapper: detour point, return address, and frame marker.
constexpr std::uintptr_t kViewportHookRva   = 0x17dd3;
constexpr std::uintptr_t kViewportReturnRva = 0x17dd8;
constexpr std::uintptr_t kFrameMarkerRva    = 0x5cfb1;
const unsigned char kViewportHookBytes[] = {0x0f, 0x57, 0xc0, 0x8b, 0xc2};

// Frame start: the island uses it to reset its state.
constexpr std::uintptr_t kFrameStartRva = 0x1c915;

// Display context. +0x2970 is the table index.
constexpr std::uintptr_t kDisplayContextRva = 0x15929d8;
constexpr std::uintptr_t kCtxSlotIndex = 0x2970;

std::uintptr_t g_base;
wchar_t g_ini[MAX_PATH];
wchar_t g_log[MAX_PATH];
volatile LONG g_log_lock;
int g_width, g_height;
bool g_fix_table = true;
bool g_fix_projection = true;
bool g_fix_hud = true;
bool g_projection_hooked;

int setting(const wchar_t* key, const wchar_t* legacy_key, int fallback) {
    constexpr wchar_t missing[] = L"__missing__";
    wchar_t value[32]{};
    GetPrivateProfileStringW(L"Fix", key, missing, value,
                             static_cast<DWORD>(sizeof(value) / sizeof(value[0])), g_ini);
    if (std::wcscmp(value, missing) != 0) return _wtoi(value);
    return GetPrivateProfileIntW(L"Ultrawide", legacy_key, fallback, g_ini);
}

void log_line(const char* fmt, ...) {
    while (InterlockedCompareExchange(&g_log_lock, 1, 0) != 0) Sleep(0);
    FILE* f = nullptr;
    if (_wfopen_s(&f, g_log, L"a") == 0 && f) {
        SYSTEMTIME t{};
        GetLocalTime(&t);
        std::fprintf(f, "[%02u:%02u:%02u.%03u] ", t.wHour, t.wMinute, t.wSecond,
                     t.wMilliseconds);
        va_list a;
        va_start(a, fmt);
        std::vfprintf(f, fmt, a);
        va_end(a);
        std::fputc('\n', f);
        std::fclose(f);
    }
    InterlockedExchange(&g_log_lock, 0);
}

bool resolve_paths() {
    wchar_t exe[MAX_PATH]{};
    const DWORD n = GetModuleFileNameW(nullptr, exe, MAX_PATH);
    if (!n || n >= MAX_PATH) return false;
    wchar_t* slash = std::wcsrchr(exe, L'\\');
    if (!slash) return false;
    *slash = 0;
    std::swprintf(g_ini, MAX_PATH, L"%ls\\PeaceWalkerUltraWideFix.ini", exe);
    std::swprintf(g_log, MAX_PATH, L"%ls\\PeaceWalkerUltraWideFix.log", exe);
    if (GetFileAttributesW(g_ini) == INVALID_FILE_ATTRIBUTES) {
        wchar_t legacy[MAX_PATH]{};
        std::swprintf(legacy, MAX_PATH, L"%ls\\pw_ultrawide.ini", exe);
        if (GetFileAttributesW(legacy) != INVALID_FILE_ATTRIBUTES) {
            std::wcsncpy(g_ini, legacy, MAX_PATH - 1);
            g_ini[MAX_PATH - 1] = 0;
        }
    }
    return true;
}

bool supported_executable() {
    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(g_base);
    if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(
        g_base + static_cast<std::uintptr_t>(dos->e_lfanew));
    return nt->Signature == IMAGE_NT_SIGNATURE &&
           nt->FileHeader.TimeDateStamp == kTimeDateStamp &&
           nt->OptionalHeader.SizeOfImage == kSizeOfImage;
}

// Physical primary-monitor resolution in real pixels. The process is not DPI
// aware, so GetSystemMetrics would return scaled values when scaling is active.
void physical_desktop_size(int* width, int* height) {
    *width = 0;
    *height = 0;
    const HDC dc = GetDC(nullptr);
    if (!dc) return;
    *width = GetDeviceCaps(dc, DESKTOPHORZRES);
    *height = GetDeviceCaps(dc, DESKTOPVERTRES);
    ReleaseDC(nullptr, dc);
}

bool write_protected(void* target, const void* data, std::size_t size) {
    DWORD old = 0;
    if (!VirtualProtect(target, size, PAGE_READWRITE, &old)) return false;
    std::memcpy(target, data, size);
    DWORD ignored = 0;
    VirtualProtect(target, size, old, &ignored);
    FlushInstructionCache(GetCurrentProcess(), target, size);
    return true;
}

// Allocate an executable page within relative-jump range (+/-2 GB) of the site.
void* allocate_island_near(std::uintptr_t anchor) {
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    const std::uintptr_t granularity = si.dwAllocationGranularity;
    const std::uintptr_t start = anchor & ~(granularity - 1);
    for (std::uintptr_t delta = granularity; delta < 0x40000000; delta += granularity) {
        for (int direction = 0; direction < 2; ++direction) {
            const std::uintptr_t candidate = direction ? start + delta : start - delta;
            if (candidate < granularity) continue;
            void* page = VirtualAlloc(reinterpret_cast<void*>(candidate), granularity,
                                      MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
            if (page) return page;
        }
    }
    return nullptr;
}

// Write a five-byte relative jump and fill the remaining bytes with int3.
bool write_jump(std::uintptr_t site, std::size_t site_size, const void* target) {
    const std::intptr_t relative = reinterpret_cast<std::intptr_t>(target) -
                                   static_cast<std::intptr_t>(site + 5);
    if (relative > 0x7fffffff || relative < -0x7fffffff) return false;
    unsigned char patch[16];
    if (site_size > sizeof(patch)) return false;
    std::memset(patch, 0xcc, site_size);
    patch[0] = 0xe9;
    const std::int32_t rel32 = static_cast<std::int32_t>(relative);
    std::memcpy(patch + 1, &rel32, sizeof(rel32));
    return write_protected(reinterpret_cast<void*>(site), patch, site_size);
}

// SteamStub encrypts the code section and decrypts it at its entry point, which
// runs after the loader resolves imports -- and therefore after this module.
// Wait for the signatures to appear. The resolution table lives in .rdata,
// which is not encrypted, so it can be read immediately.
bool wait_for_code(unsigned timeout_ms) {
    for (unsigned waited = 0; waited < timeout_ms; waited += 25) {
        const auto* epilogue =
            reinterpret_cast<const unsigned char*>(g_base + kProjEpilogueRva);
        const auto* store =
            reinterpret_cast<const unsigned char*>(g_base + kProjStoreRva);
        const auto* viewport =
            reinterpret_cast<const unsigned char*>(g_base + kViewportHookRva);
        if (std::memcmp(epilogue, kProjEpilogueBytes, sizeof(kProjEpilogueBytes)) == 0 &&
            std::memcmp(store, kProjStoreBytes, sizeof(kProjStoreBytes)) == 0 &&
            std::memcmp(viewport, kViewportHookBytes, sizeof(kViewportHookBytes)) == 0) {
            if (waited) log_line("Code decrypted after %u ms.", waited);
            return true;
        }
        Sleep(25);
    }
    return false;
}

int choose_slot(int height) {
    const auto context =
        *reinterpret_cast<std::uintptr_t const volatile*>(g_base + kDisplayContextRva);
    if (context) {
        const int index = *reinterpret_cast<const int*>(context + kCtxSlotIndex);
        if (index >= 0 && index < kResolutionSlots) return index;
    }
    const auto* table = reinterpret_cast<const std::int32_t*>(g_base + kResolutionTableRva);
    for (int i = 0; i < kResolutionSlots; ++i)
        if (table[i * 2 + 1] == height) return i;
    return -1;
}

bool patch_resolution_table(int slot, int width, int height) {
    auto* entry = reinterpret_cast<std::int32_t*>(
        g_base + kResolutionTableRva + static_cast<std::uintptr_t>(slot) * 8);
    if (entry[0] == width && entry[1] == height) return true;
    const std::int32_t values[2] = {width, height};
    return write_protected(entry, values, sizeof(values));
}

bool projection_looks_valid(const float* m) {
    return m[kProjM11 / 4] == -1.0f && m[kProjM15 / 4] == 0.0f &&
           m[kProjM5 / 4] > 0.01f && m[kProjM5 / 4] < 1000.0f;
}

bool install_projection_hook(float aspect) {
    void* island = allocate_island_near(g_base + kProjEpilogueRva);
    if (!island) {
        log_line("Projection: unable to allocate memory for the code island.");
        return false;
    }
    unsigned char code[pw::kProjectionIslandSize];
    pw::build_projection_island(
        code, static_cast<std::uint64_t>(g_base + kProjectionRva), aspect);
    std::memcpy(island, code, sizeof(code));
    if (!write_jump(g_base + kProjEpilogueRva, sizeof(kProjEpilogueBytes), island)) {
        log_line("Projection: unable to write the detour jump.");
        return false;
    }
    log_line("Projection corrected at its source (0x%llx).",
             static_cast<unsigned long long>(kProjEpilogueRva));
    return true;
}

bool install_hud_hook(int x, int width) {
    void* island = allocate_island_near(g_base + kViewportHookRva);
    if (!island) {
        log_line("HUD: unable to allocate memory for the code island.");
        return false;
    }
    const auto address = reinterpret_cast<std::uint64_t>(island);
    unsigned char code[pw::kHudIslandSize];
    pw::build_hud_island(code, address,
                         static_cast<std::uint64_t>(g_base + kFrameStartRva),
                         static_cast<std::uint64_t>(g_base + kFrameMarkerRva),
                         static_cast<std::uint64_t>(g_base + kViewportReturnRva),
                         x, width);
    std::memcpy(island, code, sizeof(code));
    if (!write_jump(g_base + kViewportHookRva, sizeof(kViewportHookBytes), island)) {
        log_line("HUD: unable to write the detour jump.");
        return false;
    }
    log_line("Interface in 16:9 band: internal viewport X=%d, width=%d of %d.",
             x, width, pw::kInternalWidth);
    return true;
}

// Reassert the resolution table and, if the projection hook could not be
// installed, maintain the projection from here. That fallback flickers and is
// reported in the log.
DWORD WINAPI maintain(LPVOID) {
    auto* table = reinterpret_cast<std::int32_t*>(g_base + kResolutionTableRva);
    auto* projection = reinterpret_cast<float*>(g_base + kProjectionRva);
    const float aspect = static_cast<float>(g_width) / static_cast<float>(g_height);
    int slot = -1;
    bool announced = false;

    for (;;) {
        if (g_fix_table) {
            if (slot < 0) slot = choose_slot(g_height);
            if (slot >= 0) {
                if (table[slot * 2] != g_width || table[slot * 2 + 1] != g_height)
                    patch_resolution_table(slot, g_width, g_height);
                if (!announced) {
                    announced = true;
                    log_line("Resolution table: entry [%d] = %dx%d.", slot,
                             g_width, g_height);
                }
            }
        }
        if (g_fix_projection && !g_projection_hooked &&
            projection_looks_valid(projection)) {
            const float wanted = projection[kProjM5 / 4] / aspect;
            if (projection[kProjM0 / 4] != wanted) projection[kProjM0 / 4] = wanted;
        }
        Sleep(g_projection_hooked ? 250 : 0);
    }
}

DWORD WINAPI initialize(LPVOID) {
    if (!resolve_paths()) return 0;
    DeleteFileW(g_log);
    log_line("---- Peace Walker UltraWide Fix %s ----", PWUWFIX_VERSION);

    if (setting(L"Enabled", L"Enabled", 1) == 0) {
        log_line("Enabled=0: no changes will be applied.");
        return 0;
    }
    g_fix_table = setting(L"RemoveLetterboxing", L"FixLetterbox", 1) != 0;
    g_fix_projection = setting(L"CorrectFOV", L"FixProjection", 1) != 0;
    g_fix_hud = setting(L"CenterHUD", L"KeepHudAt16by9", 1) != 0;

    g_base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if (!supported_executable()) {
        log_line("ERROR: unrecognized executable; no changes will be applied.");
        return 0;
    }

    physical_desktop_size(&g_width, &g_height);
    const int cfg_w = setting(L"Width", L"Width", 0);
    const int cfg_h = setting(L"Height", L"Height", 0);
    if (cfg_w > 0 && cfg_h > 0) { g_width = cfg_w; g_height = cfg_h; }
    if (g_width <= 0 || g_height <= 0) {
        log_line("ERROR: unable to determine the display resolution.");
        return 0;
    }

    const float aspect = static_cast<float>(g_width) / static_cast<float>(g_height);
    log_line("Output %dx%d (aspect %.5f). Native 16:9 = 1.77778.", g_width, g_height,
             aspect);
    if (aspect <= 16.0f / 9.0f + 0.001f) {
        log_line("The display is not wider than 16:9; no correction is needed.");
        return 0;
    }

    // Start the maintenance thread before waiting for decryption. If the table
    // is patched late, the game has already calculated the framing and the
    // image appears offset.
    CloseHandle(CreateThread(nullptr, 0, maintain, nullptr, 0, nullptr));

    if (!wait_for_code(30000)) {
        log_line("WARNING: code does not match the known signatures. No hooks "
                 "will be installed; only the resolution-table fix remains.");
        return 0;
    }

    if (g_fix_projection) {
        g_projection_hooked = install_projection_hook(aspect);
        if (!g_projection_hooked)
            log_line("WARNING: without the hook, the projection is maintained by "
                     "a thread and will flicker.");
    }

    if (g_fix_hud) {
        int x = 0, width = 0;
        if (pw::hud_band(g_width, g_height, &x, &width))
            install_hud_hook(x, width);
    } else {
        log_line("CenterHUD=0: the interface keeps the game's widescreen behavior.");
    }
    return 0;
}

}  // namespace

// The generated .def exports these two under their own names. Like the other
// exports, they only forward to the system winmm.
extern "C" FARPROC winmm_proxy_resolve_by_name(const char* name);

extern "C" MMRESULT WINAPI pw_timeBeginPeriod(UINT period) {
    using Fn = MMRESULT(WINAPI*)(UINT);
    auto fn = reinterpret_cast<Fn>(winmm_proxy_resolve_by_name("timeBeginPeriod"));
    return fn ? fn(period) : TIMERR_NOERROR;
}

extern "C" DWORD WINAPI pw_timeGetTime() {
    using Fn = DWORD(WINAPI*)();
    auto fn = reinterpret_cast<Fn>(winmm_proxy_resolve_by_name("timeGetTime"));
    return fn ? fn() : GetTickCount();
}

extern "C" void pw_ultrawide_start() {
    static LONG started = 0;
    if (InterlockedCompareExchange(&started, 1, 0) != 0) return;
    CloseHandle(CreateThread(nullptr, 0, initialize, nullptr, 0, nullptr));
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        pw_ultrawide_start();
    }
    return TRUE;
}
