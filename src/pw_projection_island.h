// Code island that corrects the projection matrix inside the call that builds it.
//
// A single instruction writes the matrix: `movaps [rax], xmm0` at RVA 0x11A49D.
// It belongs to the straight-line function at 0x11A340-0x11A4BC, with no
// internal branches and a single caller at 0x8ED15. Its nine-byte epilogue
// (`add rsp,0x100 / pop rbp / ret`) is replaced in full by a jump to this island,
// which applies the correction and executes the original epilogue.
//
// Correcting here instead of from another thread removes flicker: there is no
// window in which an uncorrected frame can be drawn.
//
//   mov   r10, <matrix>        cmp rax, r10        jne done
//   movss xmm0, [rax+0x14]     ; m5, just written by the game
//   mov   r10d, <1/aspect>     movd xmm1, r10d     mulss xmm0, xmm1
//   movss [rax], xmm0          ; m0 = m5 / aspect
//   done: add rsp,0x100        pop rbp             ret
//
// It only uses volatile registers (r10, xmm0, xmm1) and preserves rax, which is
// the function return value. xmm6 and xmm7 are already restored at 0x11A48D and
// 0x11A495.
//
// The `rax == matrix` comparison is essential: the function builds other
// matrices for other destinations, and only this one must be changed.
//
// This lives in a separate header so tests can verify every byte without loading
// the game. One misplaced byte here corrupts the process stack.

#ifndef PW_PROJECTION_ISLAND_H
#define PW_PROJECTION_ISLAND_H

#include <cstdint>
#include <cstring>

namespace pw {

constexpr int kProjectionIslandSize = 48;

constexpr int kProjectionMatrixOffset   = 2;     // imm64 after `49 ba`
constexpr int kProjectionAspectOffset   = 22;    // imm32 after `41 ba`
constexpr int kProjectionEpilogueOffset = 0x27;  // jne target

inline void build_projection_island(unsigned char* out, std::uint64_t matrix,
                                    float aspect) {
    const unsigned char code[kProjectionIslandSize] = {
        0x49, 0xba, 0, 0, 0, 0, 0, 0, 0, 0,       // 0x00 mov r10, imm64
        0x4c, 0x39, 0xd0,                          // 0x0a cmp rax, r10
        0x75, 0x18,                                // 0x0d jne done
        0xf3, 0x0f, 0x10, 0x40, 0x14,              // 0x0f movss xmm0,[rax+0x14]
        0x41, 0xba, 0, 0, 0, 0,                    // 0x14 mov r10d, imm32
        0x66, 0x41, 0x0f, 0x6e, 0xca,              // 0x1a movd xmm1, r10d
        0xf3, 0x0f, 0x59, 0xc1,                    // 0x1f mulss xmm0, xmm1
        0xf3, 0x0f, 0x11, 0x00,                    // 0x23 movss [rax], xmm0
        0x48, 0x81, 0xc4, 0x00, 0x01, 0x00, 0x00,  // 0x27 done: add rsp,0x100
        0x5d,                                      // 0x2e pop rbp
        0xc3,                                      // 0x2f ret
    };
    std::memcpy(out, code, sizeof(code));
    std::memcpy(out + kProjectionMatrixOffset, &matrix, sizeof(matrix));
    const float inverse = 1.0f / aspect;
    std::uint32_t bits = 0;
    std::memcpy(&bits, &inverse, sizeof(bits));
    std::memcpy(out + kProjectionAspectOffset, &bits, sizeof(bits));
}

}  // namespace pw

#endif
