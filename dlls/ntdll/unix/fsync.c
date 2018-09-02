/*
 * futex-based synchronization objects
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

#include "config.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef HAVE_SYS_SYSCALL_H
# include <sys/syscall.h>
#endif
#include <unistd.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#define NONAMELESSUNION
#include "windef.h"
#include "winternl.h"
#include "wine/debug.h"
#include "wine/server.h"

#include "unix_private.h"
#include "fsync.h"

WINE_DEFAULT_DEBUG_CHANNEL(fsync);

#include "pshpack4.h"
struct futex_wait_block
{
    int *addr;
#if __SIZEOF_POINTER__ == 4
    int pad;
#endif
    int val;
};
#include "poppack.h"

static inline int futex_wait_multiple( const struct futex_wait_block *futexes,
        int count, const struct timespec *timeout )
{
    return syscall( __NR_futex, futexes, 13, count, timeout, 0, 0 );
}

static inline int futex_wake( int *addr, int val )
{
    return syscall( __NR_futex, addr, 1, val, NULL, 0, 0 );
}

int do_fsync(void)
{
#ifdef __linux__
    static int do_fsync_cached = -1;

    if (do_fsync_cached == -1)
    {
        static const struct timespec zero;
        futex_wait_multiple( NULL, 0, &zero );
        do_fsync_cached = getenv("WINEFSYNC") && atoi(getenv("WINEFSYNC")) && errno != ENOSYS;
    }

    return do_fsync_cached;
#else
    static int once;
    if (!once++)
        FIXME("futexes not supported on this platform.\n");
    return 0;
#endif
}

struct fsync
{
    enum fsync_type type;
    void *shm;              /* pointer to shm section */
};

struct semaphore
{
    int count;
    int max;
};
C_ASSERT(sizeof(struct semaphore) == 8);

struct event
{
    int signaled;
    int unused;
};
C_ASSERT(sizeof(struct event) == 8);

struct mutex
{
    int tid;
    int count;  /* recursion count */
};
C_ASSERT(sizeof(struct mutex) == 8);

static char shm_name[29];
static int shm_fd;
static void **shm_addrs;
static int shm_addrs_size;  /* length of the allocated shm_addrs array */
static long pagesize;

static void *get_shm( unsigned int idx )
{
    int entry  = (idx * 8) / pagesize;
    int offset = (idx * 8) % pagesize;

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

        if (__sync_val_compare_and_swap( &shm_addrs[entry], 0, addr ))
            munmap( addr, pagesize ); /* someone beat us to it */
    }

    return (void *)((unsigned long)shm_addrs[entry] + offset);
}

/* We'd like lookup to be fast. To that end, we use a static list indexed by handle.
 * This is copied and adapted from the fd cache code. */

#define FSYNC_LIST_BLOCK_SIZE  (65536 / sizeof(struct fsync))
#define FSYNC_LIST_ENTRIES     256

static struct fsync *fsync_list[FSYNC_LIST_ENTRIES];
static struct fsync fsync_list_initial_block[FSYNC_LIST_BLOCK_SIZE];

static inline UINT_PTR handle_to_index( HANDLE handle, UINT_PTR *entry )
{
    UINT_PTR idx = (((UINT_PTR)handle) >> 2) - 1;
    *entry = idx / FSYNC_LIST_BLOCK_SIZE;
    return idx % FSYNC_LIST_BLOCK_SIZE;
}

static struct fsync *add_to_list( HANDLE handle, enum fsync_type type, void *shm )
{
    UINT_PTR entry, idx = handle_to_index( handle, &entry );

    if (entry >= FSYNC_LIST_ENTRIES)
    {
        FIXME( "too many allocated handles, not caching %p\n", handle );
        return FALSE;
    }

    if (!fsync_list[entry])  /* do we need to allocate a new block of entries? */
    {
        if (!entry) fsync_list[0] = fsync_list_initial_block;
        else
        {
            void *ptr = anon_mmap_alloc( FSYNC_LIST_BLOCK_SIZE * sizeof(struct fsync),
                                         PROT_READ | PROT_WRITE );
            if (ptr == MAP_FAILED) return FALSE;
            fsync_list[entry] = ptr;
        }
    }

