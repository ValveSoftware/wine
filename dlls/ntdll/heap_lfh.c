/*
 * Wine Low Fragmentation Heap
 *
 * Copyright 2020 Remi Bernon for CodeWeavers
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

#include "ntstatus.h"
#define WIN32_NO_STATUS

#include "ntdll_misc.h"

NTSTATUS HEAP_lfh_allocate( HANDLE std_heap, ULONG flags, SIZE_T size, void **out )
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS HEAP_lfh_free( HANDLE std_heap, ULONG flags, void *ptr )
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS HEAP_lfh_reallocate( HANDLE std_heap, ULONG flags, void *ptr, SIZE_T size, void **out )
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS HEAP_lfh_get_allocated_size( HANDLE std_heap, ULONG flags, const void *ptr, SIZE_T *out )
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS HEAP_lfh_validate( HANDLE std_heap, ULONG flags, const void *ptr )
{
    return STATUS_NOT_IMPLEMENTED;
}

void HEAP_lfh_notify_thread_destroy(BOOLEAN last)
{
}

void HEAP_lfh_set_debug_flags(ULONG flags)
{
}
