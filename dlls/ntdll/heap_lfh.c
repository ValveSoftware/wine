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

#include "ntdll_misc.h"

void *HEAP_lfh_allocate(struct tagHEAP *std_heap, ULONG flags, SIZE_T size)
{
    return NULL;
}

BOOLEAN HEAP_lfh_free(struct tagHEAP *std_heap, ULONG flags, void *ptr)
{
    return FALSE;
}

void *HEAP_lfh_reallocate(struct tagHEAP *std_heap, ULONG flags, void *ptr, SIZE_T size)
{
    return NULL;
}

SIZE_T HEAP_lfh_get_allocated_size(struct tagHEAP *std_heap, ULONG flags, const void *ptr)
{
    return ~(SIZE_T)0;
}

BOOLEAN HEAP_lfh_validate(struct tagHEAP *std_heap, ULONG flags, const void *ptr)
{
    if (ptr) return FALSE;
    else return TRUE;
}

void HEAP_lfh_notify_thread_destroy(BOOLEAN last)
{
}

void HEAP_lfh_set_debug_flags(ULONG flags)
{
}
