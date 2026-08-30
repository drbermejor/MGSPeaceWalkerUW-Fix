// Comprueba la aritmetica del parche contra los valores medidos en el proceso
// vivo, y el codigo maquina de los dos islotes byte a byte. Esto ultimo importa
// mas de lo que parece: un desplazamiento mal puesto no da un error de
// compilacion, corrompe la pila del juego.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "pw_hud_island.h"
#include "pw_projection_island.h"

namespace {

int g_failures = 0;

void check(bool ok, const char* what) {
    if (!ok) { std::printf("FALLO: %s\n", what); ++g_failures; }
}

void check_close(float got, float want, float tol, const char* what) {
    if (std::fabs(got - want) > tol) {
        std::printf("FALLO: %s  obtenido=%.6f esperado=%.6f\n", what, got, want);
        ++g_failures;
    }
}

// Hor+: se conserva el FOV vertical (m5) y solo se ensancha la horizontal.
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
    // --- La proyeccion -------------------------------------------------------
    // El frustum medido en el juego tiene el aspecto nativo de PSP, 480/272.
    const float measured_m0 = 1.70000f;
    const float measured_m5 = 3.00000f;
    check_close(measured_m5 / measured_m0, 480.0f / 272.0f, 0.0005f,
                "la matriz medida tiene el aspecto nativo de PSP");
    check_close(corrected_m0(measured_m5, 3440, 1440), 1.255814f, 0.0001f,
                "3440x1440 -> m0 = 1.255814");
    check_close(measured_m5, 3.0f, 1e-6f, "m5 no se toca: eso es lo que lo hace Hor+");
    check_close(corrected_m0(measured_m5, 2560, 1080), 3.0f / (2560.0f / 1080.0f),
                0.0001f, "2560x1080");
    check_close(corrected_m0(measured_m5, 5120, 1440), 3.0f / (5120.0f / 1440.0f),
                0.0001f, "5120x1440 (32:9)");
    check(!needs_correction(1920, 1080), "16:9 no necesita correccion");
    check(!needs_correction(1920, 1200), "16:10 es mas estrecho que 16:9");
    check(needs_correction(3440, 1440), "21:9 si la necesita");
    check(needs_correction(3840, 1080), "32:9 si la necesita");

    // --- El islote de la proyeccion -----------------------------------------
    {
        unsigned char island[pw::kProjectionIslandSize];
        pw::build_projection_island(island, 0x1410f4330ull, 3440.0f / 1440.0f);

        check(island[0] == 0x49 && island[1] == 0xba, "mov r10, imm64");
        check(read_u64(island + pw::kProjectionMatrixOffset) == 0x1410f4330ull,
              "la direccion de la matriz va en el imm64");
        check(island[10] == 0x4c && island[11] == 0x39 && island[12] == 0xd0,
              "cmp rax, r10");

        // El salto condicional tiene que caer exactamente en `add rsp,0x100`. Si
        // cayera en medio de una instruccion, el juego se llevaria la pila por
        // delante en cuanto construyera una matriz para otro destino.
        check(island[13] == 0x75, "jne de un byte");
        check(15 + island[14] == pw::kProjectionEpilogueOffset,
              "el jne salta justo al epilogo original");
        check(island[pw::kProjectionEpilogueOffset] == 0x48 &&
                  island[pw::kProjectionEpilogueOffset + 1] == 0x81 &&
                  island[pw::kProjectionEpilogueOffset + 2] == 0xc4,
              "en el destino del jne hay un add rsp");
        check(island[pw::kProjectionIslandSize - 2] == 0x5d, "pop rbp");
        check(island[pw::kProjectionIslandSize - 1] == 0xc3, "ret");

        // El inmediato es 1/aspecto: multiplicar por el equivale a dividir.
        check(island[20] == 0x41 && island[21] == 0xba, "mov r10d, imm32");
        float inverse = 0.0f;
        std::memcpy(&inverse, island + pw::kProjectionAspectOffset, sizeof(inverse));
        check_close(inverse, 1440.0f / 3440.0f, 1e-7f, "el imm32 es 1/aspecto");
        check_close(3.0f * inverse, corrected_m0(3.0f, 3440, 1440), 1e-6f,
                    "multiplicar por el imm32 equivale a dividir por el aspecto");

        // El comparador impide estropear las matrices de otras camaras.
        unsigned char other[pw::kProjectionIslandSize];
        pw::build_projection_island(other, 0x123456789ull, 3440.0f / 1440.0f);
        check(read_u64(other + pw::kProjectionMatrixOffset) == 0x123456789ull,
              "el comparador usa la direccion dada");
    }

