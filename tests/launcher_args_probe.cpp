#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdio>

int wmain() {
    wchar_t current[MAX_PATH]{};
    wchar_t app_id[64]{};
    GetCurrentDirectoryW(MAX_PATH, current);
    GetEnvironmentVariableW(L"SteamAppId", app_id,
                            static_cast<DWORD>(sizeof(app_id) / sizeof(app_id[0])));
    FILE* output = nullptr;
    if (_wfopen_s(&output, L"launcher-args.txt", L"w, ccs=UTF-8") != 0 || !output)
        return 2;
    fwprintf(output, L"command=%ls\nworking=%ls\nSteamAppId=%ls\n",
             GetCommandLineW(), current, app_id);
    std::fclose(output);
    return 0;
}
