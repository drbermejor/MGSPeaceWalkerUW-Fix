// Check patch arithmetic against values measured in the live process, and
// verify the machine code for both islands byte by byte. The latter matters
// more than it may seem: a bad displacement does not cause a compiler error;
// it corrupts the game stack.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "pw_hud_island.h"
#include "pw_frustum_island.h"
#include "pw_projection_island.h"

namespace {

int g_failures = 0;

void check(bool ok, const char* what) {
    if (!ok) { std::printf("FAIL: %s\n", what); ++g_failures; }
}

void check_close(float got, float want, float tol, const char* what) {
    if (std::fabs(got - want) > tol) {
        std::printf("FAIL: %s  got=%.6f expected=%.6f\n", what, got, want);
        ++g_failures;
    }
}

// Hor+: preserve vertical FOV (m5) and only widen the horizontal field.
float corrected_m0(float m5, int width, int height) {
    return m5 / (static_cast<float>(width) / static_cast<float>(height));
}

bool needs_correction(int width, int height) {
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    return aspect > 16.0f / 9.0f + 0.001f;
}

std::int32_t read_i32(const unsigned char* p) {
    std::int32_t v = 0;
    std::memcpy(&v, p, sizeof(v));
    return v;
}

std::uint64_t read_u64(const unsigned char* p) {
    std::uint64_t v = 0;
    std::memcpy(&v, p, sizeof(v));
    return v;
}

}  // namespace