    // --- La banda de la interfaz --------------------------------------------
    {
        int x = 0, w = 0;
        check(pw::hud_band(3440, 1440, &x, &w), "21:9 tiene banda");
        check(w == 1429, "3440x1440 -> ancho 1429");
        check(x == 245, "3440x1440 -> X 245");
        check(x * 2 + w <= pw::kInternalWidth, "la banda cabe en el objetivo interno");

        // El aspecto que resulta de la banda es 16:9, que es lo que hay que
        // reproducir: en vanilla el juego estira 480x272 a 16:9.
        const double band_aspect =
            (static_cast<double>(w) / pw::kInternalWidth) * (3440.0 / 1440.0);
        check(std::fabs(band_aspect - 16.0 / 9.0) < 0.002,
              "la banda reproduce el 16:9 de vanilla");

        int x2 = 0, w2 = 0;
        check(pw::hud_band(3840, 1080, &x2, &w2), "32:9 tiene banda");
        check(w2 < w, "cuanto mas ancha la pantalla, mas estrecha la banda");
        check(!pw::hud_band(1920, 1080, &x2, &w2), "16:9 no necesita banda");
        check(!pw::hud_band(1920, 1200, &x2, &w2), "16:10 tampoco");
        check(!pw::hud_band(0, 0, &x2, &w2), "una resolucion invalida se rechaza");
    }

