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
#include "wine/port.h"

#include <assert.h>
#include <errno.h>
#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#ifdef HAVE_SYS_POLL_H
# include <sys/poll.h>
#endif
#include <stdarg.h>
#include <stdlib.h>
#ifdef HAVE_SYS_EVENTFD_H
# include <sys/eventfd.h>
#endif
#ifdef HAVE_SYS_MMAN_H
# include <sys/mman.h>
#endif

#include "ntstatus.h"
#define WIN32_NO_STATUS
#define NONAMELESSUNION
#include "windef.h"
#include "winternl.h"
#include "wine/server.h"
#include "wine/debug.h"
#include "wine/library.h"

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

/* Entry point for drivers to set queue fd. */
void __wine_esync_set_queue_fd( int fd )
{
    ntdll_get_thread_data()->esync_queue_fd = fd;
}

struct esync
{
    enum esync_type type;   /* defined in protocol.def */
    int fd;
};

struct semaphore
{
    struct esync obj;
    int max;
};

struct event
{
    struct esync obj;
};

struct mutex
{
    struct esync obj;
    DWORD tid;
    int count;    /* recursion count */
};

/* We'd like lookup to be fast. To that end, we use a static list indexed by handle.
 * This is copied and adapted from the fd cache code. */

#define ESYNC_LIST_BLOCK_SIZE  (65536 / sizeof(struct esync *))
#define ESYNC_LIST_ENTRIES     128

static struct esync * *esync_list[ESYNC_LIST_ENTRIES];
static struct esync * esync_list_initial_block[ESYNC_LIST_BLOCK_SIZE];

static inline UINT_PTR handle_to_index( HANDLE handle, UINT_PTR *entry )
{
    UINT_PTR idx = (((UINT_PTR)handle) >> 2) - 1;
    *entry = idx / ESYNC_LIST_BLOCK_SIZE;
    return idx % ESYNC_LIST_BLOCK_SIZE;
}

static BOOL add_to_list( HANDLE handle, struct esync *obj )
{
    UINT_PTR entry, idx = handle_to_index( handle, &entry );

    if (entry >= ESYNC_LIST_ENTRIES)
    {
        FIXME( "too many allocated handles, not caching %p\n", handle );
        return FALSE;
    }

    if (!esync_list[entry])  /* do we need to allocate a new block of entries? */
    {
        if (!entry) esync_list[0] = esync_list_initial_block;
        else
        {
            void *ptr = wine_anon_mmap( NULL, ESYNC_LIST_BLOCK_SIZE * sizeof(struct esync *),
                                        PROT_READ | PROT_WRITE, 0 );
            if (ptr == MAP_FAILED) return FALSE;
            esync_list[entry] = ptr;
        }
    }

    obj = interlocked_xchg_ptr((void **)&esync_list[entry][idx], obj);
    assert(!obj);
    return TRUE;
}

static void *esync_get_object( HANDLE handle )
{
    UINT_PTR entry, idx = handle_to_index( handle, &entry );

    if (entry >= ESYNC_LIST_ENTRIES || !esync_list[entry]) return NULL;

    return esync_list[entry][idx];
}

/* Gets a waitable object. This is either a proper esync object (i.e. an event,
 * semaphore, etc. created using create_esync) or a generic synchronizable
 * server-side object which the server will signal (e.g. a process, thread,
 * message queue, etc.) */
static NTSTATUS get_waitable_object( HANDLE handle, struct esync **obj )
{
    obj_handle_t fd_handle;
    enum esync_type type;
    struct esync *esync;
    sigset_t sigset;
    NTSTATUS ret;
    int fd;

    if ((*obj = esync_get_object( handle ))) return STATUS_SUCCESS;

    /* We need to try grabbing it from the server. */
    server_enter_uninterrupted_section( &fd_cache_section, &sigset );
    if (!(esync = esync_get_object( handle )))
    {
        SERVER_START_REQ( get_esync_fd )
        {
            req->handle = wine_server_obj_handle( handle );
            if (!(ret = wine_server_call( req )))
            {
                type = reply->type;
                fd = receive_fd( &fd_handle );
                assert( wine_server_ptr_handle(fd_handle) == handle );
            }
        }
        SERVER_END_REQ;
    }
    server_leave_uninterrupted_section( &fd_cache_section, &sigset );

    if (esync)
    {
        /* We managed to grab it while in the CS; return it. */
        *obj = esync;
        return STATUS_SUCCESS;
    }

    if (ret)
    {
        WARN("Failed to retrieve fd for handle %p, status %#x.\n", handle, ret);
        *obj = NULL;
        return ret;
    }

    TRACE("Got fd %d for handle %p.\n", fd, handle);

    esync = RtlAllocateHeap( GetProcessHeap(), 0, sizeof(*esync) );
    esync->fd = fd;
    esync->type = type;

    add_to_list( handle, esync );

    *obj = esync;
    return ret;
}

