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

#if 0
#pragma makedep unix
#endif

#define _GNU_SOURCE

#include "config.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#ifdef HAVE_SYS_MMAN_H
# include <sys/mman.h>
#endif
#ifdef HAVE_SYS_POLL_H
# include <sys/poll.h>
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#include <sys/types.h>
#include <unistd.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#define NONAMELESSUNION
#include "windef.h"
#include "winternl.h"
#include "wine/server.h"
#include "wine/debug.h"

#include "unix_private.h"
#include "esync.h"

WINE_DEFAULT_DEBUG_CHANNEL(esync);

int do_esync(void)
{
#ifdef HAVE_SYS_EVENTFD_H
    static int do_esync_cached = -1;

    if (do_esync_cached == -1)
        do_esync_cached = getenv("WINEESYNC") && atoi(getenv("WINEESYNC"));

    return do_esync_cached;
#else
    static int once;
    if (!once++)
        FIXME("eventfd not supported on this platform.\n");
    return 0;
#endif
}

struct esync
{
    enum esync_type type;
    int fd;
    void *shm;
};

struct semaphore
{
    int max;
    int count;
};
C_ASSERT(sizeof(struct semaphore) == 8);

struct event
{
    int signaled;
    int locked;
};
C_ASSERT(sizeof(struct event) == 8);

static char shm_name[29];
static int shm_fd;
static void **shm_addrs;
static int shm_addrs_size;  /* length of the allocated shm_addrs array */
static long pagesize;

static pthread_mutex_t shm_addrs_mutex = PTHREAD_MUTEX_INITIALIZER;

static void *get_shm( unsigned int idx )
{
    int entry  = (idx * 8) / pagesize;
    int offset = (idx * 8) % pagesize;
    void *ret;

    pthread_mutex_lock( &shm_addrs_mutex );

    if (entry >= shm_addrs_size)
    {
        int new_size = max(shm_addrs_size * 2, entry + 1);

        if (!(shm_addrs = realloc( shm_addrs, new_size * sizeof(shm_addrs[0]) )))
            ERR("Failed to grow shm_addrs array to size %d.\n", shm_addrs_size);
        memset( shm_addrs + shm_addrs_size, 0, (new_size - shm_addrs_size) * sizeof(shm_addrs[0]) );
        shm_addrs_size = new_size;
    }

    if (!shm_addrs[entry])
    {
        void *addr = mmap( NULL, pagesize, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, entry * pagesize );
        if (addr == (void *)-1)
            ERR("Failed to map page %d (offset %#lx).\n", entry, entry * pagesize);

        TRACE("Mapping page %d at %p.\n", entry, addr);

        if (InterlockedCompareExchangePointer( &shm_addrs[entry], addr, 0 ))
            munmap( addr, pagesize ); /* someone beat us to it */
    }

    ret = (void *)((unsigned long)shm_addrs[entry] + offset);

    pthread_mutex_unlock( &shm_addrs_mutex );

    return ret;
}

/* We'd like lookup to be fast. To that end, we use a static list indexed by handle.
 * This is copied and adapted from the fd cache code. */

#define ESYNC_LIST_BLOCK_SIZE  (65536 / sizeof(struct esync))
#define ESYNC_LIST_ENTRIES     256

static struct esync *esync_list[ESYNC_LIST_ENTRIES];
static struct esync esync_list_initial_block[ESYNC_LIST_BLOCK_SIZE];

static inline UINT_PTR handle_to_index( HANDLE handle, UINT_PTR *entry )
{
    UINT_PTR idx = (((UINT_PTR)handle) >> 2) - 1;
    *entry = idx / ESYNC_LIST_BLOCK_SIZE;
    return idx % ESYNC_LIST_BLOCK_SIZE;
}

static struct esync *add_to_list( HANDLE handle, enum esync_type type, int fd, void *shm )
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
            void *ptr = anon_mmap_alloc( ESYNC_LIST_BLOCK_SIZE * sizeof(struct esync),
                                         PROT_READ | PROT_WRITE );
            if (ptr == MAP_FAILED) return FALSE;
            esync_list[entry] = ptr;
        }
    }

    if (!InterlockedCompareExchange( (int *)&esync_list[entry][idx].type, type, 0 ))
    {
        esync_list[entry][idx].fd = fd;
        esync_list[entry][idx].shm = shm;
    }
    return &esync_list[entry][idx];
}

static struct esync *get_cached_object( HANDLE handle )
{
    UINT_PTR entry, idx = handle_to_index( handle, &entry );

    if (entry >= ESYNC_LIST_ENTRIES || !esync_list[entry]) return NULL;
    if (!esync_list[entry][idx].type) return NULL;

    return &esync_list[entry][idx];
}

