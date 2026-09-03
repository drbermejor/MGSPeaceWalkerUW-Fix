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
// Known PE profiles and independent signatures are both validated before code
// hooks are installed. A signature-only candidate must also pass structural
// call-target and data-section checks; ambiguity always fails closed.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <limits>

#include "pw_hud_island.h"
#include "pw_patch_targets.h"
#include "pw_projection_island.h"

#ifndef PWUWFIX_VERSION
#define PWUWFIX_VERSION "development"
#endif

namespace {

// Exact profiles remain the fastest and strongest path for builds already
// tested in the game. The signature resolver is a compatibility path for a
// future address-only update; it never replaces these known identities.
struct ExecutableProfile {
    const char* name;
    DWORD timestamp;
    DWORD image_size;
    std::uintptr_t resolution_table;
    std::uintptr_t projection;
    std::uintptr_t projection_epilogue;
    std::uintptr_t projection_store;
    std::uintptr_t viewport_hook;
    std::uintptr_t viewport_return;
    std::uintptr_t frame_marker;
    std::uintptr_t frame_start;
    std::uintptr_t display_context;
};

constexpr ExecutableProfile kProfiles[] = {
    {"Steam build before 25052315", 0x6a6cc50e, 0x0199e000,
     0xd8f1b8, 0x10f4330, 0x11a4b4, 0x11a49d, 0x17dd3, 0x17dd8,
     0x5cfb1, 0x1c915, 0x15929d8},
    {"Steam build 25052315", 0x6a964726, 0x019a2000,
     0xd8f1c8, 0x10f8330, 0x11a604, 0x11a5ed, 0x17dd3, 0x17dd8,
     0x5d021, 0x1c915, 0x15969d8},
};

const ExecutableProfile* g_profile;

struct PatchTargets {
    std::uintptr_t resolution_table;
    std::uintptr_t projection;
    std::uintptr_t projection_epilogue;
    std::uintptr_t projection_store;
    std::uintptr_t viewport_hook;
    std::uintptr_t viewport_return;
    std::uintptr_t frame_marker;
    std::uintptr_t frame_start;
    std::uintptr_t display_context;
};

PatchTargets g_targets{};
DWORD g_timestamp;
DWORD g_image_size;
bool g_signature_fallback;

// Resolution table. Four 8-byte 16:9 entries: (width, height).
//   [0] 1280x720   [1] 1920x1080   [2] 2560x1440   [3] 3840x2160
constexpr int kResolutionSlots = 4;

// Projection matrix, 4x4 floats. PSP perspective signature:
// m11 = -1.0, m15 = 0.
constexpr std::uintptr_t kProjM0 = 0x00, kProjM5 = 0x14;
constexpr std::uintptr_t kProjM11 = 0x2c, kProjM15 = 0x3c;

// Epilogue of the matrix builder and the instruction that writes the matrix.
// Both are validated before patching.
constexpr std::size_t kProjEpilogueSize = 9;
constexpr std::size_t kViewportHookSize = 5;
constexpr std::uintptr_t kViewportHookFromFunction = 0x13;

// Display context. +0x2970 is the table index.
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
std::atomic<std::uintptr_t> g_display_context_address{};

bool projection_looks_valid(const float* matrix);

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
            wcsncpy_s(g_ini, MAX_PATH, legacy, _TRUNCATE);
        }
    }
    return true;
}

bool read_executable_identity() {
    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(g_base);
    if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(
        g_base + static_cast<std::uintptr_t>(dos->e_lfanew));
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        return false;
    }
    g_timestamp = nt->FileHeader.TimeDateStamp;
    g_image_size = nt->OptionalHeader.SizeOfImage;
#if !defined(PWUWFIX_FORCE_SIGNATURE_FALLBACK)
    for (const auto& profile : kProfiles) {
        if (g_timestamp == profile.timestamp &&
            g_image_size == profile.image_size) {
            g_profile = &profile;
            break;
        }
    }
#endif
    return true;
}

std::uintptr_t to_rva(std::uintptr_t address) {
    return address >= g_base ? address - g_base : 0;
}

bool resolve_resolution_table() {
    const auto rdata = pw::signature::mapped_image_section(g_base, ".rdata");
    const auto match = pw::signature::find_unique(
        rdata, pw::targets::kResolutionTable);
    if (match.state != pw::signature::MatchState::unique) {
        log_line("ERROR: resolution-table signature has %llu matches in .rdata; "
                 "expected exactly one.",
                 static_cast<unsigned long long>(match.count));
        return false;
    }

    const std::uintptr_t resolved = to_rva(match.address);
    if (g_profile && resolved != g_profile->resolution_table) {
        log_line("ERROR: known executable resolved the resolution table at "
                 "unexpected RVA 0x%llx.",
                 static_cast<unsigned long long>(resolved));
        return false;
    }
    g_targets.resolution_table = resolved;
    return true;
}

