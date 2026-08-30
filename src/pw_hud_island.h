// Code island that fits the 2D interface into a centered 16:9 band while leaving
// the world full-screen.
//
// The game renders the world at 1920x1088 -- exactly 480x4 and 272x4, four times
// the PSP resolution -- into a separate target, then draws the interface over it
// in the same target. The entire binary has ONE RSSetViewports call (0x17E44),
// inside a wrapper at 0x17DC0 that receives integer (x, y, w, h) arguments. Five
// bytes in that wrapper (0x17DD3) are redirected.
//
// Frame structure measured in the live process:
//
//     0x1C915  3440x1440                 <- frame start
//     0x1CA1A  3440x1440
//     0x56345  1920x1088   x N           <- world
//     0x5CFB1  1920x1088   x1            <- marker
//     0x56345   256x256    x ~21         <- shadows, scene-dependent
//     0x56345  1920x1088   x M           <- 2D block
//     0x5C775  3440x1440                 <- frame end
//
//     gameplay     before=741  shadows=21  2D=48
//     menu         before=  1  shadows=15  2D=118
//     cutscene     before= 1  shadows= 0  2D=4
//
// When a world is present, the SECOND call in the 2D block composites it, so
// banding that call would put the world inside the band; processing must start
// at the third call. When no world is present -- menus and cutscenes -- there is
// nothing to skip, and starting at the third call leaves the video outside the
// band and stretched. The starting point is therefore selected from the number
// of calls BEFORE the marker, known when the marker arrives, without carrying
// state over from the previous frame.
//
//     frame start: flag = 0; before = 0; after = 0
//     marker:      flag = 1; after = 0
//                  from = (before > 64) ? 3 : 1
//     width 256:   after = 0       (shadows restart the count; some scenes have
//                                   an interleaved 1920-wide call that would
//                                   otherwise shift the numbering)
//     width 1920:  without flag -> before++
//                  with flag    -> after++; band if after >= from
//
// It only uses rax, r10, and flags, all of which it saves and restores.
//
// The machine code lives here, separate from the module, so tests can verify it
// byte by byte without loading the game. A bad displacement does not produce a
// compiler error; it corrupts the process stack.

#ifndef PW_HUD_ISLAND_H
#define PW_HUD_ISLAND_H

#include <cstdint>
#include <cstring>