NTSTATUS esync_close( HANDLE handle )
{
    UINT_PTR entry, idx = handle_to_index( handle, &entry );

    TRACE("%p.\n", handle);

    if (entry < ESYNC_LIST_ENTRIES && esync_list[entry])
    {
        if (InterlockedExchange((int *)&esync_list[entry][idx].type, 0))
        {
            close( esync_list[entry][idx].fd );
            return STATUS_SUCCESS;
        }
    }

    return STATUS_INVALID_HANDLE;
}

static NTSTATUS create_esync( enum esync_type type, HANDLE *handle, ACCESS_MASK access,
                              const OBJECT_ATTRIBUTES *attr, int initval, int max )
{
    NTSTATUS ret;
    data_size_t len;
    struct object_attributes *objattr;
    obj_handle_t fd_handle;
    unsigned int shm_idx;
    sigset_t sigset;
    int fd;

    if ((ret = alloc_object_attributes( attr, &objattr, &len ))) return ret;

    /* We have to synchronize on the fd cache CS so that our calls to
     * receive_fd don't race with theirs. */
    server_enter_uninterrupted_section( &fd_cache_mutex, &sigset );
    SERVER_START_REQ( create_esync )
    {
        req->access  = access;
        req->initval = initval;
        req->type    = type;
        req->max     = max;
        wine_server_add_data( req, objattr, len );
        ret = wine_server_call( req );
        if (!ret || ret == STATUS_OBJECT_NAME_EXISTS)
        {
            *handle = wine_server_ptr_handle( reply->handle );
            type = reply->type;
            shm_idx = reply->shm_idx;
            fd = receive_fd( &fd_handle );
            assert( wine_server_ptr_handle(fd_handle) == *handle );
        }
    }
    SERVER_END_REQ;
    server_leave_uninterrupted_section( &fd_cache_mutex, &sigset );

    if (!ret || ret == STATUS_OBJECT_NAME_EXISTS)
    {
        add_to_list( *handle, type, fd, shm_idx ? get_shm( shm_idx ) : 0 );
        TRACE("-> handle %p, fd %d.\n", *handle, fd);
    }

    free( objattr );
    return ret;
}

extern NTSTATUS esync_create_semaphore(HANDLE *handle, ACCESS_MASK access,
    const OBJECT_ATTRIBUTES *attr, LONG initial, LONG max)
{
    TRACE("name %s, initial %d, max %d.\n",
        attr ? debugstr_us(attr->ObjectName) : "<no name>", initial, max);

    return create_esync( ESYNC_SEMAPHORE, handle, access, attr, initial, max );
}

NTSTATUS esync_release_semaphore( HANDLE handle, ULONG count, ULONG *prev )
{
    struct esync *obj;
    struct semaphore *semaphore;
    uint64_t count64 = count;
    ULONG current;

    TRACE("%p, %d, %p.\n", handle, count, prev);

    if (!(obj = get_cached_object( handle ))) return STATUS_INVALID_HANDLE;
    semaphore = obj->shm;

    do
    {
        current = semaphore->count;

        if (count + current > semaphore->max)
            return STATUS_SEMAPHORE_LIMIT_EXCEEDED;
    } while (InterlockedCompareExchange( &semaphore->count, count + current, current ) != current);

    if (prev) *prev = current;

    /* We don't have to worry about a race between increasing the count and
     * write(). The fact that we were able to increase the count means that we
     * have permission to actually write that many releases to the semaphore. */

    if (write( obj->fd, &count64, sizeof(count64) ) == -1)
        return errno_to_status( errno );

    return STATUS_SUCCESS;
}

NTSTATUS esync_create_event( HANDLE *handle, ACCESS_MASK access,
    const OBJECT_ATTRIBUTES *attr, EVENT_TYPE event_type, BOOLEAN initial )
{
    enum esync_type type = (event_type == SynchronizationEvent ? ESYNC_AUTO_EVENT : ESYNC_MANUAL_EVENT);

    TRACE("name %s, %s-reset, initial %d.\n",
        attr ? debugstr_us(attr->ObjectName) : "<no name>",
        event_type == NotificationEvent ? "manual" : "auto", initial);

    return create_esync( type, handle, access, attr, initial, 0 );
}

NTSTATUS esync_set_event( HANDLE handle )
{
    static const uint64_t value = 1;
    struct esync *obj;

    TRACE("%p.\n", handle);

    if (!(obj = get_cached_object( handle ))) return STATUS_INVALID_HANDLE;

    if (write( obj->fd, &value, sizeof(value) ) == -1)
        ERR("write: %s\n", strerror(errno));

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
    if (end)
    {
        LONGLONG timeleft = update_timeout( *end );

#ifdef HAVE_PPOLL
        /* We use ppoll() if available since the time granularity is better. */
        struct timespec tmo_p;
        tmo_p.tv_sec = timeleft / (ULONGLONG)TICKSPERSEC;
        tmo_p.tv_nsec = (timeleft % TICKSPERSEC) * 100;
        return ppoll( fds, nfds, &tmo_p, NULL );
#else
        return poll( fds, nfds, timeleft / TICKSPERMSEC );
#endif
    }
    else
        return poll( fds, nfds, -1 );
}

