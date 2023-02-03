#include <wchar.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(belauncher);

extern int _write(int, void *, int);

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPWSTR cmdline, int cmdshow)
{
    char *configs, *config, *arch_32_exe = NULL, *arch_64_exe = NULL, *game_exe, *be_arg = NULL, *arg_exe = NULL;
    LARGE_INTEGER launcher_cfg_size;
    unsigned char battleye_status;
    int game_exe_len, arg_len;
    PROCESS_INFORMATION pi;
    HANDLE launcher_cfg;
    LPWSTR launch_cmd, game_cmd, cmd_exe_arg = NULL, cmd_exe_arg_end;
    STARTUPINFOW si = {0};
    DWORD size, exe_arg_len;
    BOOL wow64;

    battleye_status = 0x3; /* Starting */
    _write(1, &battleye_status, 1);

    WINE_TRACE("Started BELauncher with parameters: %s\n", debugstr_w(cmdline));

    launcher_cfg = CreateFileW(L"Battleye\\BELauncher.ini", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (launcher_cfg == INVALID_HANDLE_VALUE)
    {
        WINE_WARN("Failed to open Battleye\\BELauncher.ini file\n");
        goto start_failed;
    }

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

    game_cmd = cmdline;

    /* Check if -exe argument was passed, which will override .ini entry */
    cmd_exe_arg = wcsstr(cmdline, L"-exe ");
    if (cmd_exe_arg && wcslen(cmd_exe_arg) >= 6)
    {
        cmd_exe_arg += 5;
        if (cmd_exe_arg[1] == L'\"')
        {
            cmd_exe_arg_end = wcschr(&cmd_exe_arg[2], L'\"');
            if (cmd_exe_arg_end)
            {
                // Include closing '\"'
                cmd_exe_arg_end++;
            }
        }
        else
        {
            cmd_exe_arg_end = wcschr(cmd_exe_arg, ' ');
        }

        if (!cmd_exe_arg_end)
        {
            // This means -exe was the last argument
            cmd_exe_arg_end = cmd_exe_arg + wcslen(cmd_exe_arg);
        }

        /* Extract the game executable */
        exe_arg_len = cmd_exe_arg_end - cmd_exe_arg;
        arg_exe = HeapAlloc(GetProcessHeap(), 0, exe_arg_len + 1);
        WideCharToMultiByte(CP_ACP, 0, cmd_exe_arg, exe_arg_len, arg_exe, exe_arg_len, NULL, NULL);

        /* Rebuild the command line to skip -exe argument */
        cmd_exe_arg = wcsstr(cmdline, L"-exe ");
        game_cmd = HeapAlloc(GetProcessHeap(), 0, (wcslen(cmdline) + 1) * sizeof(WCHAR));
        lstrcpynW(game_cmd, cmdline, cmd_exe_arg - cmdline + 1);
        game_cmd[cmd_exe_arg - cmdline] = L' ';
        if (cmd_exe_arg_end[0])
            wcscpy(&game_cmd[cmd_exe_arg - cmdline + 1], cmd_exe_arg_end + 1);
    }

    /* Replace the game executable */
    if (arg_exe)
        game_exe = arg_exe;
    else if (arch_64_exe && (sizeof(void *) == 8 || (IsWow64Process(GetCurrentProcess(), &wow64) && wow64)))
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

    battleye_status = 0x9; /* Launching Game */
    _write(1, &battleye_status, 1);

    launch_cmd = HeapAlloc(GetProcessHeap(), 0, (game_exe_len + 1 + wcslen(game_cmd) + 1 + arg_len + 1) * sizeof(WCHAR));
    MultiByteToWideChar(CP_ACP, 0, game_exe, -1, launch_cmd, game_exe_len + 1);
    launch_cmd[game_exe_len] = ' ';

    wcscpy(launch_cmd + game_exe_len + 1, game_cmd);
    launch_cmd[game_exe_len + 1 + wcslen(game_cmd)] = ' ';

    MultiByteToWideChar(CP_ACP, 0, be_arg, -1, launch_cmd + game_exe_len + 1 + wcslen(game_cmd) + 1, arg_len + 1);

    WINE_TRACE("Launching game executable for BattlEye: %s\n", debugstr_w(launch_cmd));

    if (!CreateProcessW(NULL, launch_cmd, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi))
    {
        battleye_status = 0xA; /* Launch Failed */
        _write(1, &battleye_status, 1);

        HeapFree( GetProcessHeap(), 0, launch_cmd );
        return GetLastError();
    }
    HeapFree( GetProcessHeap(), 0, launch_cmd );

    if (game_cmd != cmdline)
        HeapFree( GetProcessHeap(), 0, game_cmd );
    if (arg_exe)    
        HeapFree( GetProcessHeap(), 0, arg_exe );

    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    return 0;

start_failed:
    WINE_WARN("Failed to launch the game using BELauncher\n");
    battleye_status = 0x4; /* Start Failed */
    _write(1, &battleye_status, 1);
    return 0;
}