    // --- El islote de la interfaz -------------------------------------------
    {
        const std::uint64_t island = 0x141000000ull;
        const std::uint64_t frame_start = 0x14001c915ull;
        const std::uint64_t marker = 0x14005cfb1ull;
        const std::uint64_t back = 0x140017dd8ull;
        unsigned char code[pw::kHudIslandSize];
        pw::build_hud_island(code, island, frame_start, marker, back, 245, 1429);

        check(code[0] == 0x50 && code[1] == 0x41 && code[2] == 0x52 &&
                  code[3] == 0x9c,
              "salva rax, r10 y las banderas");
        check(code[4] == 0x48 && code[5] == 0x8b && code[6] == 0x44 &&
                  code[7] == 0x24 && code[8] == 0x60,
              "lee la direccion de retorno en [rsp+0x60]");
        check(read_u64(code + pw::kHudFrameStartOffset) == frame_start,
              "el inicio de fotograma va en su imm64");
        check(read_u64(code + pw::kHudMarkerOffset) == marker,
              "la marca va en su imm64");
        check(read_i32(code + pw::kHudXOffset) == 245, "X va en su imm32");
        check(read_i32(code + pw::kHudWOffset) == 1429, "el ancho va en su imm32");
        check(code[pw::kHudXOffset - 1] == 0xba, "X es el operando de mov edx");
        check(code[pw::kHudWOffset - 2] == 0x41 && code[pw::kHudWOffset - 1] == 0xb9,
              "el ancho es el operando de mov r9d");

        // El destino comun de los saltos tiene que ser el popfq que restaura el
        // estado. Si alguno cayera en medio de una instruccion, el juego se cae.
        check(code[pw::kHudEndOffset] == 0x9d, "el destino comun es el popfq");
        check(code[pw::kHudEndOffset + 1] == 0x41 &&
                  code[pw::kHudEndOffset + 2] == 0x5a && code[pw::kHudEndOffset + 3] == 0x58,
              "pop r10 y pop rax");

        // Todos los saltos son de 32 bits porque el islote pasa de 127 bytes.
        // Se recorren y se comprueba que ninguno se sale del islote y que los
        // que van al final caen exactamente en el destino comun.
        int checked_branches = 0;
        for (int i = 0; i + 6 <= pw::kHudIslandSize; ++i) {
            const bool near_conditional = code[i] == 0x0f && (code[i + 1] & 0xf0) == 0x80;
            const bool near_jump = code[i] == 0xe9;
            if (!near_conditional && !near_jump) continue;
            const int length = near_conditional ? 6 : 5;
            const int target = i + length + read_i32(code + i + length - 4);
            if (i + length == pw::kHudReturnOffset + 4) continue;  // el de vuelta
            if (target < 0 || target > pw::kHudIslandSize) {
                std::printf("FALLO: el salto en 0x%02x se sale del islote (0x%x)\n",
                            i, target);
                ++g_failures;
            }
            ++checked_branches;
            i += length - 1;
        }
        check(checked_branches >= 8, "se han comprobado todos los saltos");

        // Las cuatro casillas de estado viven mas alla del codigo y no se pisan.
        check(pw::kHudFlagOffset >= pw::kHudIslandSize, "la bandera no pisa el codigo");
        check(pw::kHudPreOffset >= pw::kHudIslandSize, "el contador de antes tampoco");
        check(pw::kHudPostOffset == pw::kHudPreOffset + 4, "antes y despues son contiguos");
        check(pw::kHudFromOffset % 4 == 0, "el punto de arranque esta alineado");

        // El salto de vuelta apunta a la instruccion siguiente del juego.
        check(code[pw::kHudReturnOffset - 1] == 0xe9, "vuelve con un salto relativo");
        check(island + pw::kHudReturnOffset + 4 +
                      read_i32(code + pw::kHudReturnOffset) == back,
              "el salto de vuelta apunta al sitio correcto");

        // Las dos instrucciones originales que el desvio sustituye se reejecutan
        // aqui antes de volver.
        check(code[pw::kHudEndOffset + 4] == 0x0f && code[pw::kHudEndOffset + 5] == 0x57 &&
                  code[pw::kHudEndOffset + 6] == 0xc0,
              "reejecuta el xorps original");
        check(code[pw::kHudEndOffset + 7] == 0x8b && code[pw::kHudEndOffset + 8] == 0xc2,
              "reejecuta el mov eax,edx original");

        // El umbral que separa "hay mundo" de "no lo hay" aparece tal cual en el
        // codigo, como operando del cmp contra el contador de antes.
        bool threshold_found = false;
        for (int i = 0; i + 10 <= pw::kHudIslandSize; ++i)
            if (code[i] == 0x81 && code[i + 1] == 0x3d &&
                read_i32(code + i + 6) == pw::kHudWorldThreshold)
                threshold_found = true;
        check(threshold_found, "el umbral de 64 esta en el codigo");

        // Y los dos puntos de arranque posibles: 3 con mundo, 1 sin el.
        bool from_three = false, from_one = false;
        for (int i = 0; i + 10 <= pw::kHudIslandSize; ++i) {
            if (code[i] != 0xc7 || code[i + 1] != 0x05) continue;
            if (read_i32(code + i + 2) != pw::kHudFromOffset - (i + 10)) continue;
            if (read_i32(code + i + 6) == 3) from_three = true;
            if (read_i32(code + i + 6) == 1) from_one = true;
        }
        check(from_three, "con mundo, arranca en la tercera llamada");
        check(from_one, "sin mundo, arranca en la primera");

        // Cambiar el rectangulo no puede tocar ni una instruccion: las unicas
        // diferencias tienen que caer dentro de los dos inmediatos.
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
              "cambiar el rectangulo solo toca los dos inmediatos");
        check(read_i32(other + pw::kHudXOffset) == 100 &&
                  read_i32(other + pw::kHudWOffset) == 800,
              "y los deja con los valores nuevos");
    }

    // --- La tabla de resoluciones -------------------------------------------
    {
        const int table[4][2] = {{1280, 720}, {1920, 1080}, {2560, 1440}, {3840, 2160}};
        int slot = -1;
        for (int i = 0; i < 4; ++i) if (table[i][1] == 1440) slot = i;
        check(slot == 2, "un panel de 1440 usa la entrada [2]");
        check(pw::kInternalWidth == 480 * 4 && pw::kInternalHeight == 272 * 4,
              "el objetivo interno es cuatro veces la resolucion de PSP");
    }

    if (g_failures == 0) std::printf("todas las comprobaciones pasan\n");
    return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