    if (!__sync_val_compare_and_swap((int *)&fsync_list[entry][idx].type, 0, type ))
        fsync_list[entry][idx].shm = shm;

    return &fsync_list[entry][idx];
}

static struct fsync *get_cached_object( HANDLE handle )
{
    UINT_PTR entry, idx = handle_to_index( handle, &entry );

    if (entry >= FSYNC_LIST_ENTRIES || !fsync_list[entry]) return NULL;
    if (!fsync_list[entry][idx].type) return NULL;

    return &fsync_list[entry][idx];
}

/* Gets an object. This is either a proper fsync object (i.e. an event,
 * semaphore, etc. created using create_fsync) or a generic synchronizable
 * server-side object which the server will signal (e.g. a process, thread,
 * message queue, etc.) */
static NTSTATUS get_object( HANDLE handle, struct fsync **obj )
{
    NTSTATUS ret = STATUS_SUCCESS;
    unsigned int shm_idx = 0;
    enum fsync_type type;

    if ((*obj = get_cached_object( handle ))) return STATUS_SUCCESS;

    if ((INT_PTR)handle < 0)
    {
        /* We can deal with pseudo-handles, but it's just easier this way */
        return STATUS_NOT_IMPLEMENTED;
    }

    /* We need to try grabbing it from the server. */
    SERVER_START_REQ( get_fsync_idx )
    {
        req->handle = wine_server_obj_handle( handle );
        if (!(ret = wine_server_call( req )))
        {
            shm_idx = reply->shm_idx;
            type    = reply->type;
        }
    }
    SERVER_END_REQ;

    if (ret)
    {
        WARN("Failed to retrieve shm index for handle %p, status %#x.\n", handle, ret);
        *obj = NULL;
        return ret;
    }

    TRACE("Got shm index %d for handle %p.\n", shm_idx, handle);

    *obj = add_to_list( handle, type, get_shm( shm_idx ) );
    return ret;
}

NTSTATUS fsync_close( HANDLE handle )
{
    UINT_PTR entry, idx = handle_to_index( handle, &entry );

    TRACE("%p.\n", handle);

    if (entry < FSYNC_LIST_ENTRIES && fsync_list[entry])
    {
        if (__atomic_exchange_n( &fsync_list[entry][idx].type, 0, __ATOMIC_SEQ_CST ))
            return STATUS_SUCCESS;
    }

    return STATUS_INVALID_HANDLE;
}

static NTSTATUS create_fsync( enum fsync_type type, HANDLE *handle,
    ACCESS_MASK access, const OBJECT_ATTRIBUTES *attr, int low, int high )
{
    NTSTATUS ret;
    data_size_t len;
    struct object_attributes *objattr;
    unsigned int shm_idx;

    if ((ret = alloc_object_attributes( attr, &objattr, &len ))) return ret;

    SERVER_START_REQ( create_fsync )
    {
        req->access = access;
        req->low    = low;
        req->high   = high;
        wine_server_add_data( req, objattr, len );
        ret = wine_server_call( req );
        if (!ret || ret == STATUS_OBJECT_NAME_EXISTS)
        {
            *handle = wine_server_ptr_handle( reply->handle );
            shm_idx = reply->shm_idx;
        }
    }
    SERVER_END_REQ;

    if (!ret || ret == STATUS_OBJECT_NAME_EXISTS)
    {
        add_to_list( *handle, type, get_shm( shm_idx ));
        TRACE("-> handle %p, shm index %d.\n", *handle, shm_idx);
    }

    free( objattr );
    return ret;
}

