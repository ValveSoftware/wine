#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(belauncher);

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPWSTR cmdline, int cmdshow)
{
    char *configs, *config, *arch_32_exe = NULL, *arch_64_exe = NULL, *game_exe, *be_arg = NULL;
    LARGE_INTEGER launcher_cfg_size;
    unsigned char battleye_status;
    int game_exe_len, arg_len;
    PROCESS_INFORMATION pi;
    HANDLE launcher_cfg;
    LPWSTR launch_cmd;
    STARTUPINFOW si = {0};
    DWORD size;
    BOOL wow64;

    battleye_status = 0x3; /* Starting */
    _write(1, &battleye_status, 1);

    launcher_cfg = CreateFileW(L"Battleye\\BELauncher.ini", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (launcher_cfg == INVALID_HANDLE_VALUE)
        goto start_failed;

    if(!GetFileSizeEx(launcher_cfg, &launcher_cfg_size) || launcher_cfg_size.u.HighPart)
    {
        CloseHandle(launcher_cfg);
        goto start_failed;
    }

    configs = HeapAlloc( GetProcessHeap(), 0, launcher_cfg_size.u.LowPart);

    if (!ReadFile(launcher_cfg, configs, launcher_cfg_size.u.LowPart, &size, NULL) || size != launcher_cfg_size.u.LowPart)
    {
        CloseHandle(launcher_cfg);
        HeapFree( GetProcessHeap(), 0, configs );
        goto start_failed;
    }

    CloseHandle(launcher_cfg);

    config = configs;
    do
    {
        if (!strncmp(config, "32BitExe=", 9))
            arch_32_exe = config + 9;

        if (!strncmp(config, "64BitExe=", 9))
            arch_64_exe = config + 9;

        if (!strncmp(config, "BEArg=", 6))
            be_arg = config + 6;
    }
    while ((config = strchr(config, '\n')) && *(config++));

    if (arch_64_exe && (sizeof(void *) == 8 || (IsWow64Process(GetCurrentProcess(), &wow64) && wow64)))
        game_exe = arch_64_exe;
    else if (arch_32_exe)
        game_exe = arch_32_exe;
    else
    {
        HeapFree( GetProcessHeap(), 0, configs );
        WINE_WARN("Failed to find game executable name from BattlEye config.\n");
        goto start_failed;
    }

    if (strchr(game_exe, '\r'))
        *(strchr(game_exe, '\r')) = 0;
    if (strchr(game_exe, '\n'))
        *(strchr(game_exe, '\n')) = 0;
    game_exe_len = MultiByteToWideChar(CP_ACP, 0, game_exe, -1, NULL, 0) - 1;

    if (!be_arg) arg_len = 0;
    else
    {
        if (strchr(be_arg, '\r'))
            *(strchr(be_arg, '\r')) = 0;
        if (strchr(be_arg, '\n'))
            *(strchr(be_arg, '\n')) = 0;
        arg_len = MultiByteToWideChar(CP_ACP, 0, be_arg, -1, NULL, 0) - 1;
    }

    WINE_TRACE("Launching game executable %s for BattlEye.\n", game_exe);
    battleye_status = 0x9; /* Launching Game */
    _write(1, &battleye_status, 1);

    launch_cmd = HeapAlloc(GetProcessHeap(), 0, (game_exe_len + 1 + wcslen(cmdline) + 1 + arg_len + 1) * sizeof(WCHAR));
    MultiByteToWideChar(CP_ACP, 0, game_exe, -1, launch_cmd, game_exe_len + 1);
    launch_cmd[game_exe_len] = ' ';

    wcscpy(launch_cmd + game_exe_len + 1, cmdline);
    launch_cmd[game_exe_len + 1 + wcslen(cmdline)] = ' ';

    MultiByteToWideChar(CP_ACP, 0, be_arg, -1, launch_cmd + game_exe_len + 1 + wcslen(cmdline) + 1, arg_len + 1);

    if (!CreateProcessW(NULL, launch_cmd, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi))
    {
        battleye_status = 0xA; /* Launch Failed */
        _write(1, &battleye_status, 1);

        HeapFree( GetProcessHeap(), 0, launch_cmd );
        return GetLastError();
    }
    HeapFree( GetProcessHeap(), 0, launch_cmd );

    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    return 0;

start_failed:
    battleye_status = 0x4; /* Start Failed */
    _write(1, &battleye_status, 1);
    return 0;
}