static void update_grabbed_object( struct esync *obj )
{
    if (obj->type == ESYNC_SEMAPHORE)
    {
        struct semaphore *semaphore = obj->shm;
        /* We don't have to worry about a race between this and read(); the
         * fact that we were able to grab it at all means the count is nonzero,
         * and if someone else grabbed it then the count must have been >= 2,
         * etc. */
        InterlockedExchangeAdd( &semaphore->count, -1 );
    }
    else if (obj->type == ESYNC_AUTO_EVENT)
    {
        struct event *event = obj->shm;
        /* We don't have to worry about a race between this and read(), since
         * this is just a hint, and the real state is in the kernel object.
         * This might already be 0, but that's okay! */
        event->signaled = 0;
    }
}

/* A value of STATUS_NOT_IMPLEMENTED returned from this function means that we
 * need to delegate to server_select(). */
NTSTATUS esync_wait_objects( DWORD count, const HANDLE *handles, BOOLEAN wait_any,
                             BOOLEAN alertable, const LARGE_INTEGER *timeout )
{
    struct esync *objs[MAXIMUM_WAIT_OBJECTS];
    struct pollfd fds[MAXIMUM_WAIT_OBJECTS];
    int has_esync = 0, has_server = 0;
    LONGLONG timeleft;
    LARGE_INTEGER now;
    ULONGLONG end;
    int i, ret;

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
        if ((objs[i] = get_cached_object( handles[i] )))
            has_esync = 1;
        else
            has_server = 1;
    }

    if (has_esync && has_server)
        FIXME("Can't wait on esync and server objects at the same time!\n");
    else if (has_server)
        return STATUS_NOT_IMPLEMENTED;

    if (TRACE_ON(esync))
    {
        TRACE("Waiting for %s of %d handles:", wait_any ? "any" : "all", count);
        for (i = 0; i < count; i++)
            TRACE(" %p", handles[i]);

        if (!timeout)
            TRACE(", timeout = INFINITE.\n");
        else
        {
            timeleft = update_timeout( end );
            TRACE(", timeout = %ld.%07ld sec.\n",
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

        while (1)
        {
            ret = do_poll( fds, count, timeout ? &end : NULL );
            if (ret > 0)
            {
                /* Find out which object triggered the wait. */
                for (i = 0; i < count; i++)
                {
                    struct esync *obj = objs[i];

                    if (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
                    {
                        ERR("Polling on fd %d returned %#x.\n", fds[i].fd, fds[i].revents);
                        return STATUS_INVALID_HANDLE;
                    }

                    if (obj)
                    {
                        int64_t value;
                        ssize_t size;

                        if ((size = read( fds[i].fd, &value, sizeof(value) )) == sizeof(value))
                        {
                            /* We found our object. */
                            TRACE("Woken up by handle %p [%d].\n", handles[i], i);
                            update_grabbed_object( obj );
                            return i;
                        }
                    }
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
                ERR("ppoll failed: %s\n", strerror( errno ));
                return errno_to_status( errno );
            }
        }
    }
    else
    {
        FIXME("Wait-all not implemented.\n");
        return STATUS_NOT_IMPLEMENTED;
    }
}

void esync_init(void)
{
    struct stat st;

    if (!do_esync())
    {
        /* make sure the server isn't running with WINEESYNC */
        HANDLE handle;
        NTSTATUS ret;

        ret = create_esync( 0, &handle, 0, NULL, 0, 0 );
        if (ret != STATUS_NOT_IMPLEMENTED)
        {
            ERR("Server is running with WINEESYNC but this process is not, please enable WINEESYNC or restart wineserver.\n");
            exit(1);
        }

        return;
    }

    if (stat( config_dir, &st ) == -1)
        ERR("Cannot stat %s\n", config_dir);

    if (st.st_ino != (unsigned long)st.st_ino)
        sprintf( shm_name, "/wine-%lx%08lx-esync", (unsigned long)((unsigned long long)st.st_ino >> 32), (unsigned long)st.st_ino );
    else
        sprintf( shm_name, "/wine-%lx-esync", (unsigned long)st.st_ino );

    if ((shm_fd = shm_open( shm_name, O_RDWR, 0644 )) == -1)
    {
        /* probably the server isn't running with WINEESYNC, tell the user and bail */
        if (errno == ENOENT)
            ERR("Failed to open esync shared memory file; make sure no stale wineserver instances are running without WINEESYNC.\n");
        else
            ERR("Failed to initialize shared memory: %s\n", strerror( errno ));
        exit(1);
    }

    pagesize = sysconf( _SC_PAGESIZE );

    shm_addrs = calloc( 128, sizeof(shm_addrs[0]) );
    shm_addrs_size = 128;
}