void fsync_init(void)
{
    struct stat st;

    if (!do_fsync())
    {
        /* make sure the server isn't running with WINEFSYNC */
        HANDLE handle;
        NTSTATUS ret;

        ret = create_fsync( 0, &handle, 0, NULL, 0, 0 );
        if (ret != STATUS_NOT_IMPLEMENTED)
        {
            ERR("Server is running with WINEFSYNC but this process is not, please enable WINEFSYNC or restart wineserver.\n");
            exit(1);
        }

        return;
    }

    if (stat( config_dir, &st ) == -1)
        ERR("Cannot stat %s\n", config_dir);

    if (st.st_ino != (unsigned long)st.st_ino)
        sprintf( shm_name, "/wine-%lx%08lx-fsync", (unsigned long)((unsigned long long)st.st_ino >> 32), (unsigned long)st.st_ino );
    else
        sprintf( shm_name, "/wine-%lx-fsync", (unsigned long)st.st_ino );

    if ((shm_fd = shm_open( shm_name, O_RDWR, 0644 )) == -1)
    {
        /* probably the server isn't running with WINEFSYNC, tell the user and bail */
        if (errno == ENOENT)
            ERR("Failed to open fsync shared memory file; make sure no stale wineserver instances are running without WINEFSYNC.\n");
        else
            ERR("Failed to initialize shared memory: %s\n", strerror( errno ));
        exit(1);
    }

    pagesize = sysconf( _SC_PAGESIZE );

    shm_addrs = calloc( 128, sizeof(shm_addrs[0]) );
    shm_addrs_size = 128;
}

NTSTATUS fsync_create_semaphore( HANDLE *handle, ACCESS_MASK access,
    const OBJECT_ATTRIBUTES *attr, LONG initial, LONG max )
{
    TRACE("name %s, initial %d, max %d.\n",
        attr ? debugstr_us(attr->ObjectName) : "<no name>", initial, max);

    return create_fsync( FSYNC_SEMAPHORE, handle, access, attr, initial, max );
}

NTSTATUS fsync_release_semaphore( HANDLE handle, ULONG count, ULONG *prev )
{
    struct fsync *obj;
    struct semaphore *semaphore;
    ULONG current;

    TRACE("%p, %d, %p.\n", handle, count, prev);

    if (!(obj = get_cached_object( handle ))) return STATUS_INVALID_HANDLE;
    semaphore = obj->shm;

    do
    {
        current = semaphore->count;
        if (count + current > semaphore->max)
            return STATUS_SEMAPHORE_LIMIT_EXCEEDED;
    } while (__sync_val_compare_and_swap( &semaphore->count, current, count + current ) != current);

    if (prev) *prev = current;

    if (!current)
        futex_wake( &semaphore->count, count );

    return STATUS_SUCCESS;
}

NTSTATUS fsync_create_event( HANDLE *handle, ACCESS_MASK access,
    const OBJECT_ATTRIBUTES *attr, EVENT_TYPE event_type, BOOLEAN initial )
{
    enum fsync_type type = (event_type == SynchronizationEvent ? FSYNC_AUTO_EVENT : FSYNC_MANUAL_EVENT);

    TRACE("name %s, %s-reset, initial %d.\n",
        attr ? debugstr_us(attr->ObjectName) : "<no name>",
        event_type == NotificationEvent ? "manual" : "auto", initial);

    return create_fsync( type, handle, access, attr, initial, 0xdeadbeef );
}

NTSTATUS fsync_set_event( HANDLE handle, LONG *prev )
{
    struct event *event;
    struct fsync *obj;
    LONG current;

    TRACE("%p.\n", handle);

    if (!(obj = get_cached_object( handle ))) return STATUS_INVALID_HANDLE;
    event = obj->shm;

    if (!(current = __atomic_exchange_n( &event->signaled, 1, __ATOMIC_SEQ_CST )))
        futex_wake( &event->signaled, obj->type == FSYNC_AUTO_EVENT ? 1 : INT_MAX );

    if (prev) *prev = current;

    return STATUS_SUCCESS;
}

NTSTATUS fsync_reset_event( HANDLE handle, LONG *prev )
{
    struct event *event;
    struct fsync *obj;
    LONG current;

    TRACE("%p.\n", handle);

    if (!(obj = get_cached_object( handle ))) return STATUS_INVALID_HANDLE;
    event = obj->shm;

    current = __atomic_exchange_n( &event->signaled, 0, __ATOMIC_SEQ_CST );

    if (prev) *prev = current;

    return STATUS_SUCCESS;
}

NTSTATUS fsync_create_mutex( HANDLE *handle, ACCESS_MASK access,
    const OBJECT_ATTRIBUTES *attr, BOOLEAN initial )
{
    TRACE("name %s, initial %d.\n",
        attr ? debugstr_us(attr->ObjectName) : "<no name>", initial);

    return create_fsync( FSYNC_MUTEX, handle, access, attr,
        initial ? GetCurrentThreadId() : 0, initial ? 1 : 0 );
}

