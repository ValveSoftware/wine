/*
 * Unit test suite for ntdll thread context behavior
 *
 * Copyright 2026 Hoshino Lina
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
 *
 */

#include <stdarg.h>
#include <stdbool.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "wine/test.h"

static void (WINAPI *pGetCurrentThreadStackLimits)(PULONG_PTR,PULONG_PTR);

// 90 second max runtime, to avoid winetest timeouts
#define MAX_RUNTIME 90
#define THREADS 50
#define STACK_SIZE 0x10000
#define LOOPS 2000

#ifdef __x86_64__
#define CTX_SP Rsp
#define CTX_IP Rip
#endif
#ifdef __i386__
#define CTX_SP Esp
#define CTX_IP Eip
#endif
#ifdef __arm__
#define CTX_SP Sp
#define CTX_IP Pc
#endif
#ifdef __aarch64__
#define CTX_SP Sp
#define CTX_IP Pc
#endif

static volatile bool looping = true;

struct thread {
    DWORD tid;
    bool stopped;
    HANDLE hnd;
    CONTEXT ctx;
    DWORD64 stack_lo;
    DWORD64 stack_hi;
    bool complete;
};

struct thread t[THREADS];

DWORD WINAPI thread_func(LPVOID lpParameter)
{
    struct thread *self = lpParameter;

    ULONG_PTR lo, hi;
    pGetCurrentThreadStackLimits(&lo, &hi);

    self->stack_lo = (DWORD64)lo;
    self->stack_hi = (DWORD64)hi;

    while (looping)
        Sleep(0);

    self->complete = true;

    return 0;
}

static bool is_pe_map(DWORD64 addr)
{
    /* See server/mapping.c for the upper limits. */
    return (addr >= 0x60000000 && addr < 0x7c000000) ||
           (addr >= 0x600000000000 && addr < 0x700000000000) ||
#ifdef _WIN64
           (addr >= 0x140000000 && addr < 0x141000000);
#else
           (addr >= 0x400000 && addr < 0x1400000);
#endif

}

static void test_context_sp(void)
{
    int loop;
    bool failed_sp = false;
    bool failed_ip = false;
    DWORD timeout = GetTickCount() + MAX_RUNTIME * 1000;

    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");

    pGetCurrentThreadStackLimits = (void*)GetProcAddress(hKernel32, "GetCurrentThreadStackLimits");
    if (!pGetCurrentThreadStackLimits)
        win_skip("GetCurrentThreadStackLimits not available.\n");

    for (int i = 0; i < THREADS; i++) {
        t[i].hnd = CreateThread(0, STACK_SIZE, thread_func, (LPVOID)&t[i], 0,
                                &t[i].tid);
        ok(!!t[i].hnd, "Failed to create thread");
        if (!t[i].hnd) {
            looping = false;
            break;
        }
    }

    Sleep(100);

    trace("Starting %d loops of thread context fetching\n", LOOPS);

    for (loop = 0; looping && loop < LOOPS && GetTickCount() < timeout;
         loop++) {
        for (int i = 0; i < THREADS; i++) {
            if (SuspendThread(t[i].hnd) != (DWORD)-1) {
                t[i].ctx.ContextFlags = CONTEXT_INTEGER | CONTEXT_CONTROL;

                if (GetThreadContext(t[i].hnd, &t[i].ctx)) {
                    bool in_sp_range = t[i].ctx.CTX_SP > t[i].stack_lo &&
                                       t[i].ctx.CTX_SP <= t[i].stack_hi;
                    bool in_ip_range = is_pe_map(t[i].ctx.CTX_IP);

                    if (!in_sp_range) {
                        trace("[%d/%d:%d] SP=0x%llx [%llx..%llx]\n", loop,
                              LOOPS, i, (long long)t[i].ctx.CTX_SP,
                              (long long)t[i].stack_lo,
                              (long long)t[i].stack_hi);
                        failed_sp = true;
                    }

                    if (!in_ip_range) {
                        trace("[%d/%d:%d] IP=0x%llx\n", loop, LOOPS, i,
                              (long long)t[i].ctx.CTX_IP);
                        failed_ip = true;
                    }

                    t[i].stopped = true;
                } else {
                    ResumeThread(t[i].hnd);
                }
            }
        }

        for (int i = 0; i < THREADS; i++) {
            if (t[i].stopped)
                ResumeThread(t[i].hnd);

            t[i].stopped = false;
        }

        if (failed_sp && failed_ip)
            break;
    }

    trace("Completed %d/%d loops\n", loop, LOOPS);

    looping = false;

    for (int i = 0; i < THREADS; i++)
        WaitForSingleObject(t[i].hnd, INFINITE);

    ok(!failed_sp, "Invalid SP value detected");

#ifdef __i386__
    /* Known broken on i386 */
    todo_wine
#endif
    ok(!failed_ip, "Invalid IP value detected");

    for (int i = 0; i < THREADS; i++)
        ok(t[i].complete, "Thread %d died unexpectedly\n", i);
}

START_TEST(threadctx) {
    test_context_sp();
}
