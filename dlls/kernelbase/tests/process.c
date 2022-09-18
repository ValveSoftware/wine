/*
 * Process tests
 *
 * Copyright 2021 Jinoh Kang
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

#include <stdarg.h>
#include <stdlib.h>

#include <ntstatus.h>
#define WIN32_NO_STATUS
#include <windef.h>
#include <winbase.h>
#include <winerror.h>
#include <winternl.h>

#include "wine/test.h"

static NTSTATUS (WINAPI *pNtQueryObject)(HANDLE,OBJECT_INFORMATION_CLASS,PVOID,ULONG,PULONG);

static BOOL (WINAPI *pCompareObjectHandles)(HANDLE, HANDLE);
static HANDLE (WINAPI *pOpenFileMappingFromApp)( ULONG, BOOL, LPCWSTR);
static HANDLE (WINAPI *pCreateFileMappingFromApp)(HANDLE, PSECURITY_ATTRIBUTES, ULONG, ULONG64, PCWSTR);

static void test_CompareObjectHandles(void)
{
    HANDLE h1, h2;
    BOOL ret;

    if (!pCompareObjectHandles)
    {
        skip("CompareObjectHandles is not available.\n");
        return;
    }

    ret = pCompareObjectHandles( GetCurrentProcess(), GetCurrentProcess() );
    ok( ret, "comparing GetCurrentProcess() to self failed with %u\n", GetLastError() );

    ret = pCompareObjectHandles( GetCurrentThread(), GetCurrentThread() );
    ok( ret, "comparing GetCurrentThread() to self failed with %u\n", GetLastError() );

    SetLastError(0);
    ret = pCompareObjectHandles( GetCurrentProcess(), GetCurrentThread() );
    ok( !ret && GetLastError() == ERROR_NOT_SAME_OBJECT,
        "comparing GetCurrentProcess() to GetCurrentThread() returned %u\n", GetLastError() );

    h1 = NULL;
    ret = DuplicateHandle( GetCurrentProcess(), GetCurrentProcess(), GetCurrentProcess(),
                           &h1, 0, FALSE, DUPLICATE_SAME_ACCESS );
    ok( ret, "failed to duplicate current process handle: %u\n", GetLastError() );

    ret = pCompareObjectHandles( GetCurrentProcess(), h1 );
    ok( ret, "comparing GetCurrentProcess() with %p failed with %u\n", h1, GetLastError() );

    CloseHandle( h1 );

    h1 = CreateFileA( "\\\\.\\NUL", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, 0 );
    ok( h1 != INVALID_HANDLE_VALUE, "CreateFile failed (%d)\n", GetLastError() );

    h2 = NULL;
    ret = DuplicateHandle( GetCurrentProcess(), h1, GetCurrentProcess(),
                           &h2, 0, FALSE, DUPLICATE_SAME_ACCESS );
    ok( ret, "failed to duplicate handle %p: %u\n", h1, GetLastError() );

    ret = pCompareObjectHandles( h1, h2 );
    ok( ret, "comparing %p with %p failed with %u\n", h1, h2, GetLastError() );

    CloseHandle( h2 );

    h2 = CreateFileA( "\\\\.\\NUL", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, 0 );
    ok( h2 != INVALID_HANDLE_VALUE, "CreateFile failed (%d)\n", GetLastError() );

    SetLastError(0);
    ret = pCompareObjectHandles( h1, h2 );
    ok( !ret && GetLastError() == ERROR_NOT_SAME_OBJECT,
        "comparing %p with %p returned %u\n", h1, h2, GetLastError() );

    CloseHandle( h2 );
    CloseHandle( h1 );
}

static void test_OpenFileMappingFromApp(void)
{
    OBJECT_BASIC_INFORMATION info;
    HANDLE file, mapping;
    NTSTATUS status;
    ULONG length;

    if (!pOpenFileMappingFromApp)
    {
        win_skip("OpenFileMappingFromApp is not available.\n");
        return;
    }

    file = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_EXECUTE_READ, 0, 4090, "foo");
    ok(!!file, "Failed to create a mapping.\n");

    mapping = pOpenFileMappingFromApp(FILE_MAP_READ, FALSE, L"foo");
    ok(!!mapping, "Failed to open a mapping.\n");
    status = pNtQueryObject(mapping, ObjectBasicInformation, &info, sizeof(info), &length);
    ok(!status, "Failed to get object information.\n");
    ok(info.GrantedAccess == SECTION_MAP_READ, "Unexpected access mask %#x.\n", info.GrantedAccess);
    CloseHandle(mapping);

    mapping = pOpenFileMappingFromApp(FILE_MAP_EXECUTE, FALSE, L"foo");
    ok(!!mapping, "Failed to open a mapping.\n");
    status = pNtQueryObject(mapping, ObjectBasicInformation, &info, sizeof(info), &length);
    ok(!status, "Failed to get object information.\n");
    todo_wine
    ok(info.GrantedAccess == SECTION_MAP_EXECUTE, "Unexpected access mask %#x.\n", info.GrantedAccess);
    CloseHandle(mapping);

    CloseHandle(file);
}

static void test_CreateFileMappingFromApp(void)
{
    OBJECT_BASIC_INFORMATION info;
    NTSTATUS status;
    ULONG length;
    HANDLE file;

    if (!pCreateFileMappingFromApp)
    {
        win_skip("CreateFileMappingFromApp is not available.\n");
        return;
    }

    file = pCreateFileMappingFromApp(INVALID_HANDLE_VALUE, NULL, PAGE_EXECUTE_READWRITE, 1024, L"foo");
    ok(!!file || broken(!file) /* Win8 */, "Failed to create a mapping, error %u.\n", GetLastError());
    if (!file) return;

    status = pNtQueryObject(file, ObjectBasicInformation, &info, sizeof(info), &length);
    ok(!status, "Failed to get object information.\n");
    ok(info.GrantedAccess & SECTION_MAP_EXECUTE, "Unexpected access mask %#x.\n", info.GrantedAccess);

    CloseHandle(file);
}

static void init_funcs(void)
{
    HMODULE hmod = GetModuleHandleA("kernelbase.dll");

#define X(f) { p##f = (void*)GetProcAddress(hmod, #f); }
    X(CompareObjectHandles);
    X(CreateFileMappingFromApp);
    X(OpenFileMappingFromApp);

    hmod = GetModuleHandleA("ntdll.dll");

    X(NtQueryObject);
#undef X
}

START_TEST(process)
{
    init_funcs();

    test_CompareObjectHandles();
    test_OpenFileMappingFromApp();
    test_CreateFileMappingFromApp();
}
