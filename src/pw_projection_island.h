// Islote que corrige la matriz de proyeccion dentro de la propia llamada que la
// construye.
//
// Quien escribe la matriz es una sola instruccion, `movaps [rax], xmm0` en RVA
// 0x11A49D. Pertenece a la funcion 0x11A340-0x11A4BC, recta pura -- ni un salto
// interno --, con un unico llamador en 0x8ED15. Sus nueve bytes de epilogo
// (`add rsp,0x100 / pop rbp / ret`) se sustituyen enteros por un salto a este
// islote, que hace la correccion y ejecuta el epilogo original.
//
// Corregir aqui, y no desde un hilo aparte, es lo que quita el parpadeo: no
// queda ninguna ventana en la que se pueda dibujar un fotograma sin corregir.
//
//   mov   r10, <matriz>        cmp rax, r10        jne fin
//   movss xmm0, [rax+0x14]     ; m5, recien escrito por el juego
//   mov   r10d, <1/aspecto>    movd xmm1, r10d     mulss xmm0, xmm1
//   movss [rax], xmm0          ; m0 = m5 / aspecto
//   fin:  add rsp,0x100        pop rbp             ret
//
// Solo usa registros volatiles (r10, xmm0, xmm1) y preserva rax, que es el
// valor de retorno de la funcion. xmm6 y xmm7 ya estan restaurados en 0x11A48D
// y 0x11A495.
//
// El comparador `rax == matriz` es imprescindible: la funcion construye otras
// matrices para otros destinos y solo esta debe tocarse.
//
// Vive en una cabecera aparte para que las pruebas puedan comprobar los bytes
// sin cargar el juego. Un byte mal puesto aqui corrompe la pila del proceso.

#ifndef PW_PROJECTION_ISLAND_H
#define PW_PROJECTION_ISLAND_H

#include <cstdint>
#include <cstring>

namespace pw {

constexpr int kProjectionIslandSize = 48;

constexpr int kProjectionMatrixOffset   = 2;     // imm64 tras `49 ba`
constexpr int kProjectionAspectOffset   = 22;    // imm32 tras `41 ba`
constexpr int kProjectionEpilogueOffset = 0x27;  // destino del jne

inline void build_projection_island(unsigned char* out, std::uint64_t matrix,
                                    float aspect) {
    const unsigned char code[kProjectionIslandSize] = {
        0x49, 0xba, 0, 0, 0, 0, 0, 0, 0, 0,       // 0x00 mov r10, imm64
        0x4c, 0x39, 0xd0,                          // 0x0a cmp rax, r10
        0x75, 0x18,                                // 0x0d jne fin
        0xf3, 0x0f, 0x10, 0x40, 0x14,              // 0x0f movss xmm0,[rax+0x14]
        0x41, 0xba, 0, 0, 0, 0,                    // 0x14 mov r10d, imm32
        0x66, 0x41, 0x0f, 0x6e, 0xca,              // 0x1a movd xmm1, r10d
        0xf3, 0x0f, 0x59, 0xc1,                    // 0x1f mulss xmm0, xmm1
        0xf3, 0x0f, 0x11, 0x00,                    // 0x23 movss [rax], xmm0
        0x48, 0x81, 0xc4, 0x00, 0x01, 0x00, 0x00,  // 0x27 fin: add rsp,0x100
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
