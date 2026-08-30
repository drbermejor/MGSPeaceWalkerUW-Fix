// Islote que mete la interfaz 2D en una banda 16:9 centrada, dejando el mundo a
// pantalla completa.
//
// El juego renderiza el mundo a 1920x1088 -- exactamente 480x4 y 272x4, cuatro
// veces la resolucion de PSP -- en un objetivo aparte, y dibuja la interfaz
// encima, en el mismo objetivo. En todo el binario hay UNA sola llamada a
// RSSetViewports (0x17E44), dentro de una envoltura en 0x17DC0 que recibe
// (x, y, w, h) como enteros. Se desvian cinco bytes de esa envoltura (0x17DD3).
//
// La forma del fotograma, medida en el proceso vivo:
//
//     0x1C915  3440x1440                 <- inicio de fotograma
//     0x1CA1A  3440x1440
//     0x56345  1920x1088   x N           <- el mundo
//     0x5CFB1  1920x1088   x1            <- la marca
//     0x56345   256x256    x ~21         <- sombras, dependen de la escena
//     0x56345  1920x1088   x M           <- el bloque 2D
//     0x5C775  3440x1440                 <- fin de fotograma
//
//     gameplay     antes=741  sombras=21  2D=48
//     menu         antes=  1  sombras=15  2D=118
//     cinematica   antes=  1  sombras= 0  2D=4
//
// Cuando hay mundo, la SEGUNDA llamada del bloque 2D es la que lo compone, y
// bandearla meteria el mundo en la banda: hay que empezar en la tercera. Cuando
// no lo hay -- menus, cinematicas -- no hay nada que saltar, y empezar en la
// tercera dejaba el video fuera de la banda, estirado. Por eso el punto de
// arranque se decide por el numero de llamadas ANTERIORES a la marca, que se
// conoce justo cuando la marca llega: sin heredar nada del fotograma anterior.
//
//     inicio de fotograma:  bandera = 0; antes = 0; despues = 0
//     marca:                bandera = 1; despues = 0
//                           desde = (antes > 64) ? 3 : 1
//     ancho 256:            despues = 0        (las sombras reinician la cuenta:
//                                               hay escenas con una llamada de
//                                               1920 intercalada entre ellas que
//                                               si no desplazaria la numeracion)
//     ancho 1920:  sin bandera -> antes++
//                  con bandera -> despues++;  banda si despues >= desde
//
// Solo usa rax, r10 y las banderas, que salva y restaura.
//
// El codigo maquina vive aqui, aparte del modulo, para que las pruebas lo
// comprueben byte a byte sin cargar el juego: un desplazamiento mal puesto no da
// error de compilacion, corrompe la pila del proceso.

#ifndef PW_HUD_ISLAND_H
#define PW_HUD_ISLAND_H

#include <cstdint>
#include <cstring>

namespace pw {

constexpr int kHudIslandSize = 256;

// Estado del islote, en la misma pagina que el codigo y mas alla de su final.
// Los operandos relativos a RIP que los alcanzan ya vienen resueltos en la
// plantilla, porque son distancias internas y no dependen de donde caiga.
constexpr int kHudFlagOffset    = 0x200;
constexpr int kHudPreOffset     = 0x208;
constexpr int kHudPostOffset    = 0x20c;
constexpr int kHudFromOffset    = 0x210;

// Lo unico que hay que rellenar en ejecucion.
constexpr int kHudFrameStartOffset = 0x0b;  // imm64: direccion de 0x1C915
constexpr int kHudMarkerOffset     = 0x3e;  // imm64: direccion de 0x5CFB1
constexpr int kHudXOffset          = 0xe8;  // imm32 de `mov edx, X`
constexpr int kHudWOffset          = 0xee;  // imm32 de `mov r9d, W`
constexpr int kHudReturnOffset     = 0xfc;  // rel32 del salto de vuelta
constexpr int kHudEndOffset        = 0xf2;  // destino comun de los saltos

// Umbral que separa "hay mundo que componer" de "no lo hay". Medido: gameplay
// 741 llamadas antes de la marca; menus y cinematicas, 1.
constexpr int kHudWorldThreshold = 64;

// El objetivo interno al que se aplica el rectangulo. No es la resolucion de
// salida: confundirlos da una banda del tamano equivocado.
constexpr int kInternalWidth  = 1920;
constexpr int kInternalHeight = 1088;

// Rectangulo de la banda 16:9 dentro del objetivo interno, para una salida dada.
// Devuelve false si la pantalla no es mas ancha que 16:9.
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