NTSTATUS fsync_release_mutex( HANDLE handle, LONG *prev )
{
    struct mutex *mutex;
    struct fsync *obj;

    TRACE("%p, %p.\n", handle, prev);

    if (!(obj = get_cached_object( handle ))) return STATUS_INVALID_HANDLE;
    mutex = obj->shm;

    if (mutex->tid != GetCurrentThreadId()) return STATUS_MUTANT_NOT_OWNED;

    if (prev) *prev = mutex->count;

    if (!--mutex->count)
    {
        __atomic_store_n( &mutex->tid, 0, __ATOMIC_SEQ_CST );
        futex_wake( &mutex->tid, 1 );
    }

    return STATUS_SUCCESS;
}

static LONGLONG update_timeout( ULONGLONG end )
{
    LARGE_INTEGER now;
    LONGLONG timeleft;

    NtQuerySystemTime( &now );
    timeleft = end - now.QuadPart;
    if (timeleft < 0) timeleft = 0;
    return timeleft;
}

static NTSTATUS __fsync_wait_objects( DWORD count, const HANDLE *handles,
    BOOLEAN wait_any, BOOLEAN alertable, const LARGE_INTEGER *timeout )
{
    struct futex_wait_block futexes[MAXIMUM_WAIT_OBJECTS];
    struct fsync *objs[MAXIMUM_WAIT_OBJECTS];
    int has_fsync = 0, has_server = 0;
    BOOL msgwait = FALSE;
    int dummy_futex = 0;
    LONGLONG timeleft;
    LARGE_INTEGER now;
    ULONGLONG end;
    int i, ret;

    NtQuerySystemTime( &now );
    if (timeout)
    {
        if (timeout->QuadPart == TIMEOUT_INFINITE)
            timeout = NULL;
        else if (timeout->QuadPart > 0)
            end = timeout->QuadPart;
        else
            end = now.QuadPart - timeout->QuadPart;
    }

    for (i = 0; i < count; i++)
    {
        ret = get_object( handles[i], &objs[i] );
        if (ret == STATUS_SUCCESS)
            has_fsync = 1;
        else if (ret == STATUS_NOT_IMPLEMENTED)
            has_server = 1;
        else
            return ret;
    }

    if (objs[count - 1] && objs[count - 1]->type == FSYNC_QUEUE)
        msgwait = TRUE;

    if (has_fsync && has_server)
        FIXME("Can't wait on fsync and server objects at the same time!\n");
    else if (has_server)
        return STATUS_NOT_IMPLEMENTED;

    if (TRACE_ON(fsync))
    {
        TRACE("Waiting for %s of %d handles:", wait_any ? "any" : "all", count);
        for (i = 0; i < count; i++)
            TRACE(" %p", handles[i]);

        if (msgwait)
            TRACE(" or driver events");

        if (!timeout)
            TRACE(", timeout = INFINITE.\n");
        else
        {
            timeleft = update_timeout( end );
            TRACE(", timeout = %ld.%07ld sec.\n",
                (long) (timeleft / TICKSPERSEC), (long) (timeleft % TICKSPERSEC));
        }
    }

    if (wait_any || count == 1)
    {
        while (1)
        {
            /* Try to grab anything. */

            for (i = 0; i < count; i++)
            {
                struct fsync *obj = objs[i];

                if (obj)
                {
                    switch (obj->type)
                    {
                    case FSYNC_SEMAPHORE:
                    {
                        struct semaphore *semaphore = obj->shm;
                        int current;

                        do
                        {
                            if (!(current = semaphore->count)) break;
                        } while (__sync_val_compare_and_swap( &semaphore->count, current, current - 1 ) != current);

                        if (current)
                        {
                            TRACE("Woken up by handle %p [%d].\n", handles[i], i);
                            return i;
                        }

                        futexes[i].addr = &semaphore->count;
                        futexes[i].val = current;
                        break;
                    }
                    case FSYNC_MUTEX:
                    {
                        struct mutex *mutex = obj->shm;

                        if (mutex->tid == GetCurrentThreadId())
                        {
                            TRACE("Woken up by handle %p [%d].\n", handles[i], i);
                            mutex->count++;
                            return i;
                        }

                        if (!__sync_val_compare_and_swap( &mutex->tid, 0, GetCurrentThreadId() ))
                        {
                            TRACE("Woken up by handle %p [%d].\n", handles[i], i);
                            mutex->count = 1;
                            return i;
                        }

                        futexes[i].addr = &mutex->tid;
                        futexes[i].val  = mutex->tid;
                        break;
                    }
                    case FSYNC_AUTO_EVENT:
                    {
                        struct event *event = obj->shm;

                        if (__sync_val_compare_and_swap( &event->signaled, 1, 0 ))
                        {
                            TRACE("Woken up by handle %p [%d].\n", handles[i], i);
                            return i;
                        }

                        futexes[i].addr = &event->signaled;
                        futexes[i].val = 0;
                        break;
                    }
                    case FSYNC_MANUAL_EVENT:
                    case FSYNC_MANUAL_SERVER:
                    case FSYNC_QUEUE:
                    {
                        struct event *event = obj->shm;

                        if (__atomic_load_n( &event->signaled, __ATOMIC_SEQ_CST ))
                        {
                            TRACE("Woken up by handle %p [%d].\n", handles[i], i);
                            return i;
                        }

                        futexes[i].addr = &event->signaled;
                        futexes[i].val = 0;
                        break;
                    }
                    default:
                        assert(0);
                    }
                }
                else
                {
                    /* Avoid breaking things entirely. */
                    futexes[i].addr = &dummy_futex;
                    futexes[i].val = dummy_futex;
                }

#if __SIZEOF_POINTER__ == 4
                futexes[i].pad = 0;
#endif
            }

            /* Looks like everything is contended, so wait. */

            if (timeout)
            {
                LONGLONG timeleft = update_timeout( end );
                struct timespec tmo_p;
                tmo_p.tv_sec = timeleft / (ULONGLONG)TICKSPERSEC;
                tmo_p.tv_nsec = (timeleft % TICKSPERSEC) * 100;

                ret = futex_wait_multiple( futexes, count, &tmo_p );
            }
            else
                ret = futex_wait_multiple( futexes, count, NULL );

            /* FUTEX_WAIT_MULTIPLE can succeed or return -EINTR, -EAGAIN,
             * -EFAULT/-EACCES, -ETIMEDOUT. In the first three cases we need to
             * try again, bad address is already handled by the fact that we
             * tried to read from it, so only break out on a timeout. */
            if (ret == -1 && errno == ETIMEDOUT)
            {
                TRACE("Wait timed out.\n");
                return STATUS_TIMEOUT;
            }
        } /* while (1) */
    }
    else
    {
        FIXME("Wait-all not implemented.\n");
        return STATUS_NOT_IMPLEMENTED;
    }
}

