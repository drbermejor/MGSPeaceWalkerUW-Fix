// Peace Walker UltraWide Fix.
//
// El juego renderiza el mundo a 1920x1088 (cuatro veces la resolucion de PSP)
// en un objetivo aparte y despues lo compone contra la salida, encajado en 16:9
// con barras negras. Tres cosas, independientes y todas desactivables:
//
//   1. La tabla de resoluciones en .rdata define el tamano compuesto. Con una
//      entrada de la anchura real desaparecen las barras.
//   2. La matriz de proyeccion es un global estatico y el port nunca corrigio
//      su aspecto: sin tocarla, ampliar la salida solo estira la imagen. Se
//      corrige DENTRO de la funcion que la construye, no desde un hilo, porque
//      el juego la reconstruye una vez por fotograma y competir con el produce
//      parpadeo.
//   3. La interfaz 2D se dibuja en el mismo objetivo interno que el mundo. Para
//      dejarla en 16:9 mientras el mundo ocupa toda la pantalla se le cambia el
//      viewport a partir de la llamada en la que empieza el 2D, que se decide
//      por fotograma segun haya mundo que componer o no.
//
// Todas las direcciones estan medidas sobre el ejecutable de Master Collection
// Vol.1, y la cabecera PE se comprueba antes de tocar nada.

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

// Ejecutable soportado: METAL GEAR SOLID PEACE WALKER.exe de Master Collection
// Vol.1 (Steam AppID 2492660), empaquetado o no: la cabecera es la misma.
constexpr DWORD kTimeDateStamp = 0x6a6cc50e;
constexpr DWORD kSizeOfImage   = 0x0199e000;

// Tabla de resoluciones. Cuatro entradas 16:9 de 8 bytes: (ancho, alto).
//   [0] 1280x720   [1] 1920x1080   [2] 2560x1440   [3] 3840x2160
constexpr std::uintptr_t kResolutionTableRva = 0xd8f1b8;
constexpr int kResolutionSlots = 4;

// Matriz de proyeccion, 4x4 de flotantes. Firma de perspectiva de PSP:
// m11 = -1.0, m15 = 0.
constexpr std::uintptr_t kProjectionRva = 0x10f4330;
constexpr std::uintptr_t kProjM0 = 0x00, kProjM5 = 0x14;
constexpr std::uintptr_t kProjM11 = 0x2c, kProjM15 = 0x3c;

// Epilogo de la funcion que construye la matriz, y la instruccion que la
// escribe. Las dos se comprueban antes de parchear.
constexpr std::uintptr_t kProjEpilogueRva = 0x11a4b4;
constexpr std::uintptr_t kProjStoreRva    = 0x11a49d;
const unsigned char kProjEpilogueBytes[] = {0x48, 0x81, 0xc4, 0x00, 0x01,
                                            0x00, 0x00, 0x5d, 0xc3};
const unsigned char kProjStoreBytes[] = {0x0f, 0x29, 0x00};

// Envoltura del viewport: el punto de desvio, la vuelta y la marca de fotograma.
constexpr std::uintptr_t kViewportHookRva   = 0x17dd3;
constexpr std::uintptr_t kViewportReturnRva = 0x17dd8;
constexpr std::uintptr_t kFrameMarkerRva    = 0x5cfb1;
const unsigned char kViewportHookBytes[] = {0x0f, 0x57, 0xc0, 0x8b, 0xc2};

// Inicio de fotograma: el islote lo usa para reiniciar su estado.
constexpr std::uintptr_t kFrameStartRva = 0x1c915;

// Contexto de display. +0x2970 es el indice dentro de la tabla.
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

// Resolucion fisica del monitor principal, en pixeles reales: el proceso no es
// consciente del DPI, asi que GetSystemMetrics mentiria con escalado activo.
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

// Reserva una pagina ejecutable a tiro de salto relativo (+-2 GB) del sitio.
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

// Escribe un salto relativo de cinco bytes, rellenando el resto con int3.
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

