/*
 * eventfd-based synchronization objects
 *
 * Copyright (C) 2018 Zebediah Figura
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

#include "config.h"

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#ifdef HAVE_SYS_EVENTFD_H
# include <sys/eventfd.h>
#endif

#include "ntstatus.h"
#define WIN32_NO_STATUS
#define NONAMELESSUNION
#include "windef.h"
#include "winternl.h"
#include "wine/server.h"
#include "wine/debug.h"

#include "ntdll_misc.h"
#include "esync.h"

#ifndef EFD_SEMAPHORE
#define EFD_SEMAPHORE 1
#endif

WINE_DEFAULT_DEBUG_CHANNEL(esync);

int do_esync(void)
{
#ifdef HAVE_SYS_EVENTFD_H
    static int do_esync_cached = -1;

    if (do_esync_cached == -1)
        do_esync_cached = (getenv("WINEESYNC") != NULL);

    return do_esync_cached;
#else
    static int once;
    if (!once++)
        FIXME("eventfd not supported on this platform.\n");
    return 0;
#endif
}

enum esync_type
{
    ESYNC_SEMAPHORE = 1,
};

struct esync
{
    enum esync_type type;
    int fd;
};

struct semaphore
{
    struct esync obj;
    int max;
};

static NTSTATUS create_esync(int *fd, HANDLE *handle, ACCESS_MASK access,
    const OBJECT_ATTRIBUTES *attr, int initval, int flags)
{
    NTSTATUS ret;
    data_size_t len;
    struct object_attributes *objattr;
    obj_handle_t fd_handle;
    sigset_t sigset;

    if ((ret = alloc_object_attributes( attr, &objattr, &len ))) return ret;

    /* We have to synchronize on the fd cache CS so that our calls to
     * receive_fd don't race with theirs. */
    server_enter_uninterrupted_section( &fd_cache_section, &sigset );
    SERVER_START_REQ( create_esync )
    {
        req->access  = access;
        req->initval = initval;
        req->flags   = flags;
        wine_server_add_data( req, objattr, len );
        ret = wine_server_call( req );
        if (!ret || ret == STATUS_OBJECT_NAME_EXISTS)
        {
            *handle = wine_server_ptr_handle( reply->handle );
            *fd = receive_fd( &fd_handle );
            assert( wine_server_ptr_handle(fd_handle) == *handle );
        }
    }
    SERVER_END_REQ;
    server_leave_uninterrupted_section( &fd_cache_section, &sigset );

    TRACE("-> handle %p, fd %d.\n", *handle, *fd);

    RtlFreeHeap( GetProcessHeap(), 0, objattr );
    return ret;
}

extern NTSTATUS esync_create_semaphore(HANDLE *handle, ACCESS_MASK access,
    const OBJECT_ATTRIBUTES *attr, LONG initial, LONG max)
{
    struct semaphore *semaphore;
    NTSTATUS ret;
    int fd = -1;

    TRACE("name %s, initial %d, max %d.\n",
        attr ? debugstr_us(attr->ObjectName) : "<no name>", initial, max);

    ret = create_esync( &fd, handle, access, attr, initial, EFD_SEMAPHORE );
    if (!ret || ret == STATUS_OBJECT_NAME_EXISTS)
    {
        semaphore = RtlAllocateHeap( GetProcessHeap(), 0, sizeof(*semaphore) );
        if (!semaphore)
            return STATUS_NO_MEMORY;

        semaphore->obj.type = ESYNC_SEMAPHORE;
        semaphore->obj.fd = fd;
        semaphore->max = max;
    }

    return ret;
}