/* Like esync, we need to let the server know when we are doing a message wait,
 * and when we are done with one, so that all of the code surrounding hung
 * queues works, and we also need this for WaitForInputIdle().
 *
 * Unlike esync, we can't wait on the queue fd itself locally. Instead we let
 * the server do that for us, the way it normally does. This could actually
 * work for esync too, and that might be better. */
static void server_set_msgwait( int in_msgwait )
{
    SERVER_START_REQ( fsync_msgwait )
    {
        req->in_msgwait = in_msgwait;
        wine_server_call( req );
    }
    SERVER_END_REQ;
}

/* This is a very thin wrapper around the proper implementation above. The
 * purpose is to make sure the server knows when we are doing a message wait.
 * This is separated into a wrapper function since there are at least a dozen
 * exit paths from fsync_wait_objects(). */
NTSTATUS fsync_wait_objects( DWORD count, const HANDLE *handles, BOOLEAN wait_any,
                             BOOLEAN alertable, const LARGE_INTEGER *timeout )
{
    BOOL msgwait = FALSE;
    struct fsync *obj;
    NTSTATUS ret;

    if (!get_object( handles[count - 1], &obj ) && obj->type == FSYNC_QUEUE)
    {
        msgwait = TRUE;
        server_set_msgwait( 1 );
    }

    ret = __fsync_wait_objects( count, handles, wait_any, alertable, timeout );

    if (msgwait)
        server_set_msgwait( 0 );

    return ret;
}