NTSTATUS esync_close( HANDLE handle )
{
    UINT_PTR entry, idx = handle_to_index( handle, &entry );
    struct esync *obj;

    TRACE("%p.\n", handle);

    if (entry < ESYNC_LIST_ENTRIES && esync_list[entry])
    {
        if ((obj = interlocked_xchg_ptr( (void **)&esync_list[entry][idx], 0 )))
        {
            close( obj->fd );
            RtlFreeHeap( GetProcessHeap(), 0, obj );
            return STATUS_SUCCESS;
        }
    }

    return STATUS_INVALID_HANDLE;
}

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

NTSTATUS esync_create_semaphore(HANDLE *handle, ACCESS_MASK access,
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

        add_to_list( *handle, &semaphore->obj );
    }

    return ret;
}

NTSTATUS esync_release_semaphore( HANDLE handle, ULONG count, ULONG *prev )
{
    struct semaphore *semaphore = esync_get_object( handle );
    uint64_t count64 = count;

    TRACE("%p, %d, %p.\n", handle, count, prev);

    if (!semaphore) return STATUS_INVALID_HANDLE;

    if (prev)
    {
        FIXME("Can't write previous value.\n");
        *prev = 1;
    }

    if (write( semaphore->obj.fd, &count64, sizeof(count64) ) == -1)
        return FILE_GetNtStatus();

    return STATUS_SUCCESS;
}

NTSTATUS esync_create_event( HANDLE *handle, ACCESS_MASK access,
    const OBJECT_ATTRIBUTES *attr, EVENT_TYPE type, BOOLEAN initial )
{
    struct event *event;
    NTSTATUS ret;
    int fd;

    TRACE("name %s, %s-reset, initial %d.\n",
        attr ? debugstr_us(attr->ObjectName) : "<no name>",
        type == NotificationEvent ? "manual" : "auto", initial);

    ret = create_esync( &fd, handle, access, attr, initial, 0 );
    if (!ret || ret == STATUS_OBJECT_NAME_EXISTS)
    {
        event = RtlAllocateHeap( GetProcessHeap(), 0, sizeof(*event) );
        if (!event)
            return STATUS_NO_MEMORY;

        event->obj.type = (type == NotificationEvent ? ESYNC_MANUAL_EVENT : ESYNC_AUTO_EVENT);
        event->obj.fd = fd;

        add_to_list( *handle, &event->obj);
    }

    return ret;
}

NTSTATUS esync_set_event( HANDLE handle )
{
    struct event *event = esync_get_object( handle );
    static const uint64_t value = 1;

    TRACE("%p.\n", handle);

    if (!event) return STATUS_INVALID_HANDLE;

    if (write( event->obj.fd, &value, sizeof(value) ) == -1)
        return FILE_GetNtStatus();

    return STATUS_SUCCESS;
}

NTSTATUS esync_reset_event( HANDLE handle )
{
    struct event *event = esync_get_object( handle );
    static uint64_t value;

    TRACE("%p.\n", handle);

    if (!event) return STATUS_INVALID_HANDLE;

    /* we don't care about the return value */
    read( event->obj.fd, &value, sizeof(value) );

    return STATUS_SUCCESS;
}

NTSTATUS esync_pulse_event( HANDLE handle )
{
    struct event *event = esync_get_object( handle );
    static uint64_t value = 1;

    TRACE("%p.\n", handle);

    if (!event) return STATUS_INVALID_HANDLE;

    /* This isn't really correct; an application could miss the write.
     * Unfortunately we can't really do much better. Fortunately this is rarely
     * used (and publicly deprecated). */
    if (write( event->obj.fd, &value, sizeof(value) ) == -1)
        return FILE_GetNtStatus();
    read( event->obj.fd, &value, sizeof(value) );

    return STATUS_SUCCESS;
}

