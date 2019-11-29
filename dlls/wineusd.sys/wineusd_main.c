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
#include "wine/usd.h"
#include "wine/list.h"

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
    CLIENT_ID   cid;
    HANDLE      process;
    HANDLE      section;
    void       *page;
};

static DWORD WINAPI wineusd_thread_proc(void *arg)
{
    struct wineusd_entry *entry;
    ULARGE_INTEGER interrupt;
    ULARGE_INTEGER tick;
    LARGE_INTEGER now;

    TRACE("Started user shared data thread.\n");

    while (WaitForSingleObject(wineusd_thread_stop, 16) == WAIT_TIMEOUT)
    {
        EnterCriticalSection(&wineusd_cs);

        NtQuerySystemTime(&now);
        RtlQueryUnbiasedInterruptTime(&interrupt.QuadPart);

        tick = interrupt;
        tick.QuadPart /= 10000;

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

static NTSTATUS wineusd_initialize(struct wineusd_entry *entry, IRP *irp)
{
    const KSHARED_USER_DATA *init = irp->AssociatedIrp.SystemBuffer;
    void *user_shared_data_address = (void *)0x7ffe0000;
    NTSTATUS status = STATUS_SUCCESS;
    LARGE_INTEGER section_size;
    SIZE_T view_size;

    entry->cid.UniqueProcess = PsGetCurrentProcessId();
    entry->cid.UniqueThread = (HANDLE)0;
    if ((status = NtOpenProcess(&entry->process, PROCESS_VM_OPERATION, NULL, &entry->cid)))
    {
        ERR("Failed to open target process, status: %x.\n", status);
        return status;
    }

    section_size.HighPart = 0;
    section_size.LowPart = 0x10000;
    if ((status = NtCreateSection(&entry->section, SECTION_ALL_ACCESS, NULL, &section_size,
                                  PAGE_READWRITE, SEC_COMMIT, NULL)))
    {
        ERR("Failed to create section, status: %x.\n", status);
        return status;
    }

    view_size = 0;
    if ((status = NtMapViewOfSection(entry->section, NtCurrentProcess(), &entry->page, 0, 0, 0,
                                     &view_size, ViewShare, 0, PAGE_READWRITE)))
    {
        ERR("Failed to map section to driver memory, status: %x.\n", status);
        return status;
    }
    memcpy(entry->page, init, sizeof(*init));

    view_size = 0;
    if ((status = NtMapViewOfSection(entry->section, entry->process, &user_shared_data_address, 0, 0, 0,
                                     &view_size, ViewShare, 0, PAGE_READONLY)))
    {
        ERR("Failed to map user shared data to target process, status: %x.\n", status);
        return status;
    }

    EnterCriticalSection(&wineusd_cs);
    list_add_head(&wineusd_entries, &entry->link);
    LeaveCriticalSection(&wineusd_cs);

    TRACE("Initialized user shared data for process %04x.\n", HandleToULong(PsGetCurrentProcessId()));

    return STATUS_SUCCESS;
}

static void wineusd_close(struct wineusd_entry *entry)
{
    void *user_shared_data_address = (void *)0x7ffe0000;

    TRACE("Closing user shared data for process %04x.\n", HandleToULong(entry->cid.UniqueProcess));

    EnterCriticalSection(&wineusd_cs);
    list_remove(&entry->link);
    LeaveCriticalSection(&wineusd_cs);

    NtUnmapViewOfSection(entry->process, user_shared_data_address);
    NtUnmapViewOfSection(NtCurrentProcess(), entry->page);

    NtClose(entry->section);
    NtClose(entry->process);
    heap_free(entry);
}

static NTSTATUS WINAPI wineusd_dispatch_create(DEVICE_OBJECT *device, IRP *irp)
{
    IO_STACK_LOCATION *stack = IoGetCurrentIrpStackLocation(irp);
    struct wineusd_entry *entry;
    NTSTATUS status;

    status = STATUS_NO_MEMORY;
    if (!(entry = heap_alloc_zero(sizeof(*entry))))
    {
        ERR("Failed to allocate memory.\n");
        goto done;
    }
    stack->FileObject->FsContext = entry;

    list_init(&entry->link);
    entry->process = INVALID_HANDLE_VALUE;
    entry->section = INVALID_HANDLE_VALUE;
    entry->page    = NULL;

    TRACE("Created user shared data for process %04x.\n", HandleToULong(PsGetCurrentProcessId()));

done:
    irp->IoStatus.Status = status;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return status;
}

static NTSTATUS WINAPI wineusd_dispatch_close(DEVICE_OBJECT *device, IRP *irp)
{
    IO_STACK_LOCATION *stack = IoGetCurrentIrpStackLocation(irp);
    struct wineusd_entry *entry = stack->FileObject->FsContext;

    if (entry) wineusd_close(entry);

    irp->IoStatus.Status = STATUS_SUCCESS;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

static NTSTATUS WINAPI wineusd_dispatch_ioctl(DEVICE_OBJECT *device, IRP *irp)
{
    IO_STACK_LOCATION *stack = IoGetCurrentIrpStackLocation(irp);
    struct wineusd_entry *entry = stack->FileObject->FsContext;
    NTSTATUS ret;

    switch (stack->Parameters.DeviceIoControl.IoControlCode)
    {
    case IOCTL_WINEUSD_INITIALIZE:
        ret = wineusd_initialize(entry, irp);
        break;
    default:
        FIXME("Unhandled ioctl %#x.\n", stack->Parameters.DeviceIoControl.IoControlCode);
        ret = STATUS_NOT_IMPLEMENTED;
        break;
    }

    irp->IoStatus.Status = ret;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return ret;
}

static void WINAPI wineusd_unload(DRIVER_OBJECT *driver)
{
    struct wineusd_entry *entry, *cursor;

    SetEvent(wineusd_thread_stop);
    WaitForSingleObject(wineusd_thread, INFINITE);
    CloseHandle(wineusd_thread);
    CloseHandle(wineusd_thread_stop);

    LIST_FOR_EACH_ENTRY_SAFE(entry, cursor, &wineusd_entries, struct wineusd_entry, link)
        wineusd_close(entry);

    IoDeleteDevice(device_obj);
}

NTSTATUS WINAPI DriverEntry(DRIVER_OBJECT *driver, UNICODE_STRING *path)
{
    static const WCHAR device_nameW[] = {'\\','D','e','v','i','c','e','\\','W','i','n','e','U','s','d',0};
    OBJECT_ATTRIBUTES attr = {sizeof(attr)};
    UNICODE_STRING string;
    NTSTATUS ret;

    TRACE("Driver %p, path %s.\n", driver, debugstr_w(path->Buffer));

    RtlInitUnicodeString(&string, device_nameW);
    if ((ret = IoCreateDevice(driver, 0, &string, FILE_DEVICE_UNKNOWN, 0, FALSE, &device_obj)))
    {
        ERR("Failed to create user shared data device, status %#x.\n", ret);
        return ret;
    }

    driver->MajorFunction[IRP_MJ_CREATE] = wineusd_dispatch_create;
    driver->MajorFunction[IRP_MJ_CLOSE] = wineusd_dispatch_close;
    driver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = wineusd_dispatch_ioctl;
    driver->DriverUnload = wineusd_unload;

    wineusd_thread_stop = CreateEventW(NULL, FALSE, FALSE, NULL);
    wineusd_thread = CreateThread(NULL, 0, wineusd_thread_proc, NULL, 0, NULL);

    return STATUS_SUCCESS;
}
