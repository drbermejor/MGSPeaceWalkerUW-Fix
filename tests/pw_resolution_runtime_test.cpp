// Actual Windows memory/startup/monitor tests. No real graphics resources/GPU.
#include "../src/pw_ultrawide.cpp"
#include <cstdlib>
extern "C" FARPROC winmm_proxy_resolve_by_name(const char*) { return nullptr; }
namespace {
int failures = 0;
void check(bool ok, const char* label) {
    if (!ok) { std::printf("FAIL: %s\n", label); ++failures; }
}
pw::LaunchResolution parse(const wchar_t* command) {
    int count = 0;
    auto** args = CommandLineToArgvW(command, &count);
    auto mode = pw::launch_resolution(count, args);
    if (args) LocalFree(args);
    return mode;
}
void parser_tests() {
    check(parse(L"game.exe").valid && parse(L"game.exe").slot == 0, "absent modes default to zero");
    for (int res = 0; res <= 1; ++res) for (int up = 0; up <= 3; ++up) {
        wchar_t command[256]{};
        std::swprintf(command, 256, L"\"C:\\Game Folder\\game.exe\" -region eu -resolution %d -upscale %d", res, up);
        auto mode = parse(command);
        check(mode.valid && mode.slot == (res > up ? res : up), "all supported mode combinations");
    }
    check(parse(L"game.exe -upscale \"2\" -resolution 1").slot == 2, "quoted value/reversed arguments");
    check(parse(L"game.exe -resolution 1").slot == 1, "resolution only");
    check(parse(L"game.exe -upscale 3").slot == 3, "upscale only");
    const wchar_t* rejected[] = {
        L"game.exe -resolution", L"game.exe -upscale", L"game.exe -resolution 2",
        L"game.exe -upscale 4", L"game.exe -upscale -1", L"game.exe -upscale 2junk",
        L"game.exe -upscale 02", L"game.exe -upscale \"\"",
        L"game.exe -upscale 1 -upscale 2", L"game.exe -resolution 1 -resolution 1",
        L"game.exe -resolution -upscale 2"
    };
    for (auto command : rejected) check(!parse(command).valid, "ambiguous/invalid modes refused");
    check(!pw::launch_resolution(0, nullptr).valid, "null argv refused");
    const wchar_t* many[33]{};
    for (auto& arg : many) arg = L"x";
    check(!pw::launch_resolution(33, many).valid, "32 argument limit respected");
}
}
int main() {
    parser_tests();
    check(early_resolution_default(&kProfiles[1]), "audited build defaults to early resolution");
    check(!early_resolution_default(&kProfiles[0]), "older build retains legacy default");
    check(!early_resolution_default(nullptr), "signature-only build retains legacy default");
    auto mismatched = kProfiles[1];
    mismatched.image_size = kProfiles[0].image_size;
    check(!early_resolution_default(&mismatched), "timestamp alone cannot enable early resolution");
    auto* memory = static_cast<unsigned char*>(VirtualAlloc(nullptr, 0x5000,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (!memory) return 2;
    g_resolution_table_address = reinterpret_cast<std::uintptr_t>(memory);
    const std::int32_t original[8] = {1280,720,1920,1080,2560,1440,3840,2160};
    std::memcpy(memory, original, sizeof(original));
    DWORD old = 0;
    check(VirtualProtect(memory, 0x1000, PAGE_READONLY, &old) != 0, "read-only fixture");
    const auto context = reinterpret_cast<std::uintptr_t>(memory + 0x1000);
    auto* active = reinterpret_cast<int*>(context + kCtxSlotIndex);
    std::uintptr_t context_pointer = 0;
    const auto address = reinterpret_cast<std::uintptr_t>(&context_pointer);
    g_width = 5120; g_height = 2160; g_fix_table = true; g_early_resolution = true;
    ResolutionMonitor state;
    check(apply_early_resolution(state, parse(L"game.exe -resolution 1 -upscale 2").slot, address),
          "synchronous write before context publication");
    std::int32_t rows[8]{};
    check(read_resolution_table(rows) && rows[4] == 5120 && rows[5] == 2160 &&
          rows[0] == 1280 && rows[2] == 1920 && rows[6] == 3840 && rows[7] == 2160,
          "only row 2 receives target, not height-selected row 3");
    check(state.attempts == 1 && state.announced, "single verified startup write");
    MEMORY_BASIC_INFORMATION info{};
    VirtualQuery(memory, &info, sizeof(info));
    check(info.Protect == PAGE_READONLY, "protection restored");
    *active = 2; context_pointer = context; g_display_context_address.store(address);
    const std::int32_t correct[4] = {5120,2160,0,0};
    std::memcpy(reinterpret_cast<void*>(context + 0x2940), correct, sizeof(correct));
    std::memcpy(reinterpret_cast<void*>(context + 0x29a0), correct, 8);
    FramingSnapshot snapshot;
    check(read_framing_snapshot(snapshot) && framing_matches_target(snapshot, 2, 5120, 2160),
          "correct framing/descriptor recognized");
    maintain_resolution_once(state);
    check(!state.inconsistent && state.attempts == 1, "normal monitor does not write");
    // Audited SSE float/truncation arithmetic for the stale startup row.
    const int canvas_w = static_cast<int>(5120.0f / (2160.0f / 1440.0f));
    const int origin_x = static_cast<int>(static_cast<float>(canvas_w - 2560) * 0.5f);
    check(canvas_w == 3413 && origin_x == 426, "framing regression arithmetic");
    const std::int32_t stale[4] = {canvas_w,1440,origin_x,0};
    std::memcpy(reinterpret_cast<void*>(context + 0x2940), stale, sizeof(stale));
    check(read_framing_snapshot(snapshot) && !framing_matches_target(snapshot, 2, 5120, 2160),
          "test.1 stale framing rejected despite correct table");
    std::memcpy(reinterpret_cast<void*>(context + 0x2940), correct, sizeof(correct));
    std::memcpy(reinterpret_cast<void*>(context + 0x29a0), stale, 8);
    check(read_framing_snapshot(snapshot) && !framing_matches_target(snapshot, 2, 5120, 2160),
          "stale output descriptor rejected");
    ResolutionMonitor late;
    check(!apply_early_resolution(late, 3, address), "published context blocks late write");
    check(!apply_early_resolution(late, 3, 1), "unreadable context blocks write");
    check(!apply_early_resolution(late, -1, address) && !apply_early_resolution(late, 4, address),
          "invalid slots rejected");
    check(read_resolution_table(rows) && rows[6] == 3840, "late refusal preserves table");
    *active = 3;
    for (int i = 0; i < 4; ++i) maintain_resolution_once(state);
    check(state.slot == 2 && state.inconsistent && state.attempts == 1, "no live migration");
    *active = 2;
    const std::int32_t restored[2] = {2560,1440};
    check(write_protected(memory + 16, restored, sizeof(restored)), "simulate restored row");
    maintain_resolution_once(state);
    check(read_resolution_table(rows) && rows[4] == 2560 && state.attempts == 1,
          "no late reassertion after restore");
    *active = 9;
    check(active_resolution_slot() == -1, "invalid active slot rejected");
    context_pointer = 1;
    check(active_resolution_slot() == -1 && !read_framing_snapshot(snapshot), "unreadable context safe");
    const int sizes[][2] = {{2560,1080}, {3440,1440}, {5120,1440}};
    for (const auto& size : sizes) {
        check(write_protected(memory, original, sizeof(original)), "reset fixture");
        context_pointer = 0; g_width = size[0]; g_height = size[1];
        ResolutionMonitor other;
        check(apply_early_resolution(other, 2, address) && read_resolution_table(rows) &&
              rows[4] == g_width && rows[5] == g_height && rows[2] == 1920 && rows[6] == 3840,
              "early row across output resolutions");
    }
    check(write_protected(memory, original, sizeof(original)), "reset fixture");
    g_fix_table = false;
    ResolutionMonitor disabled;
    check(apply_early_resolution(disabled, 2, address), "disabled fix permits diagnostics");
    maintain_resolution_once(disabled);
    check(read_resolution_table(rows) && std::memcmp(rows, original, sizeof(original)) == 0,
          "RemoveLetterboxing=0 never writes");
    g_early_resolution = false; g_fix_table = true; g_width = 5120; g_height = 2160;
    context_pointer = context; *active = 2;
    ResolutionMonitor legacy;
    for (int i = 0; i < 4; ++i) maintain_resolution_once(legacy);
    check(read_resolution_table(rows) && legacy.slot == 3 && rows[4] == 2560 && rows[6] == 5120,
          "comparison mode retains rc.5 height selection without migration");
    VirtualProtect(memory, 0x1000, PAGE_NOACCESS, &old);
    check(!patch_resolution_table(0,5120,2160), "inaccessible table fails verification");
    g_display_context_address.store(0);
    VirtualFree(memory, 0, MEM_RELEASE);
    int x = 0, width = 0;
    check(pw::hud_band(5120,2160,&x,&width) && x == 240 && width == 1440, "5K2K HUD math");
    check(pw::hud_band(2560,1080,&x,&width) && x == 240 && width == 1440, "1080 ultrawide HUD math");
    std::printf("Resolution runtime checks: %d failure(s).\n", failures);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