NTSTATUS esync_create_mutex( HANDLE *handle, ACCESS_MASK access,
    const OBJECT_ATTRIBUTES *attr, BOOLEAN initial )
{
    struct mutex *mutex;
    NTSTATUS ret;
    int fd;

    TRACE("name %s, initial %d.\n",
        attr ? debugstr_us(attr->ObjectName) : "<no name>", initial);

    ret = create_esync( &fd, handle, access, attr, initial ? 0 : 1, 0 );
    if (!ret || ret == STATUS_OBJECT_NAME_EXISTS)
    {
        mutex = RtlAllocateHeap( GetProcessHeap(), 0, sizeof(*mutex) );
        if (!mutex)
            return STATUS_NO_MEMORY;

        /* can't take ownership of the mutex if we didn't create it */
        if (ret == STATUS_OBJECT_NAME_EXISTS)
            initial = FALSE;

        mutex->obj.type = ESYNC_MUTEX;
        mutex->obj.fd = fd;
        mutex->tid = initial ? GetCurrentThreadId() : 0;
        mutex->count = initial ? 1 : 0;

        add_to_list( *handle, &mutex->obj);
    }

    return ret;
}

NTSTATUS esync_release_mutex( HANDLE *handle, LONG *prev )
{
    struct mutex *mutex = esync_get_object( handle );
    static const uint64_t value = 1;

    TRACE("%p, %p.\n", handle, prev);

    if (!mutex) return STATUS_INVALID_HANDLE;

    /* This is thread-safe, because the only thread that can change the tid to
     * or from our tid is ours. */
    if (mutex->tid != GetCurrentThreadId()) return STATUS_MUTANT_NOT_OWNED;

    if (prev) *prev = mutex->count;

    mutex->count--;

    if (!mutex->count)
    {
        /* This is also thread-safe, as long as signaling the file is the last
         * thing we do. Other threads don't care about the tid if it isn't
         * theirs. */
        mutex->tid = 0;

        if (write( mutex->obj.fd, &value, sizeof(value) ) == -1)
            return FILE_GetNtStatus();
    }

    return STATUS_SUCCESS;
}

#define TICKSPERSEC        10000000
#define TICKSPERMSEC       10000

static LONGLONG update_timeout( ULONGLONG end )
{
    LARGE_INTEGER now;
    LONGLONG timeleft;

    NtQuerySystemTime( &now );
    timeleft = end - now.QuadPart;
    if (timeleft < 0) timeleft = 0;
    return timeleft;
}

static int do_poll( struct pollfd *fds, nfds_t nfds, ULONGLONG *end )
{
    int ret;

    do
    {
        if (end)
        {
            LONGLONG timeleft = update_timeout( *end );

#ifdef HAVE_PPOLL
            /* We use ppoll() if available since the time granularity is better. */
            struct timespec tmo_p;
            tmo_p.tv_sec = timeleft / (ULONGLONG)TICKSPERSEC;
            tmo_p.tv_nsec = (timeleft % TICKSPERSEC) * 100;
            ret = ppoll( fds, nfds, &tmo_p, NULL );
#else
            ret = poll( fds, nfds, timeleft / TICKSPERMSEC );
#endif
        }
        else
            ret = poll( fds, nfds, -1 );

    /* If we receive EINTR we were probably suspended (SIGUSR1), possibly for a
     * system APC. The right thing to do is just try again. */
    } while (ret < 0 && errno == EINTR);

    return ret;
}

/* A value of STATUS_NOT_IMPLEMENTED returned from this function means that we
 * need to delegate to server_select(). */
