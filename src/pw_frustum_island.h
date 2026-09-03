#pragma once

#include <cstdint>
#include <cstring>

namespace pw {

// Replaces the 39-byte block that seeds the four side planes. The original
// camera stores:
//   +0x250 = height/width
//   +0x254 = native horizontal projection scale (m0)
// Therefore m5 = camera[+0x254] / camera[+0x250]. Only the left/right plane
// coefficients should become m5/output_aspect; the top/bottom coefficients
// must retain the original values so vertical FOV remains unchanged.
inline constexpr int kFrustumIslandSize = 91;
inline constexpr int kFrustumAspectOffset = 87;
inline constexpr int kFrustumReturnDisplacementOffset = 83;

inline void build_frustum_island(unsigned char* out,
                                 std::uint64_t island_address,
                                 std::uint64_t return_address,
                                 float aspect) {
    const unsigned char code[kFrustumIslandSize] = {
        0x9c,                                            // 000 pushfq
        0x48, 0x83, 0xec, 0x10,                          // 001 sub rsp,10
        0xf3, 0x0f, 0x7f, 0x14, 0x24,                    // 005 save xmm2
        0x0f, 0x28, 0xd0,                                // 010 movaps xmm2,xmm0
        0xf3, 0x0f, 0x5e, 0x93, 0x50, 0x02, 0x00, 0x00,  // 013 divss xmm2,[rbx+250]
        0xf3, 0x0f, 0x59, 0x15, 0x3a, 0x00, 0x00, 0x00,  // 021 mulss xmm2,[aspect]
        0xf3, 0x0f, 0x11, 0x57, 0x10,                    // 029 movss [rdi+10],xmm2
        0x0f, 0x28, 0xc8,                                // 034 movaps xmm1,xmm0
        0xf3, 0x0f, 0x11, 0x47, 0x34,                    // 037 movss [rdi+34],xmm0
        0xf3, 0x0f, 0x10, 0x83, 0x50, 0x02, 0x00, 0x00,  // 042 movss xmm0,[rbx+250]
        0x41, 0x0f, 0x57, 0xcb,                          // 050 xorps xmm1,xmm11
        0x41, 0x0f, 0x57, 0xc3,                          // 054 xorps xmm0,xmm11
        0x41, 0x0f, 0x57, 0xd3,                          // 058 xorps xmm2,xmm11
        0xf3, 0x0f, 0x11, 0x57, 0x20,                    // 062 movss [rdi+20],xmm2
        0xf3, 0x0f, 0x11, 0x47, 0x38,                    // 067 movss [rdi+38],xmm0
        0xf3, 0x0f, 0x6f, 0x14, 0x24,                    // 072 restore xmm2
        0x48, 0x83, 0xc4, 0x10,                          // 077 add rsp,10
        0x9d,                                            // 081 popfq
        0xe9, 0, 0, 0, 0,                                // 082 jmp return
        0, 0, 0, 0,                                      // 087 1/output_aspect
    };
    std::memcpy(out, code, sizeof(code));
    const float inverse = 1.0f / aspect;
    std::memcpy(out + kFrustumAspectOffset, &inverse, sizeof(inverse));

    const auto next = static_cast<std::int64_t>(island_address) +
        kFrustumReturnDisplacementOffset + sizeof(std::int32_t);
    const auto displacement = static_cast<std::int32_t>(
        static_cast<std::int64_t>(return_address) - next);
    std::memcpy(out + kFrustumReturnDisplacementOffset, &displacement,
                sizeof(displacement));
}

}  // namespace pw
