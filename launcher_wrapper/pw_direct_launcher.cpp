#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <iterator>
#include <string>
#include <vector>

#ifndef PWUWFIX_VERSION
#define PWUWFIX_VERSION "development"
#endif
#define PWUWFIX_WIDEN_INNER(value) L##value
#define PWUWFIX_WIDEN(value) PWUWFIX_WIDEN_INNER(value)

namespace {

constexpr wchar_t kMarker[] = L"PWUWFIX_DIRECT_LAUNCHER_ACTIVE";
constexpr wchar_t kTitle[] =
    L"Peace Walker UltraWide Fix " PWUWFIX_WIDEN(PWUWFIX_VERSION);

std::wstring parent(const std::wstring& path) {
    const auto slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring() : path.substr(0, slash);
}

void error(const wchar_t* message, DWORD code = 0) {
    std::wstring text(message);
    if (code) text += L"\n\nWindows error: " + std::to_wstring(code);
    MessageBoxW(nullptr, text.c_str(), kTitle, MB_OK | MB_ICONERROR);
}

bool safe_token(const std::wstring& token) {
    if (token.empty() || token.size() > 16) return false;
    for (wchar_t c : token) {
        if (!((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
              (c >= L'0' && c <= L'9') || c == L'_' || c == L'-')) return false;
    }
    return true;
}

std::wstring setting(const std::wstring& ini, const wchar_t* key,
                     const wchar_t* fallback) {
    wchar_t value[32]{};
    GetPrivateProfileStringW(L"Launcher", key, fallback, value,
                             static_cast<DWORD>(std::size(value)), ini.c_str());
    return safe_token(value) ? std::wstring(value) : std::wstring(fallback);
}

void log(const std::wstring& directory, const std::string& text) {
    const std::wstring path = directory + L"\\PeaceWalkerUltraWideFix-launcher.log";
    HANDLE file = CreateFileW(path.c_str(), FILE_APPEND_DATA,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                              OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
    WriteFile(file, "\r\n", 2, &written, nullptr);
    CloseHandle(file);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, wchar_t*, int) {
    if (GetEnvironmentVariableW(kMarker, nullptr, 0) != 0) return 0;

    wchar_t module[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, module, MAX_PATH);
    if (!length || length >= MAX_PATH) {
        error(L"Could not determine the launcher path.", GetLastError());
        return 1;
    }

    const std::wstring launcher_directory = parent(module);
    const std::wstring install_directory = parent(launcher_directory);
    const std::wstring game_directory = install_directory + L"\\mgspw";
    const std::wstring executable =
        game_directory + L"\\METAL GEAR SOLID PEACE WALKER.exe";
    const std::wstring ini = game_directory + L"\\PeaceWalkerUltraWideFix.ini";
    if (GetFileAttributesW(executable.c_str()) == INVALID_FILE_ATTRIBUTES) {
        error(L"The mgspw game executable was not found next to the launcher folder.");
        return 2;
    }

    const std::wstring region = setting(ini, L"Region", L"eu");
    const std::wstring language = setting(ini, L"Language", L"en");
    const std::wstring self_region = setting(ini, L"SelfRegion", L"EU");
    const std::wstring controller = setting(ini, L"ControllerType", L"XBOX");
    std::wstring command =
        L"-region " + region + L" -lan " + language +
        L" -selfregion " + self_region +
        L" -resolution 1 -upscale 2 -movie 1"
        L" -launcherpath launcher.exe -ctrltype " + controller +
        L" -launcherroot \"" + launcher_directory + L"\"";

    SetEnvironmentVariableW(kMarker, L"1");
    if (GetEnvironmentVariableW(L"SteamAppId", nullptr, 0) == 0)
        SetEnvironmentVariableW(L"SteamAppId", L"2492660");

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    std::vector<wchar_t> mutable_command(command.begin(), command.end());
    mutable_command.push_back(L'\0');
    const BOOL created = CreateProcessW(
        executable.c_str(), mutable_command.data(), nullptr, nullptr, FALSE, 0,
        nullptr, game_directory.c_str(), &startup, &process);
    if (!created) {
        const DWORD code = GetLastError();
        log(launcher_directory, "CreateProcessW failed: " + std::to_string(code));
        error(L"Could not start Peace Walker.", code);
        return 3;
    }

    log(launcher_directory, "Direct launcher " PWUWFIX_VERSION
                            "; game PID=" + std::to_string(process.dwProcessId));
    CloseHandle(process.hThread);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exit_code = 0;
    GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hProcess);
    return static_cast<int>(exit_code);
}