// SteamStub cifra la seccion de codigo y la descifra en su punto de entrada,
// que corre despues de que el cargador resuelva las importaciones -- es decir,
// despues de este modulo. Hay que esperar a que las firmas aparezcan. La tabla
// de resoluciones vive en .rdata, que no esta cifrada, y por eso si se lee al
// instante.
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
            if (waited) log_line("Codigo descifrado tras %u ms.", waited);
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
        log_line("Proyeccion: no se pudo reservar memoria para el islote.");
        return false;
    }
    unsigned char code[pw::kProjectionIslandSize];
    pw::build_projection_island(
        code, static_cast<std::uint64_t>(g_base + kProjectionRva), aspect);
    std::memcpy(island, code, sizeof(code));
    if (!write_jump(g_base + kProjEpilogueRva, sizeof(kProjEpilogueBytes), island)) {
        log_line("Proyeccion: no se pudo escribir el salto.");
        return false;
    }
    log_line("Proyeccion corregida en origen (0x%llx).",
             static_cast<unsigned long long>(kProjEpilogueRva));
    return true;
}

bool install_hud_hook(int x, int width) {
    void* island = allocate_island_near(g_base + kViewportHookRva);
    if (!island) {
        log_line("HUD: no se pudo reservar memoria para el islote.");
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
        log_line("HUD: no se pudo escribir el salto.");
        return false;
    }
    log_line("Interfaz en banda 16:9: viewport interno X=%d ancho=%d de %d.",
             x, width, pw::kInternalWidth);
    return true;
}

// Reafirma la tabla de resoluciones y, si el enganche de la proyeccion no
// llegara a instalarse, la mantiene desde aqui. En ese caso hay parpadeo, y se
// avisa en el log.
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
                    log_line("Tabla de resoluciones: entrada [%d] = %dx%d.", slot,
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
        log_line("Enabled=0: no se aplica nada.");
        return 0;
    }
    g_fix_table = setting(L"RemoveLetterboxing", L"FixLetterbox", 1) != 0;
    g_fix_projection = setting(L"CorrectFOV", L"FixProjection", 1) != 0;
    g_fix_hud = setting(L"CenterHUD", L"KeepHudAt16by9", 1) != 0;

    g_base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if (!supported_executable()) {
        log_line("ERROR: ejecutable no reconocido; no se aplica nada.");
        return 0;
    }

    physical_desktop_size(&g_width, &g_height);
    const int cfg_w = setting(L"Width", L"Width", 0);
    const int cfg_h = setting(L"Height", L"Height", 0);
    if (cfg_w > 0 && cfg_h > 0) { g_width = cfg_w; g_height = cfg_h; }
    if (g_width <= 0 || g_height <= 0) {
        log_line("ERROR: no se pudo determinar la resolucion de pantalla.");
        return 0;
    }

    const float aspect = static_cast<float>(g_width) / static_cast<float>(g_height);
    log_line("Salida %dx%d (aspecto %.5f). Nativo 16:9 = 1.77778.", g_width, g_height,
             aspect);
    if (aspect <= 16.0f / 9.0f + 0.001f) {
        log_line("La pantalla no es mas ancha que 16:9: no hay nada que corregir.");
        return 0;
    }

    // El hilo arranca antes de esperar al descifrado: si la tabla se parchea
    // tarde, el juego ya ha calculado el encuadre y la imagen sale desplazada.
    CloseHandle(CreateThread(nullptr, 0, maintain, nullptr, 0, nullptr));

    if (!wait_for_code(30000)) {
        log_line("AVISO: el codigo no coincide con las firmas conocidas. No se "
                 "engancha nada; solo queda la tabla de resoluciones.");
        return 0;
    }

    if (g_fix_projection) {
        g_projection_hooked = install_projection_hook(aspect);
        if (!g_projection_hooked)
            log_line("AVISO: sin enganche, la proyeccion se mantiene desde el hilo "
                     "y eso produce parpadeo.");
    }

    if (g_fix_hud) {
        int x = 0, width = 0;
        if (pw::hud_band(g_width, g_height, &x, &width))
            install_hud_hook(x, width);
    } else {
        log_line("CenterHUD=0: la interfaz conserva el comportamiento panoramico del juego.");
    }
    return 0;
}

}  // namespace

// El .def generado exporta estos dos con nombre propio; aqui solo reenvian al
// winmm del sistema, igual que el resto.
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