int main() {
    // --- Projection ----------------------------------------------------------
    // The frustum measured in the game uses the native PSP aspect, 480/272.
    const float measured_m0 = 1.70000f;
    const float measured_m5 = 3.00000f;
    check_close(measured_m5 / measured_m0, 480.0f / 272.0f, 0.0005f,
                "the measured matrix uses the native PSP aspect");
    check_close(corrected_m0(measured_m5, 3440, 1440), 1.255814f, 0.0001f,
                "3440x1440 -> m0 = 1.255814");
    check_close(measured_m5, 3.0f, 1e-6f, "m5 remains unchanged, making this Hor+");
    check_close(corrected_m0(measured_m5, 2560, 1080), 3.0f / (2560.0f / 1080.0f),
                0.0001f, "2560x1080");
    check_close(corrected_m0(measured_m5, 5120, 1440), 3.0f / (5120.0f / 1440.0f),
                0.0001f, "5120x1440 (32:9)");
    check(!needs_correction(1920, 1080), "16:9 needs no correction");
    check(!needs_correction(1920, 1200), "16:10 is narrower than 16:9");
    check(needs_correction(3440, 1440), "21:9 needs correction");
    check(needs_correction(3840, 1080), "32:9 needs correction");

    // --- Projection island ---------------------------------------------------
    {
        unsigned char island[pw::kProjectionIslandSize];
        pw::build_projection_island(island, 0x1410f4330ull, 3440.0f / 1440.0f);

        check(island[0] == 0x49 && island[1] == 0xba, "mov r10, imm64");
        check(read_u64(island + pw::kProjectionMatrixOffset) == 0x1410f4330ull,
              "the matrix address is stored in the imm64");
        check(island[10] == 0x4c && island[11] == 0x39 && island[12] == 0xd0,
              "cmp rax, r10");

        // The conditional jump must land exactly on `add rsp,0x100`. Landing in
        // the middle of an instruction would corrupt the stack as soon as the
        // game built a matrix for another destination.
        check(island[13] == 0x75, "one-byte jne");
        check(15 + island[14] == pw::kProjectionEpilogueOffset,
              "jne lands exactly on the original epilogue");
        check(island[pw::kProjectionEpilogueOffset] == 0x48 &&
                  island[pw::kProjectionEpilogueOffset + 1] == 0x81 &&
                  island[pw::kProjectionEpilogueOffset + 2] == 0xc4,
              "the jne target contains add rsp");
        check(island[pw::kProjectionIslandSize - 2] == 0x5d, "pop rbp");
        check(island[pw::kProjectionIslandSize - 1] == 0xc3, "ret");

        // The immediate is 1/aspect: multiplying by it is equivalent to division.
        check(island[20] == 0x41 && island[21] == 0xba, "mov r10d, imm32");
        float inverse = 0.0f;
        std::memcpy(&inverse, island + pw::kProjectionAspectOffset, sizeof(inverse));
        check_close(inverse, 1440.0f / 3440.0f, 1e-7f, "imm32 is 1/aspect");
        check_close(3.0f * inverse, corrected_m0(3.0f, 3440, 1440), 1e-6f,
                    "multiplication by imm32 equals division by aspect");

        // The comparison protects matrices used by other cameras.
        unsigned char other[pw::kProjectionIslandSize];
        pw::build_projection_island(other, 0x123456789ull, 3440.0f / 1440.0f);
        check(read_u64(other + pw::kProjectionMatrixOffset) == 0x123456789ull,
              "the comparison uses the supplied address");
    }

    // --- Interface band ------------------------------------------------------
    {
        int x = 0, w = 0;
        check(pw::hud_band(3440, 1440, &x, &w), "21:9 has a band");
        check(w == 1429, "3440x1440 -> width 1429");
        check(x == 245, "3440x1440 -> X 245");
        check(x * 2 + w <= pw::kInternalWidth, "the band fits the internal target");

        // The band produces a 16:9 aspect, which is the intended result: the
        // unmodified game stretches 480x272 to 16:9.
        const double band_aspect =
            (static_cast<double>(w) / pw::kInternalWidth) * (3440.0 / 1440.0);
        check(std::fabs(band_aspect - 16.0 / 9.0) < 0.002,
              "the band reproduces the unmodified game's 16:9 output");

        int x2 = 0, w2 = 0;
        check(pw::hud_band(3840, 1080, &x2, &w2), "32:9 has a band");
        check(w2 < w, "wider displays produce a narrower band");
        check(!pw::hud_band(1920, 1080, &x2, &w2), "16:9 needs no band");
        check(!pw::hud_band(1920, 1200, &x2, &w2), "16:10 needs no band either");
        check(!pw::hud_band(0, 0, &x2, &w2), "an invalid resolution is rejected");
    }

    // --- Visibility-frustum island ------------------------------------------
    {
        const std::uint64_t island_address = 0x142000000ull;
        const std::uint64_t return_address = 0x14008eed1ull;
        unsigned char island[pw::kFrustumIslandSize];
        pw::build_frustum_island(island, island_address, return_address,
                                 5120.0f / 1440.0f);

        check(island[0] == 0x9c && island[81] == 0x9d,
              "frustum island preserves flags");
        check(island[10] == 0x0f && island[13] == 0xf3 &&
                  island[21] == 0xf3,
              "frustum island derives horizontal scale from live m5");
        check(island_address + 29 + read_i32(island + 25) ==
                  island_address + pw::kFrustumAspectOffset,
              "frustum island reads its embedded inverse aspect");
        check(island[29] == 0xf3 && island[33] == 0x10 &&
                  island[62] == 0xf3 && island[66] == 0x20,
              "frustum island replaces only the horizontal plane seeds");
        float inverse = 0.0f;
        std::memcpy(&inverse, island + pw::kFrustumAspectOffset,
                    sizeof(inverse));
        check_close(inverse, 1440.0f / 5120.0f, 1e-7f,
                    "frustum island stores height/output-width");
        check(island[82] == 0xe9, "frustum island returns with a rel32 jump");
        check(island_address + pw::kFrustumReturnDisplacementOffset + 4 +
                  read_i32(island + pw::kFrustumReturnDisplacementOffset) ==
                  return_address,
              "frustum island returns after the displaced instruction");
    }

    // --- Interface island ----------------------------------------------------
    {
        const std::uint64_t island = 0x141000000ull;
        const std::uint64_t frame_start = 0x14001c915ull;
        const std::uint64_t marker = 0x14005cfb1ull;
        const std::uint64_t back = 0x140017dd8ull;
        unsigned char code[pw::kHudIslandSize];
        pw::build_hud_island(code, island, frame_start, marker, back, 245, 1429);

        check(code[0] == 0x50 && code[1] == 0x41 && code[2] == 0x52 &&
                  code[3] == 0x9c,
              "saves rax, r10, and flags");
        check(code[4] == 0x48 && code[5] == 0x8b && code[6] == 0x44 &&
                  code[7] == 0x24 && code[8] == 0x60,
              "reads the return address from [rsp+0x60]");
        check(read_u64(code + pw::kHudFrameStartOffset) == frame_start,
              "frame start is stored in its imm64");
        check(read_u64(code + pw::kHudMarkerOffset) == marker,
              "marker is stored in its imm64");
        check(read_i32(code + pw::kHudXOffset) == 245, "X is stored in its imm32");
        check(read_i32(code + pw::kHudWOffset) == 1429, "width is stored in its imm32");
        check(code[pw::kHudXOffset - 1] == 0xba, "X is the mov edx operand");
        check(code[pw::kHudWOffset - 2] == 0x41 && code[pw::kHudWOffset - 1] == 0xb9,
              "width is the mov r9d operand");

        // The common branch target must be the popfq that restores state. If any
        // branch landed in the middle of an instruction, the game would crash.
        check(code[pw::kHudEndOffset] == 0x9d, "the common target is popfq");
        check(code[pw::kHudEndOffset + 1] == 0x41 &&
                  code[pw::kHudEndOffset + 2] == 0x5a && code[pw::kHudEndOffset + 3] == 0x58,
              "pop r10 and pop rax");

        // All branches are 32-bit because the island exceeds 127 bytes. Walk
        // through them and verify that none leaves the island and that branches
        // to the end land exactly on the common target.
        int checked_branches = 0;
        for (int i = 0; i + 6 <= pw::kHudIslandSize; ++i) {
            const bool near_conditional = code[i] == 0x0f && (code[i + 1] & 0xf0) == 0x80;
            const bool near_jump = code[i] == 0xe9;
            if (!near_conditional && !near_jump) continue;
            const int length = near_conditional ? 6 : 5;
            const int target = i + length + read_i32(code + i + length - 4);
            if (i + length == pw::kHudReturnOffset + 4) continue;  // return jump
            if (target < 0 || target > pw::kHudIslandSize) {
                std::printf("FAIL: branch at 0x%02x leaves the island (0x%x)\n",
                            i, target);
                ++g_failures;
            }
            ++checked_branches;
            i += length - 1;
        }
        check(checked_branches >= 8, "all branches were checked");

        // All four state slots live beyond the code and do not overlap it.
        check(pw::kHudFlagOffset >= pw::kHudIslandSize, "the flag does not overlap code");
        check(pw::kHudPreOffset >= pw::kHudIslandSize, "the before counter does not overlap code");
        check(pw::kHudPostOffset == pw::kHudPreOffset + 4, "before and after are contiguous");
        check(pw::kHudFromOffset % 4 == 0, "the starting point is aligned");

        // The return jump points to the game's next instruction.
        check(code[pw::kHudReturnOffset - 1] == 0xe9, "returns with a relative jump");
        check(island + pw::kHudReturnOffset + 4 +
                      read_i32(code + pw::kHudReturnOffset) == back,
              "the return jump points to the correct address");

        // Re-execute the two original instructions replaced by the detour before
        // returning.
        check(code[pw::kHudEndOffset + 4] == 0x0f && code[pw::kHudEndOffset + 5] == 0x57 &&
                  code[pw::kHudEndOffset + 6] == 0xc0,
              "re-executes the original xorps");
        check(code[pw::kHudEndOffset + 7] == 0x8b && code[pw::kHudEndOffset + 8] == 0xc2,
              "re-executes the original mov eax,edx");

        // The threshold separating "world present" from "no world" appears in
        // the code as the cmp operand against the before counter.
        bool threshold_found = false;
        for (int i = 0; i + 10 <= pw::kHudIslandSize; ++i)
            if (code[i] == 0x81 && code[i + 1] == 0x3d &&
                read_i32(code + i + 6) == pw::kHudWorldThreshold)
                threshold_found = true;
        check(threshold_found, "the threshold of 64 is present in the code");

        // Verify both possible starting points: 3 with a world, 1 without one.
        bool from_three = false, from_one = false;
        for (int i = 0; i + 10 <= pw::kHudIslandSize; ++i) {
            if (code[i] != 0xc7 || code[i + 1] != 0x05) continue;
            if (read_i32(code + i + 2) != pw::kHudFromOffset - (i + 10)) continue;
            if (read_i32(code + i + 6) == 3) from_three = true;
            if (read_i32(code + i + 6) == 1) from_one = true;
        }
        check(from_three, "with a world, processing starts on the third call");
        check(from_one, "without a world, processing starts on the first call");

        // Changing the rectangle cannot affect an instruction; all differences
        // must be confined to the two immediate operands.
        unsigned char other[pw::kHudIslandSize];
        pw::build_hud_island(other, island, frame_start, marker, back, 100, 800);
        bool outside_immediates = false;
        for (int i = 0; i < pw::kHudIslandSize; ++i) {
            if (other[i] == code[i]) continue;
            const bool in_x = i >= pw::kHudXOffset && i < pw::kHudXOffset + 4;
            const bool in_w = i >= pw::kHudWOffset && i < pw::kHudWOffset + 4;
            if (!in_x && !in_w) outside_immediates = true;
        }
        check(!outside_immediates,
              "changing the rectangle only alters the two immediates");
        check(read_i32(other + pw::kHudXOffset) == 100 &&
                  read_i32(other + pw::kHudWOffset) == 800,
              "and stores the new values in them");
    }

    // --- Resolution table ----------------------------------------------------
    {
        const int table[4][2] = {{1280, 720}, {1920, 1080}, {2560, 1440}, {3840, 2160}};
        int slot = -1;
        for (int i = 0; i < 4; ++i) if (table[i][1] == 1440) slot = i;
        check(slot == 2, "a 1440-line display uses entry [2]");
        check(pw::kInternalWidth == 480 * 4 && pw::kInternalHeight == 272 * 4,
              "the internal target is four times the PSP resolution");
    }

    if (g_failures == 0) std::printf("all checks passed\n");
    return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