namespace pw {

constexpr int kHudIslandSize = 256;

// Island state, on the same page as the code and beyond its end. RIP-relative
// operands that reach it are already resolved in the template because these are
// internal distances that do not depend on where the island is allocated.
constexpr int kHudFlagOffset    = 0x200;
constexpr int kHudPreOffset     = 0x208;
constexpr int kHudPostOffset    = 0x20c;
constexpr int kHudFromOffset    = 0x210;

// Values that must be filled at runtime.
constexpr int kHudFrameStartOffset = 0x0b;  // imm64: address of 0x1C915
constexpr int kHudMarkerOffset     = 0x3e;  // imm64: address of 0x5CFB1
constexpr int kHudXOffset          = 0xe8;  // imm32 for `mov edx, X`
constexpr int kHudWOffset          = 0xee;  // imm32 for `mov r9d, W`
constexpr int kHudReturnOffset     = 0xfc;  // return jump rel32
constexpr int kHudEndOffset        = 0xf2;  // common branch target

// Threshold between "a world must be composited" and "no world is present".
// Measured: gameplay has 741 calls before the marker; menus and cutscenes have 1.
constexpr int kHudWorldThreshold = 64;

// Internal target to which the rectangle is applied. This is not the output
// resolution; confusing the two produces a band with the wrong size.
constexpr int kInternalWidth  = 1920;
constexpr int kInternalHeight = 1088;

// Centered 16:9 band inside the internal target for a given output resolution.
// Returns false when the display is not wider than 16:9.
inline bool hud_band(int output_width, int output_height, int* x, int* width) {
    if (output_width <= 0 || output_height <= 0) return false;
    const double k = (static_cast<double>(output_height) * 16.0 / 9.0) /
                     static_cast<double>(output_width);
    if (k >= 1.0) return false;
    const int w = static_cast<int>(kInternalWidth * k + 0.5);
    *width = w;
    *x = (kInternalWidth - w) / 2;
    return true;
}

inline void build_hud_island(unsigned char* out, std::uint64_t island,
                             std::uint64_t frame_start, std::uint64_t marker,
                             std::uint64_t back, int x, int width) {
    static const unsigned char code[kHudIslandSize] = {
        0x50, 0x41, 0x52, 0x9c, 0x48, 0x8b, 0x44, 0x24, 0x60, 0x49, 0xba, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4c, 0x39, 0xd0, 0x0f, 0x85,
        0x20, 0x00, 0x00, 0x00, 0xc6, 0x05, 0xdd, 0x01, 0x00, 0x00, 0x00, 0xc7,
        0x05, 0xdb, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc7, 0x05, 0xd5,
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe9, 0xb6, 0x00, 0x00, 0x00,
        0x49, 0xba, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4c, 0x39,
        0xd0, 0x0f, 0x85, 0x3f, 0x00, 0x00, 0x00, 0xc6, 0x05, 0xaa, 0x01, 0x00,
        0x00, 0x01, 0xc7, 0x05, 0xac, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x81, 0x3d, 0x9e, 0x01, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x0f, 0x86,
        0x0f, 0x00, 0x00, 0x00, 0xc7, 0x05, 0x96, 0x01, 0x00, 0x00, 0x03, 0x00,
        0x00, 0x00, 0xe9, 0x73, 0x00, 0x00, 0x00, 0xc7, 0x05, 0x87, 0x01, 0x00,
        0x00, 0x01, 0x00, 0x00, 0x00, 0xe9, 0x64, 0x00, 0x00, 0x00, 0x41, 0x81,
        0xf9, 0x00, 0x01, 0x00, 0x00, 0x0f, 0x85, 0x0f, 0x00, 0x00, 0x00, 0xc7,
        0x05, 0x67, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe9, 0x48, 0x00,
        0x00, 0x00, 0x41, 0x81, 0xf9, 0x80, 0x07, 0x00, 0x00, 0x0f, 0x85, 0x3b,
        0x00, 0x00, 0x00, 0x80, 0x3d, 0x42, 0x01, 0x00, 0x00, 0x00, 0x0f, 0x85,
        0x0b, 0x00, 0x00, 0x00, 0xff, 0x05, 0x3e, 0x01, 0x00, 0x00, 0xe9, 0x23,
        0x00, 0x00, 0x00, 0xff, 0x05, 0x37, 0x01, 0x00, 0x00, 0x8b, 0x05, 0x35,
        0x01, 0x00, 0x00, 0x39, 0x05, 0x2b, 0x01, 0x00, 0x00, 0x0f, 0x82, 0x0b,
        0x00, 0x00, 0x00, 0xba, 0x00, 0x00, 0x00, 0x00, 0x41, 0xb9, 0x00, 0x00,
        0x00, 0x00, 0x9d, 0x41, 0x5a, 0x58, 0x0f, 0x57, 0xc0, 0x8b, 0xc2, 0xe9,
        0x00, 0x00, 0x00, 0x00,
    };
    std::memcpy(out, code, sizeof(code));
    std::memcpy(out + kHudFrameStartOffset, &frame_start, sizeof(frame_start));
    std::memcpy(out + kHudMarkerOffset, &marker, sizeof(marker));
    const std::int32_t x32 = x, w32 = width;
    std::memcpy(out + kHudXOffset, &x32, sizeof(x32));
    std::memcpy(out + kHudWOffset, &w32, sizeof(w32));
    const std::int32_t rel = static_cast<std::int32_t>(
        back - (island + kHudReturnOffset + 4));
    std::memcpy(out + kHudReturnOffset, &rel, sizeof(rel));
}

}  // namespace pw

#endif
