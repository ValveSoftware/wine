/*
 * Copyright 2022 Nikolay Sivov for CodeWeavers
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

#include <stdio.h>
#include <stdlib.h>

#include <windows.h>
#include <dbghelp.h>
#include <shellapi.h>

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, WCHAR *cmdline, int nCmdShow)
{
    HANDLE process, dumpfile;
    WCHAR **argv, *ptr;
    DWORD pid;
    int argc;
    BOOL ret;

    argv = CommandLineToArgvW(cmdline, &argc);
    if (argc < 1) return 1;

    ptr = argv[0];
    if (ptr[0] == '0' && towlower(ptr[1]) == 'x')
        pid = wcstoul(ptr, NULL, 16);
    else
        pid = wcstoul(ptr, NULL, 10);

    process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!process)
        return 2;

    dumpfile = CreateFileW(L"minidump.dmp", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (dumpfile == INVALID_HANDLE_VALUE)
    {
        CloseHandle(process);
        return 3;
    }

    ret = MiniDumpWriteDump(process, pid, dumpfile, MiniDumpNormal | MiniDumpWithFullMemory, NULL, NULL, NULL);

    CloseHandle(dumpfile);
    CloseHandle(process);

    return ret ? 0 : 4;
}
