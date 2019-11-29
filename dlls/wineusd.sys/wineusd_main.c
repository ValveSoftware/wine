/*
 * User shared data update service
 *
 * Copyright 2019 Rémi Bernon for CodeWeavers
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

#include <assert.h>
#include <stdarg.h>
#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "winternl.h"
#include "winioctl.h"
#include "ddk/wdm.h"
#include "wine/debug.h"
#include "wine/heap.h"
#include "wine/list.h"
#include "wine/usd.h"

static HANDLE directory_obj;
static DEVICE_OBJECT *device_obj;

WINE_DEFAULT_DEBUG_CHANNEL(wineusd);

#define DECLARE_CRITICAL_SECTION(cs) \
    static CRITICAL_SECTION cs; \
    static CRITICAL_SECTION_DEBUG cs##_debug = \
    { 0, 0, &cs, { &cs##_debug.ProcessLocksList, &cs##_debug.ProcessLocksList }, \
      0, 0, { (DWORD_PTR)(__FILE__ ": " # cs) }}; \
    static CRITICAL_SECTION cs = { &cs##_debug, -1, 0, 0, 0, 0 };

DECLARE_CRITICAL_SECTION(wineusd_cs);

static struct list wineusd_entries = LIST_INIT(wineusd_entries);
static HANDLE wineusd_thread, wineusd_thread_stop;

struct wineusd_entry
{
    struct list link;
    ULONG32     pid;
    HANDLE      section;
    void       *page;
};

static NTSTATUS wineusd_create(HANDLE pid, struct wineusd_entry **entry_ptr)
{
    static const WCHAR section_formatW[] = {'\\','D','e','v','i','c','e','\\','W','i','n','e','U','s','d','\\','%','0','8','x',0};
    struct wineusd_entry *entry;
    OBJECT_ATTRIBUTES attr = {sizeof(attr)};
    UNICODE_STRING string;
    NTSTATUS status;
    SIZE_T size = 0;
    WCHAR section_nameW[64];

    LIST_FOR_EACH_ENTRY(entry, &wineusd_entries, struct wineusd_entry, link)
        if (entry->pid == HandleToUlong(PsGetCurrentProcessId()))
            goto done;

    if (!(entry = heap_alloc_zero(sizeof(*entry))))
        return STATUS_NO_MEMORY;
    entry->pid = HandleToUlong(pid);

    swprintf(section_nameW, ARRAY_SIZE(section_nameW), section_formatW, entry->pid);
    RtlInitUnicodeString(&string, section_nameW);
    InitializeObjectAttributes(&attr, &string, 0, NULL, NULL);
    if ((status = NtOpenSection(&entry->section, SECTION_ALL_ACCESS, &attr)))
    {
        /* the main process may be starting up, we should get notified later again */
        WARN("Failed to open section for process %08x, status: %x.\n", entry->pid, status);
        goto error;
    }

    entry->page = NULL;
    size = 0;
    if ((status = NtMapViewOfSection(entry->section, NtCurrentProcess(), &entry->page, 0, 0, 0,
                                     &size, ViewShare, 0, PAGE_READWRITE)))
    {
        ERR("Failed to map section to driver memory, status: %x.\n", status);
        goto error;
    }

    list_add_head(&wineusd_entries, &entry->link);
    TRACE("Created user shared data for process %08x.\n", entry->pid);

done:
    *entry_ptr = entry;
    return STATUS_SUCCESS;

error:
    if (entry && entry->section) NtClose(entry->section);
    if (entry) heap_free(entry);
    return status;
}

static void wineusd_close(struct wineusd_entry *entry)
{
    TRACE("Closing user shared data for process %08x.\n", entry->pid);

    list_remove(&entry->link);
    NtUnmapViewOfSection(NtCurrentProcess(), entry->page);
    NtClose(entry->section);
    heap_free(entry);
}

static NTSTATUS wineusd_initialize(void)
{
    SYSTEM_PROCESS_INFORMATION *spi;
    struct wineusd_entry *entry;
    NTSTATUS status;
    ULONG size = 0x4000;
    char *buffer;

    if (!(buffer = heap_alloc(size)))
        return STATUS_NO_MEMORY;

    while ((status = NtQuerySystemInformation(SystemProcessInformation, buffer, size, NULL))
           == STATUS_INFO_LENGTH_MISMATCH)
    {
        size *= 2;
        if (!(buffer = heap_realloc(buffer, size)))
            return STATUS_NO_MEMORY;
    }

    if (status)
    {
        ERR("Failed to list existing processes, status:%x\n", status);
        goto done;
    }

    spi = (SYSTEM_PROCESS_INFORMATION*)buffer;
    do
    {
        wineusd_create(spi->UniqueProcessId, &entry);
        if (spi->NextEntryOffset == 0) break;
        spi = (SYSTEM_PROCESS_INFORMATION *)((char *)spi + spi->NextEntryOffset);
    }
    while ((char *)spi < buffer + size);

done:
    if (buffer) heap_free(buffer);
    return status;
}