NTSTATUS esync_wait_objects( DWORD count, const HANDLE *handles, BOOLEAN wait_any,
                             BOOLEAN alertable, const LARGE_INTEGER *timeout )
{
    struct esync *objs[MAXIMUM_WAIT_OBJECTS];
    struct pollfd fds[MAXIMUM_WAIT_OBJECTS + 1];
    int has_esync = 0, has_server = 0;
    DWORD pollcount = count;
    BOOL msgwait = FALSE;
    LONGLONG timeleft;
    LARGE_INTEGER now;
    ULONGLONG end;
    int ret;
    int i;

    NtQuerySystemTime( &now );
    if (timeout)
    {
        if (timeout->QuadPart == TIMEOUT_INFINITE)
            timeout = NULL;
        else if (timeout->QuadPart >= 0)
            end = timeout->QuadPart;
        else
            end = now.QuadPart - timeout->QuadPart;
    }

    for (i = 0; i < count; i++)
    {
        ret = get_waitable_object( handles[i], &objs[i] );
        if (ret == STATUS_SUCCESS)
            has_esync = 1;
        else if (ret == STATUS_NOT_IMPLEMENTED)
            has_server = 1;
        else
            return ret;
    }

    if (objs[count - 1] && objs[count - 1]->type == ESYNC_QUEUE)
    {
        /* Last object in the list is a queue, which means someone is using
         * MsgWaitForMultipleObjects(). We have to wait not only for the server
         * fd (signaled on send_message, etc.) but also the USER driver's fd
         * (signaled on e.g. X11 events.) */
        msgwait = TRUE;
    }

    if (has_esync && has_server)
    {
        FIXME("Can't wait on esync and server objects at the same time!\n");
        /* Wait on just the eventfds; it's the best we can do. */
    }
    else if (has_server)
    {
        /* It's just server objects, so delegate to the server. */
        return STATUS_NOT_IMPLEMENTED;
    }

    if (TRACE_ON(esync))
    {
        TRACE("Waiting for %s of %d handles:", wait_any ? "any" : "all", count);
        for (i = 0; i < count; i++)
            DPRINTF(" %p", handles[i]);

        if (msgwait)
            DPRINTF(" or driver events (fd %d)", ntdll_get_thread_data()->esync_queue_fd);

        if (!timeout)
            DPRINTF(", timeout = INFINITE.\n");
        else
        {
            timeleft = update_timeout( end );
            DPRINTF(", timeout = %ld.%07ld sec.\n",
                (long) timeleft / TICKSPERSEC, (long) timeleft % TICKSPERSEC);
        }
    }

    if (wait_any || count == 1)
    {
        for (i = 0; i < count; i++)
        {
            fds[i].fd = objs[i] ? objs[i]->fd : -1;
            fds[i].events = POLLIN;
        }
        if (msgwait)
        {
            fds[count].fd = ntdll_get_thread_data()->esync_queue_fd;
            fds[count].events = POLLIN;
            pollcount++;
        }

        while (1)
        {
            ret = do_poll( fds, pollcount, timeout ? &end : NULL );
            if (ret > 0)
            {
                /* Find out which object triggered the wait. */
                for (i = 0; i < count; i++)
                {
                    if (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
                    {
                        ERR("Polling on fd %d returned %#x.\n", fds[i].fd, fds[i].revents);
                        return STATUS_INVALID_HANDLE;
                    }

                    if (objs[i])
                    {
                        int64_t value;
                        ssize_t size;

                        if (objs[i]->type == ESYNC_MANUAL_EVENT || objs[i]->type == ESYNC_MANUAL_SERVER)
                        {
                            /* Don't grab the object, just check if it's signaled. */
                            if (fds[i].revents & POLLIN)
                            {
                                TRACE("Woken up by handle %p [%d].\n", handles[i], i);
                                return i;
                            }
                        }
                        else
                        {
                            if ((size = read( fds[i].fd, &value, sizeof(value) )) == sizeof(value))
                            {
                                /* We found our object. */
                                TRACE("Woken up by handle %p [%d].\n", handles[i], i);
                                return i;
                            }
                        }
                    }
                }

                if (msgwait && (fds[count].revents & POLLIN))
                {
                    TRACE("Woken up by driver events.\n");
                    return count - 1;
                }

                /* If we got here, someone else stole (or reset, etc.) whatever
                 * we were waiting for. So keep waiting. */
                NtQuerySystemTime( &now );
            }
            else if (ret == 0)
            {
                TRACE("Wait timed out.\n");
                return STATUS_TIMEOUT;
            }
            else
            {
                ERR("ppoll failed: %s\n", strerror(errno));
                return FILE_GetNtStatus();
            }
        }
    }
    else
    {
        FIXME("Wait-all not implemented.\n");
        return STATUS_NOT_IMPLEMENTED;
    }
}
