/*
 * Copyright 2026 Paul Gofman for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <windows.h>
#include "wine/test.h"

static void test_console_mode_change(void)
{
    STARTUPINFOA si = { sizeof(si) };
    HANDLE con_out, con_out_dup;
    PROCESS_INFORMATION info;
    DWORD old_mode, mode;
    char cmd[1024];
    DWORD ret;
    BOOL bret;

    FreeConsole();
    bret = AllocConsole();
    ok( bret, "got error %lu.\n", GetLastError() );

    con_out = CreateFileA( "CONOUT$", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, 0 );
    ok( con_out != INVALID_HANDLE_VALUE, "got error %ld.\n", GetLastError() );

    bret = GetConsoleMode( con_out, &old_mode );
    ok( bret, "got error %lu.\n", GetLastError() );
    ok( old_mode == (ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT)
        || old_mode == (ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING),
        "got %#lx.\n", old_mode );
    SetConsoleMode( con_out, ENABLE_PROCESSED_OUTPUT );

    bret = DuplicateHandle( GetCurrentProcess(), con_out, GetCurrentProcess(), &con_out_dup, 0, TRUE, DUPLICATE_SAME_ACCESS );
    ok( bret, "got error %lu.\n", GetLastError() );
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle( STD_INPUT_HANDLE );
    si.hStdError = GetStdHandle( STD_ERROR_HANDLE );
    si.hStdOutput = con_out_dup;
    strcpy( cmd, "cmd.exe /c nonexistent" );
    bret = CreateProcessA( NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &info );
    ok( bret, "got error %lu.\n", GetLastError() );
    ret = WaitForSingleObject( info.hProcess, 5000 );
    ok( ret == WAIT_OBJECT_0, "got %lu.\n", ret );
    CloseHandle( info.hProcess );
    CloseHandle( info.hThread );

    bret = GetConsoleMode( con_out, &mode );
    ok( bret, "got error %lu.\n", GetLastError() );
    todo_wine ok( mode == (ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING)
        || broken( mode == (ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT) ) /* before Win10 */,
        "got %#lx.\n", mode );

    bret = SetConsoleMode( con_out, ENABLE_PROCESSED_OUTPUT );
    ok( bret, "got error %lu.\n", GetLastError() );
    strcpy( cmd, "cmd.exe /c echo \033[31mred text\033[0m\r\n" );
    bret = CreateProcessA( NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &info );
    ok( bret, "got error %lu.\n", GetLastError() );
    wait_child_process( &info );
    bret = GetConsoleMode( con_out, &mode );
    ok( bret, "got error %lu.\n", GetLastError() );
    todo_wine ok( mode == (ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING)
        || broken( mode == (ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT) ) /* before Win10 */,
        "got %#lx.\n", mode );

    bret = SetConsoleMode( con_out, old_mode );
    ok( bret, "got error %lu.\n", GetLastError() );
    CloseHandle( con_out_dup );
    CloseHandle( con_out );
}

START_TEST(console)
{
    test_console_mode_change();
}