static DWORD WINAPI wineusd_thread_proc(void *arg)
{
    struct wineusd_entry *entry;
    ULARGE_INTEGER interrupt;
    ULARGE_INTEGER tick;
    LARGE_INTEGER now;
    NTSTATUS status;

    EnterCriticalSection(&wineusd_cs);
    if ((status = wineusd_initialize()))
        WARN("Failed to initialize process list, status:%x\n", status);
    LeaveCriticalSection(&wineusd_cs);

    TRACE("Started user shared data thread.\n");

    while (WaitForSingleObject(wineusd_thread_stop, 16) == WAIT_TIMEOUT)
    {
        NtQuerySystemTime(&now);
        RtlQueryUnbiasedInterruptTime(&interrupt.QuadPart);

        tick = interrupt;
        tick.QuadPart /= 10000;

        EnterCriticalSection(&wineusd_cs);
        LIST_FOR_EACH_ENTRY(entry, &wineusd_entries, struct wineusd_entry, link)
        {
            KSHARED_USER_DATA *usd = entry->page;

            usd->SystemTime.High2Time = now.u.HighPart;
            usd->SystemTime.LowPart   = now.u.LowPart;
            usd->SystemTime.High1Time = now.u.HighPart;

            usd->InterruptTime.High2Time = interrupt.HighPart;
            usd->InterruptTime.LowPart   = interrupt.LowPart;
            usd->InterruptTime.High1Time = interrupt.HighPart;

            usd->TickCount.High2Time  = tick.HighPart;
            usd->TickCount.LowPart    = tick.LowPart;
            usd->TickCount.High1Time  = tick.HighPart;
            usd->TickCountLowDeprecated = tick.LowPart;
            usd->TickCountMultiplier    = 1 << 24;
        }
        LeaveCriticalSection(&wineusd_cs);
    }

    TRACE("Stopped user shared data thread.\n");

    return 0;
}

static NTSTATUS WINAPI wineusd_dispatch_create(DEVICE_OBJECT *device, IRP *irp)
{
    struct wineusd_entry *entry = NULL;
    IO_STACK_LOCATION *stack = IoGetCurrentIrpStackLocation(irp);
    NTSTATUS status = STATUS_SUCCESS;

    EnterCriticalSection(&wineusd_cs);
    status = wineusd_create(PsGetCurrentProcessId(), &entry);
    LeaveCriticalSection(&wineusd_cs);

    stack->FileObject->FsContext = entry;
    irp->IoStatus.Status = status;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return status;
}

static NTSTATUS WINAPI wineusd_dispatch_close(DEVICE_OBJECT *device, IRP *irp)
{
    IO_STACK_LOCATION *stack = IoGetCurrentIrpStackLocation(irp);
    struct wineusd_entry *entry = stack->FileObject->FsContext;

    if (entry)
    {
        EnterCriticalSection(&wineusd_cs);
        wineusd_close(entry);
        LeaveCriticalSection(&wineusd_cs);
    }

    irp->IoStatus.Status = STATUS_SUCCESS;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

static void WINAPI wineusd_unload(DRIVER_OBJECT *driver)
{
    struct wineusd_entry *entry, *cursor;

    SetEvent(wineusd_thread_stop);
    WaitForSingleObject(wineusd_thread, INFINITE);
    CloseHandle(wineusd_thread);
    CloseHandle(wineusd_thread_stop);

    EnterCriticalSection(&wineusd_cs);
    LIST_FOR_EACH_ENTRY_SAFE(entry, cursor, &wineusd_entries, struct wineusd_entry, link)
        wineusd_close(entry);

    IoDeleteDevice(device_obj);
    NtClose(directory_obj);
    LeaveCriticalSection(&wineusd_cs);
}

NTSTATUS WINAPI DriverEntry(DRIVER_OBJECT *driver, UNICODE_STRING *path)
{
    static const WCHAR directory_nameW[] = {'\\','D','e','v','i','c','e','\\','W','i','n','e','U','s','d',0};
    static const WCHAR device_nameW[] = {'\\','D','e','v','i','c','e','\\','W','i','n','e','U','s','d','\\','C','o','n','t','r','o','l',0};
    OBJECT_ATTRIBUTES attr = {sizeof(attr)};
    UNICODE_STRING string;
    NTSTATUS status;

    TRACE("Driver %p, path %s.\n", driver, debugstr_w(path->Buffer));

    RtlInitUnicodeString(&string, directory_nameW);
    InitializeObjectAttributes(&attr, &string, 0, NULL, NULL);
    if ((status = NtCreateDirectoryObject(&directory_obj, 0, &attr)) &&
        status != STATUS_OBJECT_NAME_COLLISION)
        ERR("Failed to create directory, status: %x\n", status);

    RtlInitUnicodeString(&string, device_nameW);
    if ((status = IoCreateDevice(driver, 0, &string, FILE_DEVICE_UNKNOWN, 0, FALSE, &device_obj)))
    {
        ERR("Failed to create user shared data device, status: %x\n", status);
        NtClose(directory_obj);
        return status;
    }

    driver->MajorFunction[IRP_MJ_CREATE] = wineusd_dispatch_create;
    driver->MajorFunction[IRP_MJ_CLOSE] = wineusd_dispatch_close;
    driver->DriverUnload = wineusd_unload;

    wineusd_thread_stop = CreateEventW(NULL, FALSE, FALSE, NULL);
    wineusd_thread = CreateThread(NULL, 0, wineusd_thread_proc, NULL, 0, NULL);

    return STATUS_SUCCESS;
}