bool targets_match_profile(const PatchTargets& targets) {
    return g_profile &&
        targets.resolution_table == g_profile->resolution_table &&
        targets.projection == g_profile->projection &&
        targets.projection_epilogue == g_profile->projection_epilogue &&
        targets.projection_store == g_profile->projection_store &&
        targets.viewport_hook == g_profile->viewport_hook &&
        targets.viewport_return == g_profile->viewport_return &&
        targets.frame_marker == g_profile->frame_marker &&
        targets.frame_start == g_profile->frame_start &&
        targets.display_context == g_profile->display_context;
}

bool resolve_code_targets() {
    const auto text = pw::signature::mapped_image_section(g_base, ".text");
    const auto data = pw::signature::mapped_image_section(g_base, ".data");
    if (!text.valid() || !data.valid()) return false;

    const auto projection = pw::signature::find_unique(
        text, pw::targets::kProjectionTail);
    const auto viewport = pw::signature::find_unique(
        text, pw::targets::kViewportHook);
    const auto frame_start = pw::signature::find_unique(
        text, pw::targets::kFrameStart);
    const auto frame_marker = pw::signature::find_unique(
        text, pw::targets::kFrameMarker);
    const auto display = pw::signature::find_unique(
        text, pw::targets::kDisplayContext);
    if (projection.state != pw::signature::MatchState::unique ||
        viewport.state != pw::signature::MatchState::unique ||
        frame_start.state != pw::signature::MatchState::unique ||
        frame_marker.state != pw::signature::MatchState::unique ||
        display.state != pw::signature::MatchState::unique) {
        return false;
    }

    const std::uintptr_t viewport_function =
        viewport.address - kViewportHookFromFunction;
    std::uintptr_t frame_start_target = 0;
    std::uintptr_t frame_marker_target = 0;
    if (!pw::signature::decode_relative_target(
            frame_start.address + pw::targets::kFrameStartCallOffset,
            1, 5, &frame_start_target) ||
        !pw::signature::decode_relative_target(
            frame_marker.address + pw::targets::kFrameMarkerCallOffset,
            1, 5, &frame_marker_target) ||
        frame_start_target != viewport_function ||
        frame_marker_target != viewport_function) {
        return false;
    }

    std::uintptr_t display_context = 0;
    if (!pw::signature::decode_relative_target(
            display.address +
                pw::targets::kDisplayContextInstructionOffset,
            3, 7, &display_context) ||
        display_context < pw::targets::kProjectionFromDisplayContext) {
        return false;
    }
    const std::uintptr_t projection_matrix =
        display_context - pw::targets::kProjectionFromDisplayContext;
    if (!pw::signature::contains(data, display_context,
                                 sizeof(std::uintptr_t)) ||
        !pw::signature::contains(data, projection_matrix, 16 * sizeof(float)) ||
        !projection_looks_valid(
            reinterpret_cast<const float*>(projection_matrix))) {
        return false;
    }

    PatchTargets resolved = g_targets;
    resolved.projection = to_rva(projection_matrix);
    resolved.projection_store = to_rva(
        projection.address + pw::targets::kProjectionStoreOffset);
    resolved.projection_epilogue = to_rva(
        projection.address + pw::targets::kProjectionEpilogueOffset);
    resolved.viewport_hook = to_rva(viewport.address);
    resolved.viewport_return = resolved.viewport_hook + kViewportHookSize;
    resolved.frame_start = to_rva(
        frame_start.address + pw::targets::kFrameStartReturnOffset);
    resolved.frame_marker = to_rva(
        frame_marker.address + pw::targets::kFrameMarkerReturnOffset);
    resolved.display_context = to_rva(display_context);

    if (resolved.projection_epilogue - resolved.projection_store != 0x17 ||
        resolved.viewport_return - resolved.viewport_hook !=
            kViewportHookSize) {
        return false;
    }
    if (g_profile && !targets_match_profile(resolved)) return false;

    g_targets = resolved;
    g_display_context_address.store(g_base + resolved.display_context,
                                    std::memory_order_release);
    return true;
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

// Code is patched after SteamStub decrypts .text and other game threads may
// already be running from the same memory page. Keep execute permission while
// writing code; temporarily making the whole page PAGE_READWRITE creates a
// repeatable DEP race in nearby instructions. Data writes do not need execute.
bool write_protected(void* target, const void* data, std::size_t size,
                     bool executable = false) {
    DWORD old = 0;
    const DWORD writable =
        executable ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;
    if (!VirtualProtect(target, size, writable, &old)) return false;
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
            if ((!direction && delta > start) ||
                (direction &&
                 start > (std::numeric_limits<std::uintptr_t>::max)() - delta)) {
                continue;
            }
            const std::uintptr_t candidate =
                direction ? start + delta : start - delta;
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
    return write_protected(reinterpret_cast<void*>(site), patch, site_size,
                           true);
}

// SteamStub encrypts .text on disk and decrypts it at the entry point, after
// the loader has already loaded this proxy. Search only the mapped .text after
// decryption. The complete locators are intentionally longer than the small
// byte sequences overwritten by the hooks.
bool wait_for_code(unsigned timeout_ms) {
    for (unsigned waited = 0; waited < timeout_ms; waited += 25) {
        if (resolve_code_targets()) {
            if (waited) log_line("Code decrypted after %u ms.", waited);
            return true;
        }
        Sleep(25);
    }
    const auto text = pw::signature::mapped_image_section(g_base, ".text");
    const auto report = [&](const char* name,
                            const pw::signature::BytePattern& pattern) {
        const auto result = pw::signature::find_unique(text, pattern);
        const char* state = result.state == pw::signature::MatchState::unique
            ? "unique"
            : result.state == pw::signature::MatchState::ambiguous
                ? "ambiguous"
                : "missing";
        log_line("Signature diagnostic: %s = %s.", name, state);
    };
    report("projection tail", pw::targets::kProjectionTail);
    report("viewport wrapper", pw::targets::kViewportHook);
    report("frame start", pw::targets::kFrameStart);
    report("frame marker", pw::targets::kFrameMarker);
    report("display context", pw::targets::kDisplayContext);
    log_line("Signature diagnostic: a unique-looking set can still be rejected "
             "when a call target, data range, matrix shape or known-profile "
             "comparison is inconsistent.");
    return false;
}

int choose_slot(int height) {
    const auto* table = reinterpret_cast<const std::int32_t*>(
        g_base + g_targets.resolution_table);
    for (int i = 0; i < kResolutionSlots; ++i)
        if (table[i * 2 + 1] == height) return i;

    // Non-standard heights do not identify one of the four table rows. Once
    // code signatures have resolved the display context, its active row is a
    // safe fallback. Standard heights never depend on this late value.
    const auto context_address =
        g_display_context_address.load(std::memory_order_acquire);
    if (context_address) {
        const auto context =
            *reinterpret_cast<std::uintptr_t const volatile*>(context_address);
        if (context) {
            const int index = *reinterpret_cast<const int*>(context + kCtxSlotIndex);
            if (index >= 0 && index < kResolutionSlots) return index;
        }
    }
    return -1;
}

bool patch_resolution_table(int slot, int width, int height) {
    auto* entry = reinterpret_cast<std::int32_t*>(
        g_base + g_targets.resolution_table +
        static_cast<std::uintptr_t>(slot) * 8);
    if (entry[0] == width && entry[1] == height) return true;
    const std::int32_t values[2] = {width, height};
    return write_protected(entry, values, sizeof(values));
}

bool projection_looks_valid(const float* m) {
    return m[kProjM11 / 4] == -1.0f && m[kProjM15 / 4] == 0.0f &&
           m[kProjM5 / 4] > 0.01f && m[kProjM5 / 4] < 1000.0f;
}

bool install_projection_hook(float aspect) {
    void* island = allocate_island_near(g_base + g_targets.projection_epilogue);
    if (!island) {
        log_line("Projection: unable to allocate memory for the code island.");
        return false;
    }
    unsigned char code[pw::kProjectionIslandSize];
    pw::build_projection_island(
        code, static_cast<std::uint64_t>(g_base + g_targets.projection), aspect);
    std::memcpy(island, code, sizeof(code));
    if (!write_jump(g_base + g_targets.projection_epilogue,
                    kProjEpilogueSize, island)) {
        VirtualFree(island, 0, MEM_RELEASE);
        log_line("Projection: unable to write the detour jump.");
        return false;
    }
    log_line("Projection corrected at its source (0x%llx).",
             static_cast<unsigned long long>(g_targets.projection_epilogue));
    return true;
}

bool install_hud_hook(int x, int width) {
    void* island = allocate_island_near(g_base + g_targets.viewport_hook);
    if (!island) {
        log_line("HUD: unable to allocate memory for the code island.");
        return false;
    }
    const auto address = reinterpret_cast<std::uint64_t>(island);
    unsigned char code[pw::kHudIslandSize];
    pw::build_hud_island(code, address,
                         static_cast<std::uint64_t>(g_base + g_targets.frame_start),
                         static_cast<std::uint64_t>(g_base + g_targets.frame_marker),
                         static_cast<std::uint64_t>(g_base + g_targets.viewport_return),
                         x, width);
    std::memcpy(island, code, sizeof(code));
    if (!write_jump(g_base + g_targets.viewport_hook,
                    kViewportHookSize, island)) {
        VirtualFree(island, 0, MEM_RELEASE);
        log_line("HUD: unable to write the detour jump.");
        return false;
    }
    log_line("Interface in 16:9 band: internal viewport X=%d, width=%d of %d.",
             x, width, pw::kInternalWidth);
    return true;
}

// Reassert the selected table row because the game can restore it later. This
// thread starts before .text is decrypted; delaying it until hook resolution
// lets the game calculate its initial framing from the original 16:9 row.
DWORD WINAPI maintain_resolution_table(LPVOID) {
    auto* table = reinterpret_cast<std::int32_t*>(
        g_base + g_targets.resolution_table);
    int slot = -1;
    bool announced = false;

    for (;;) {
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
        Sleep(250);
    }
}

// This is retained only as a last-resort fallback if the source hook cannot be
// installed after all targets were validated. It races the game's writer and
// may flicker, which is why the source hook remains the normal path.
DWORD WINAPI maintain_projection(LPVOID) {
    auto* projection = reinterpret_cast<float*>(
        g_base + g_targets.projection);
    const float aspect = static_cast<float>(g_width) /
                         static_cast<float>(g_height);
    for (;;) {
        if (projection_looks_valid(projection)) {
            const float wanted = projection[kProjM5 / 4] / aspect;
            if (projection[kProjM0 / 4] != wanted)
                projection[kProjM0 / 4] = wanted;
        }
        Sleep(0);
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
    if (!read_executable_identity()) {
        log_line("ERROR: invalid executable image; no changes will be applied.");
        return 0;
    }
    if (g_profile) {
        log_line("Executable profile: %s.", g_profile->name);
    } else {
        g_signature_fallback = true;
#if defined(PWUWFIX_FORCE_SIGNATURE_FALLBACK)
        log_line("Maintainer test: forcing signature compatibility for "
                 "timestamp=0x%08lx, image=0x%08lx.",
                 static_cast<unsigned long>(g_timestamp),
                 static_cast<unsigned long>(g_image_size));
#else
        log_line("Unknown PE identity: timestamp=0x%08lx, image=0x%08lx. "
                 "Trying the fail-closed signature compatibility path.",
                 static_cast<unsigned long>(g_timestamp),
                 static_cast<unsigned long>(g_image_size));
#endif
    }
    if (!resolve_resolution_table()) {
        log_line("ERROR: no changes will be applied.");
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
    if (g_fix_table) {
        const HANDLE thread = CreateThread(
            nullptr, 0, maintain_resolution_table, nullptr, 0, nullptr);
        if (thread) CloseHandle(thread);
    }

    if (!wait_for_code(30000)) {
        log_line("ERROR: the complete code signatures did not resolve uniquely. "
                 "No hooks will be installed; only a requested, independently "
                 "validated resolution-table fix can remain active.");
        return 0;
    }
    if (g_signature_fallback) {
        log_line("Signature compatibility accepted: all five code locators were "
                 "unique and their call/data-flow relationships were valid.");
    } else {
        log_line("Known profile independently confirmed by complete signatures.");
    }
    log_line("Resolved RVAs: table=0x%llx, projection=0x%llx, "
             "projection-store=0x%llx, viewport=0x%llx, frame-start=0x%llx, "
             "frame-marker=0x%llx, display-context=0x%llx.",
             static_cast<unsigned long long>(g_targets.resolution_table),
             static_cast<unsigned long long>(g_targets.projection),
             static_cast<unsigned long long>(g_targets.projection_store),
             static_cast<unsigned long long>(g_targets.viewport_hook),
             static_cast<unsigned long long>(g_targets.frame_start),
             static_cast<unsigned long long>(g_targets.frame_marker),
             static_cast<unsigned long long>(g_targets.display_context));

    if (g_fix_projection) {
        g_projection_hooked = install_projection_hook(aspect);
        if (!g_projection_hooked) {
            log_line("WARNING: without the hook, the projection is maintained by "
                     "a thread and will flicker.");
            const HANDLE thread = CreateThread(
                nullptr, 0, maintain_projection, nullptr, 0, nullptr);
            if (thread) CloseHandle(thread);
        }
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
    const HANDLE thread = CreateThread(nullptr, 0, initialize, nullptr, 0, nullptr);
    if (thread) CloseHandle(thread);
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        pw_ultrawide_start();
    }
    return TRUE;
}
