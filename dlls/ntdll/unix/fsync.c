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
#ifdef HAVE_LINUX_FUTEX_H
# include <linux/futex.h>
#endif
#include <unistd.h>
#include <stdint.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winternl.h"
#include "wine/debug.h"
#include "wine/server.h"

#include "unix_private.h"
#include "fsync.h"

WINE_DEFAULT_DEBUG_CHANNEL(fsync);

#include "pshpack4.h"
#include "poppack.h"

static int current_pid;

/* futex_waitv interface */

#ifndef __NR_futex_waitv

# define __NR_futex_waitv 449
# define FUTEX_32 2
struct futex_waitv {
    uint64_t   val;
    uint64_t   uaddr;
    uint32_t   flags;
    uint32_t __reserved;
};

#endif

#define u64_to_ptr(x) (void *)(uintptr_t)(x)

struct timespec64
{
    long long tv_sec;
    long long tv_nsec;
};

static LONGLONG nt_time_from_ts( struct timespec *ts )
{
    return ticks_from_time_t( ts->tv_sec ) + (ts->tv_nsec + 50) / 100;
}

static void get_wait_end_time( const LARGE_INTEGER **timeout, struct timespec64 *end, clockid_t *clock_id )
{
    ULONGLONG nt_end;

    if (!*timeout) return;
    if ((*timeout)->QuadPart == TIMEOUT_INFINITE)
    {
        *timeout = NULL;
        return;
    }

    if ((*timeout)->QuadPart > 0)
    {
        nt_end = (*timeout)->QuadPart;
        *clock_id = CLOCK_REALTIME;
    }
    else
    {
        struct timespec ts;

        clock_gettime( CLOCK_MONOTONIC, &ts );
        nt_end = nt_time_from_ts( &ts ) - (*timeout)->QuadPart;
        *clock_id = CLOCK_MONOTONIC;
    }

    nt_end -= SECS_1601_TO_1970 * TICKSPERSEC;
    end->tv_sec = nt_end / (ULONGLONG)TICKSPERSEC;
    end->tv_nsec = (nt_end % TICKSPERSEC) * 100;
}

static LONGLONG update_timeout( const struct timespec64 *end, clockid_t clock_id )
{
    struct timespec end_ts, ts;
    LONGLONG timeleft;

    clock_gettime( clock_id, &ts );
    end_ts.tv_sec = end->tv_sec;
    end_ts.tv_nsec = end->tv_nsec;
    timeleft = nt_time_from_ts( &end_ts ) - nt_time_from_ts( &ts );
    if (timeleft < 0) timeleft = 0;
    return timeleft;
}

static inline void futex_vector_set( struct futex_waitv *waitv, int *addr, int val )
{
    waitv->uaddr = (uintptr_t) addr;
    waitv->val = val;
    waitv->flags = FUTEX_32;
    waitv->__reserved = 0;
}

static void simulate_sched_quantum(void)
{
    if (!fsync_simulate_sched_quantum) return;
    /* futex wait is often very quick to resume a waiting thread when woken.
     * That reveals synchonization bugs in some games which happen to work on
     * Windows due to the waiting threads having some minimal delay to wake up. */
    usleep(0);
}

static inline int futex_wait_multiple( const struct futex_waitv *futexes,
        int count, const struct timespec64 *end, clockid_t clock_id )
{
   if (end)
        return syscall( __NR_futex_waitv, futexes, count, 0, end, clock_id );
   else
        return syscall( __NR_futex_waitv, futexes, count, 0, NULL, 0 );
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
        syscall( __NR_futex_waitv, NULL, 0, 0, NULL, 0 );
        do_fsync_cached = getenv("WINEFSYNC") && atoi(getenv("WINEFSYNC")) && errno != ENOSYS && errno != EPERM;
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
    int ref;
    int last_pid;
};
C_ASSERT(sizeof(struct semaphore) == 16);

struct event
{
    int signaled;
    int unused;
    int ref;
    int last_pid;
};
C_ASSERT(sizeof(struct event) == 16);

struct mutex
{
    int tid;
    int count;  /* recursion count */
    int ref;
    int last_pid;
};
C_ASSERT(sizeof(struct mutex) == 16);

static char shm_name[29];
static int shm_fd;
static volatile void *shm_addrs[8192];

static void *get_shm( unsigned int idx )
{
    int entry  = (idx * 16) / FSYNC_SHM_PAGE_SIZE;
    int offset = (idx * 16) % FSYNC_SHM_PAGE_SIZE;

    if (entry >= ARRAY_SIZE(shm_addrs))
    {
        ERR( "idx %u exceeds maximum of %u.\n", idx,
             (unsigned int)ARRAY_SIZE(shm_addrs) * (FSYNC_SHM_PAGE_SIZE / 16) );
        return NULL;
    }

    if (!shm_addrs[entry])
    {
        void *addr = mmap( NULL, FSYNC_SHM_PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd,
                           (off_t)entry * FSYNC_SHM_PAGE_SIZE );
        if (addr == (void *)-1)
            ERR("Failed to map page %d (offset %s).\n", entry,
                 wine_dbgstr_longlong((off_t)entry * FSYNC_SHM_PAGE_SIZE));

        TRACE("Mapping page %d at %p.\n", entry, addr);

        if (__sync_val_compare_and_swap( &shm_addrs[entry], 0, addr ))
            munmap( addr, FSYNC_SHM_PAGE_SIZE ); /* someone beat us to it */
    }

    return (char *)shm_addrs[entry] + offset;
}

/* We'd like lookup to be fast. To that end, we use a static list indexed by handle.
 * This is copied and adapted from the fd cache code. */

#define FSYNC_LIST_BLOCK_SIZE  (65536 / sizeof(struct fsync))
#define FSYNC_LIST_ENTRIES     256

struct fsync_cache
{
    enum fsync_type type;
    unsigned int shm_idx;
};

C_ASSERT(sizeof(struct fsync_cache) == sizeof(uint64_t));

static struct fsync_cache *fsync_list[FSYNC_LIST_ENTRIES];
static struct fsync_cache fsync_list_initial_block[FSYNC_LIST_BLOCK_SIZE];

static inline UINT_PTR handle_to_index( HANDLE handle, UINT_PTR *entry )
{
    UINT_PTR idx = (((UINT_PTR)handle) >> 2) - 1;
    *entry = idx / FSYNC_LIST_BLOCK_SIZE;
    return idx % FSYNC_LIST_BLOCK_SIZE;
}

static void add_to_list( HANDLE handle, enum fsync_type type, unsigned int shm_idx )
{
    UINT_PTR entry, idx = handle_to_index( handle, &entry );
    struct fsync_cache cache;

    if (entry >= FSYNC_LIST_ENTRIES)
    {
        FIXME( "too many allocated handles, not caching %p\n", handle );
        return;
    }

    if (!fsync_list[entry])  /* do we need to allocate a new block of entries? */
    {
        if (!entry) fsync_list[0] = fsync_list_initial_block;
        else
        {
            void *ptr = anon_mmap_alloc( FSYNC_LIST_BLOCK_SIZE * sizeof(*fsync_list[entry]),
                                         PROT_READ | PROT_WRITE );
            if (ptr == MAP_FAILED) return;
            if (__sync_val_compare_and_swap( &fsync_list[entry], NULL, ptr ))
                munmap( ptr, FSYNC_LIST_BLOCK_SIZE * sizeof(*fsync_list[entry]) );
        }
    }

    cache.type = type;
    cache.shm_idx = shm_idx;
    __atomic_store_n( (uint64_t *)&fsync_list[entry][idx], *(uint64_t *)&cache, __ATOMIC_SEQ_CST );
}

static void grab_object( struct fsync *obj )
{
    int *shm = obj->shm;

    __atomic_add_fetch( &shm[2], 1, __ATOMIC_SEQ_CST );
}

static unsigned int shm_index_from_shm( char *shm )
{
    unsigned int i, idx_offset;

    for (i = 0; i < ARRAY_SIZE(shm_addrs); ++i)
    {
        if (shm >= (char *)shm_addrs[i] && shm < (char *)shm_addrs[i] + FSYNC_SHM_PAGE_SIZE)
        {
            idx_offset = (shm - (char *)shm_addrs[i]) / 16;
            return i * (FSYNC_SHM_PAGE_SIZE / 16) + idx_offset;
        }
    }

    ERR( "Index for shm %p not found.\n", shm );
    return ~0u;
}

static void put_object( struct fsync *obj )
{
    int *shm = obj->shm;

    if (__atomic_load_n( &shm[2], __ATOMIC_SEQ_CST ) == 1)
    {
        /* We are holding the last reference, it should be released on server so shm idx get freed. */
        SERVER_START_REQ( fsync_free_shm_idx )
        {
            req->shm_idx = shm_index_from_shm( obj->shm );
            wine_server_call( req );
        }
        SERVER_END_REQ;
    }
    else
    {
        __atomic_sub_fetch( &shm[2], 1, __ATOMIC_SEQ_CST );
    }
}

static void put_object_from_wait( struct fsync *obj )
{
    int *shm = obj->shm;

    __sync_val_compare_and_swap( &shm[3], current_pid, 0 );
    put_object( obj );
}

static BOOL get_cached_object( HANDLE handle, struct fsync *obj )
{
    UINT_PTR entry, idx = handle_to_index( handle, &entry );
    struct fsync_cache cache;

    if (entry >= FSYNC_LIST_ENTRIES || !fsync_list[entry]) return FALSE;

again:
    *(uint64_t *)&cache = __atomic_load_n( (uint64_t *)&fsync_list[entry][idx], __ATOMIC_SEQ_CST );

    if (!cache.type || !cache.shm_idx) return FALSE;

    obj->type = cache.type;
    obj->shm = get_shm( cache.shm_idx );
    grab_object( obj );
    if (((int *)obj->shm)[2] < 2 ||
        *(uint64_t *)&cache != __atomic_load_n( (uint64_t *)&fsync_list[entry][idx], __ATOMIC_SEQ_CST ))
    {
        /* This check does not strictly guarantee that we avoid the potential race but is supposed to greatly
         * reduce the probability of that. */
        FIXME( "Cache changed while getting object, handle %p, shm_idx %d, refcount %d.\n",
               handle, cache.shm_idx, ((int *)obj->shm)[2] );
        put_object( obj );
        goto again;
    }
    return TRUE;
}

/* Gets an object. This is either a proper fsync object (i.e. an event,
 * semaphore, etc. created using create_fsync) or a generic synchronizable
 * server-side object which the server will signal (e.g. a process, thread,
 * message queue, etc.) */
static NTSTATUS get_object( HANDLE handle, struct fsync *obj )
{
    NTSTATUS ret = STATUS_SUCCESS;
    unsigned int shm_idx = 0;
    enum fsync_type type;
    sigset_t sigset;

    if (get_cached_object( handle, obj )) return STATUS_SUCCESS;

    if ((INT_PTR)handle < 0)
    {
        /* We can deal with pseudo-handles, but it's just easier this way */
        return STATUS_NOT_IMPLEMENTED;
    }

    if (!handle) return STATUS_INVALID_HANDLE;

    /* We need to try grabbing it from the server. Uninterrupted section
     * is needed to avoid race with NtClose() which first calls fsync_close()
     * and then closes handle on server. Without the section we might cache
     * already closed handle back. */
    server_enter_uninterrupted_section( &fd_cache_mutex, &sigset );
    if (get_cached_object( handle, obj ))
    {
        server_leave_uninterrupted_section( &fd_cache_mutex, &sigset );
        return STATUS_SUCCESS;
    }
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
    if (!ret) add_to_list( handle, type, shm_idx );
    server_leave_uninterrupted_section( &fd_cache_mutex, &sigset );

    if (ret)
    {
        WARN("Failed to retrieve shm index for handle %p, status %#x.\n", handle, (unsigned int)ret);
        return ret;
    }

    TRACE("Got shm index %d for handle %p.\n", shm_idx, handle);

    obj->type = type;
    obj->shm = get_shm( shm_idx );
    /* get_fsync_idx server request increments shared mem refcount, so not grabbing object here. */
    return ret;
}

static NTSTATUS get_object_for_wait( HANDLE handle, struct fsync *obj, int *prev_pid )
{
    NTSTATUS ret;
    int *shm;

    if ((ret = get_object( handle, obj ))) return ret;

    shm = obj->shm;
    /* Give wineserver a chance to cleanup shm index if the process
     * is killed while we are waiting on the object. */
    if (fsync_yield_to_waiters)
        *prev_pid = __atomic_exchange_n( &shm[3], current_pid, __ATOMIC_SEQ_CST );
    else
        __atomic_store_n( &shm[3], current_pid, __ATOMIC_SEQ_CST );
    return STATUS_SUCCESS;
}

NTSTATUS fsync_close( HANDLE handle )
{
    UINT_PTR entry, idx = handle_to_index( handle, &entry );

    TRACE("%p.\n", handle);

    if (entry < FSYNC_LIST_ENTRIES && fsync_list[entry])
    {
        struct fsync_cache cache;

        cache.type = 0;
        cache.shm_idx = 0;
        *(uint64_t *)&cache = __atomic_exchange_n( (uint64_t *)&fsync_list[entry][idx],
                                                   *(uint64_t *)&cache, __ATOMIC_SEQ_CST );
        if (cache.type) return STATUS_SUCCESS;
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
        req->type   = type;
        wine_server_add_data( req, objattr, len );
        ret = wine_server_call( req );
        if (!ret || ret == STATUS_OBJECT_NAME_EXISTS)
        {
            *handle = wine_server_ptr_handle( reply->handle );
            shm_idx = reply->shm_idx;
            type    = reply->type;
        }
    }
    SERVER_END_REQ;

    if (!ret || ret == STATUS_OBJECT_NAME_EXISTS)
    {
        add_to_list( *handle, type, shm_idx );
        TRACE("-> handle %p, shm index %d.\n", *handle, shm_idx);
    }

    free( objattr );
    return ret;
}

static NTSTATUS open_fsync( enum fsync_type type, HANDLE *handle,
    ACCESS_MASK access, const OBJECT_ATTRIBUTES *attr )
{
    NTSTATUS ret;
    unsigned int shm_idx;

    SERVER_START_REQ( open_fsync )
    {
        req->access     = access;
        req->attributes = attr->Attributes;
        req->rootdir    = wine_server_obj_handle( attr->RootDirectory );
        req->type       = type;
        if (attr->ObjectName)
            wine_server_add_data( req, attr->ObjectName->Buffer, attr->ObjectName->Length );
        if (!(ret = wine_server_call( req )))
        {
            *handle = wine_server_ptr_handle( reply->handle );
            type = reply->type;
            shm_idx = reply->shm_idx;
        }
    }
    SERVER_END_REQ;

    if (!ret)
    {
        add_to_list( *handle, type, shm_idx );

        TRACE("-> handle %p, shm index %u.\n", *handle, shm_idx);
    }
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

    current_pid = GetCurrentProcessId();
    assert(current_pid);
}

NTSTATUS fsync_create_semaphore( HANDLE *handle, ACCESS_MASK access,
    const OBJECT_ATTRIBUTES *attr, LONG initial, LONG max )
{
    TRACE("name %s, initial %d, max %d.\n",
        attr ? debugstr_us(attr->ObjectName) : "<no name>", (int)initial, (int)max);

    return create_fsync( FSYNC_SEMAPHORE, handle, access, attr, initial, max );
}

NTSTATUS fsync_open_semaphore( HANDLE *handle, ACCESS_MASK access,
    const OBJECT_ATTRIBUTES *attr )
{
    TRACE("name %s.\n", debugstr_us(attr->ObjectName));

    return open_fsync( FSYNC_SEMAPHORE, handle, access, attr );
}

NTSTATUS fsync_release_semaphore( HANDLE handle, ULONG count, ULONG *prev )
{
    struct fsync obj;
    struct semaphore *semaphore;
    ULONG current;
    NTSTATUS ret;

    TRACE("%p, %d, %p.\n", handle, (int)count, prev);

    if ((ret = get_object( handle, &obj ))) return ret;
    semaphore = obj.shm;

    do
    {
        current = semaphore->count;
        if (count + current > semaphore->max)
        {
            put_object( &obj );
            return STATUS_SEMAPHORE_LIMIT_EXCEEDED;
        }
    } while (__sync_val_compare_and_swap( &semaphore->count, current, count + current ) != current);

    if (prev) *prev = current;

    futex_wake( &semaphore->count, INT_MAX );

    put_object( &obj );
    return STATUS_SUCCESS;
}

NTSTATUS fsync_query_semaphore( HANDLE handle, void *info, ULONG *ret_len )
{
    struct fsync obj;
    struct semaphore *semaphore;
    SEMAPHORE_BASIC_INFORMATION *out = info;
    NTSTATUS ret;

    TRACE("handle %p, info %p, ret_len %p.\n", handle, info, ret_len);

    if ((ret = get_object( handle, &obj ))) return ret;
    semaphore = obj.shm;

    out->CurrentCount = semaphore->count;
    out->MaximumCount = semaphore->max;
    if (ret_len) *ret_len = sizeof(*out);

    put_object( &obj );
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

NTSTATUS fsync_open_event( HANDLE *handle, ACCESS_MASK access,
    const OBJECT_ATTRIBUTES *attr )
{
    TRACE("name %s.\n", debugstr_us(attr->ObjectName));

    return open_fsync( FSYNC_AUTO_EVENT, handle, access, attr );
}

NTSTATUS fsync_set_event( HANDLE handle, LONG *prev )
{
    struct event *event;
    struct fsync obj;
    LONG current;
    NTSTATUS ret;

    TRACE("%p.\n", handle);

    if ((ret = get_object( handle, &obj ))) return ret;
    event = obj.shm;

    if (obj.type != FSYNC_MANUAL_EVENT && obj.type != FSYNC_AUTO_EVENT)
    {
        put_object( &obj );
        return STATUS_OBJECT_TYPE_MISMATCH;
    }

    if (!(current = __atomic_exchange_n( &event->signaled, 1, __ATOMIC_SEQ_CST )))
        futex_wake( &event->signaled, INT_MAX );

    if (prev) *prev = current;

    put_object( &obj );
    return STATUS_SUCCESS;
}

NTSTATUS fsync_reset_event( HANDLE handle, LONG *prev )
{
    struct event *event;
    struct fsync obj;
    LONG current;
    NTSTATUS ret;

    TRACE("%p.\n", handle);

    if ((ret = get_object( handle, &obj ))) return ret;
    event = obj.shm;

    if (obj.type != FSYNC_MANUAL_EVENT && obj.type != FSYNC_AUTO_EVENT)
    {
        put_object( &obj );
        return STATUS_OBJECT_TYPE_MISMATCH;
    }

    current = __atomic_exchange_n( &event->signaled, 0, __ATOMIC_SEQ_CST );

    if (prev) *prev = current;

    put_object( &obj );
    return STATUS_SUCCESS;
}

NTSTATUS fsync_pulse_event( HANDLE handle, LONG *prev )
{
    struct event *event;
    struct fsync obj;
    LONG current;
    NTSTATUS ret;

    TRACE("%p.\n", handle);

    if ((ret = get_object( handle, &obj ))) return ret;
    event = obj.shm;

    if (obj.type != FSYNC_MANUAL_EVENT && obj.type != FSYNC_AUTO_EVENT)
    {
        put_object( &obj );
        return STATUS_OBJECT_TYPE_MISMATCH;
    }

    /* This isn't really correct; an application could miss the write.
     * Unfortunately we can't really do much better. Fortunately this is rarely
     * used (and publicly deprecated). */
    if (!(current = __atomic_exchange_n( &event->signaled, 1, __ATOMIC_SEQ_CST )))
        futex_wake( &event->signaled, INT_MAX );

    /* Try to give other threads a chance to wake up. Hopefully erring on this
     * side is the better thing to do... */
    usleep(0);

    __atomic_store_n( &event->signaled, 0, __ATOMIC_SEQ_CST );

    if (prev) *prev = current;

    put_object( &obj );
    return STATUS_SUCCESS;
}

NTSTATUS fsync_query_event( HANDLE handle, void *info, ULONG *ret_len )
{
    struct event *event;
    struct fsync obj;
    EVENT_BASIC_INFORMATION *out = info;
    NTSTATUS ret;

    TRACE("handle %p, info %p, ret_len %p.\n", handle, info, ret_len);

    if ((ret = get_object( handle, &obj ))) return ret;
    event = obj.shm;

    out->EventState = event->signaled;
    out->EventType = (obj.type == FSYNC_AUTO_EVENT ? SynchronizationEvent : NotificationEvent);
    if (ret_len) *ret_len = sizeof(*out);

    put_object( &obj );
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

NTSTATUS fsync_open_mutex( HANDLE *handle, ACCESS_MASK access,
    const OBJECT_ATTRIBUTES *attr )
{
    TRACE("name %s.\n", debugstr_us(attr->ObjectName));

    return open_fsync( FSYNC_MUTEX, handle, access, attr );
}

NTSTATUS fsync_release_mutex( HANDLE handle, LONG *prev )
{
    struct mutex *mutex;
    struct fsync obj;
    NTSTATUS ret;

    TRACE("%p, %p.\n", handle, prev);

    if ((ret = get_object( handle, &obj ))) return ret;
    mutex = obj.shm;

    if (mutex->tid != GetCurrentThreadId())
    {
        put_object( &obj );
        return STATUS_MUTANT_NOT_OWNED;
    }

    if (prev) *prev = mutex->count;

    if (!--mutex->count)
    {
        __atomic_store_n( &mutex->tid, 0, __ATOMIC_SEQ_CST );
        futex_wake( &mutex->tid, INT_MAX );
    }

    put_object( &obj );
    return STATUS_SUCCESS;
}

NTSTATUS fsync_query_mutex( HANDLE handle, void *info, ULONG *ret_len )
{
    struct fsync obj;
    struct mutex *mutex;
    MUTANT_BASIC_INFORMATION *out = info;
    NTSTATUS ret;

    TRACE("handle %p, info %p, ret_len %p.\n", handle, info, ret_len);

    if ((ret = get_object( handle, &obj ))) return ret;
    mutex = obj.shm;

    out->CurrentCount = 1 - mutex->count;
    out->OwnedByCaller = (mutex->tid == GetCurrentThreadId());
    out->AbandonedState = (mutex->tid == ~0);
    if (ret_len) *ret_len = sizeof(*out);

    put_object( &obj );
    return STATUS_SUCCESS;
}

static inline void try_yield_to_waiters( int prev_pid )
{
    if (!fsync_yield_to_waiters) return;

    /* On Windows singaling an object will wake the threads waiting on the object. With fsync
     * it may happen that signaling thread (or other thread) grabs the object before the already waiting
     * thread gets a chance. Try to workaround that for the affected apps. Non-zero 'prev_pid' indicates
     * that the object is grabbed in __fsync_wait_objects() by some other thread. It is the same for
     * a non-current pid, but we may currently have a stale PID on an object from a terminated process
     * and it is probably safer to skip this workaround. This won't work great if the object is used in 'wait all'
     * and the waiter is blocked on the other object.
     * This check is also not entirely reliable as if multiple waiters from the same process enter
     * __fsync_wait_objects() the first one leaving will clear 'last_pid' in the object. */

    if (prev_pid == current_pid)
        usleep(0);
}

static NTSTATUS do_single_wait( int *addr, int val, const struct timespec64 *end, clockid_t clock_id,
                                BOOLEAN alertable )
{
    struct futex_waitv futexes[2];
    int ret;

    futex_vector_set( &futexes[0], addr, val );

    if (alertable)
    {
        int *apc_futex = ntdll_get_thread_data()->fsync_apc_futex;

        if (__atomic_load_n( apc_futex, __ATOMIC_SEQ_CST ))
            return STATUS_USER_APC;

        futex_vector_set( &futexes[1], apc_futex, 0 );

        ret = futex_wait_multiple( futexes, 2, end, clock_id );

        if (__atomic_load_n( apc_futex, __ATOMIC_SEQ_CST ))
            return STATUS_USER_APC;
    }
    else
    {
        ret = futex_wait_multiple( futexes, 1, end, clock_id );
    }

    if (!ret)
        return 0;
    else if (ret < 0 && errno == ETIMEDOUT)
        return STATUS_TIMEOUT;
    else
        return STATUS_PENDING;
}

static void put_objects( struct fsync *objs, unsigned int count )
{
    unsigned int i;

    for (i = 0; i < count; ++i)
        if (objs[i].type) put_object_from_wait( &objs[i] );
}

static NTSTATUS __fsync_wait_objects( DWORD count, const HANDLE *handles,
    BOOLEAN wait_any, BOOLEAN alertable, const LARGE_INTEGER *timeout )
{
    static const LARGE_INTEGER zero = {0};

    int current_tid = 0;
#define CURRENT_TID (current_tid ? current_tid : (current_tid = GetCurrentThreadId()))

    struct futex_waitv futexes[MAXIMUM_WAIT_OBJECTS + 1];
    struct fsync objs[MAXIMUM_WAIT_OBJECTS];
    BOOL msgwait = FALSE, waited = FALSE;
    int prev_pids[MAXIMUM_WAIT_OBJECTS];
    int has_fsync = 0, has_server = 0;
    clockid_t clock_id = 0;
    struct timespec64 end;
    int dummy_futex = 0;
    LONGLONG timeleft;
    DWORD waitcount;
    int i, ret;

    /* Grab the APC futex if we don't already have it. */
    if (alertable && !ntdll_get_thread_data()->fsync_apc_futex)
    {
        unsigned int idx = 0;
        SERVER_START_REQ( get_fsync_apc_idx )
        {
            if (!(ret = wine_server_call( req )))
                idx = reply->shm_idx;
        }
        SERVER_END_REQ;

        if (idx)
        {
            struct event *apc_event = get_shm( idx );
            ntdll_get_thread_data()->fsync_apc_futex = &apc_event->signaled;
        }
    }

    get_wait_end_time( &timeout, &end, &clock_id );

    for (i = 0; i < count; i++)
    {
        ret = get_object_for_wait( handles[i], &objs[i], &prev_pids[i] );
        if (ret == STATUS_SUCCESS)
        {
            assert( objs[i].type );
            has_fsync = 1;
        }
        else if (ret == STATUS_NOT_IMPLEMENTED)
        {
            objs[i].type = 0;
            objs[i].shm = NULL;
            has_server = 1;
        }
        else
        {
            put_objects( objs, i );
            return ret;
        }
    }

    if (count && objs[count - 1].type == FSYNC_QUEUE)
        msgwait = TRUE;

    if (has_fsync && has_server)
        FIXME("Can't wait on fsync and server objects at the same time!\n");
    else if (has_server)
    {
        put_objects( objs, count );
        return STATUS_NOT_IMPLEMENTED;
    }

    if (TRACE_ON(fsync))
    {
        TRACE("Waiting for %s of %d handles:", wait_any ? "any" : "all", (int)count);
        for (i = 0; i < count; i++)
            TRACE(" %p", handles[i]);

        if (msgwait)
            TRACE(" or driver events");
        if (alertable)
            TRACE(", alertable");

        if (!timeout)
            TRACE(", timeout = INFINITE.\n");
        else
        {
            timeleft = update_timeout( &end, clock_id );
            TRACE(", timeout = %ld.%07ld sec.\n",
                (long) (timeleft / TICKSPERSEC), (long) (timeleft % TICKSPERSEC));
        }
    }

    if (wait_any || count <= 1)
    {
        while (1)
        {
            /* Try to grab anything. */

            if (alertable)
            {
                /* We must check this first! The server may set an event that
                 * we're waiting on, but we need to return STATUS_USER_APC. */
                if (__atomic_load_n( ntdll_get_thread_data()->fsync_apc_futex, __ATOMIC_SEQ_CST ))
                    goto userapc;
            }

            for (i = 0; i < count; i++)
            {
                struct fsync *obj = &objs[i];

                if (obj->type)
                {
                    switch (obj->type)
                    {
                    case FSYNC_SEMAPHORE:
                    {
                        struct semaphore *semaphore = obj->shm;
                        int current, new;

                        new = __atomic_load_n( &semaphore->count, __ATOMIC_SEQ_CST );
                        if (!waited && new)
                            try_yield_to_waiters(prev_pids[i]);

                        while ((current = new))
                        {
                            if ((new = __sync_val_compare_and_swap( &semaphore->count, current, current - 1 )) == current)
                            {
                                TRACE("Woken up by handle %p [%d].\n", handles[i], i);
                                if (waited) simulate_sched_quantum();
                                put_objects( objs, count );
                                return i;
                            }
                        }
                        futex_vector_set( &futexes[i], &semaphore->count, 0 );
                        break;
                    }
                    case FSYNC_MUTEX:
                    {
                        struct mutex *mutex = obj->shm;
                        int tid;

                        if (mutex->tid == CURRENT_TID)
                        {
                            TRACE("Woken up by handle %p [%d].\n", handles[i], i);
                            mutex->count++;
                            if (waited) simulate_sched_quantum();
                            put_objects( objs, count );
                            return i;
                        }

                        if (!waited && !mutex->tid)
                            try_yield_to_waiters(prev_pids[i]);

                        if (!(tid = __sync_val_compare_and_swap( &mutex->tid, 0, CURRENT_TID )))
                        {
                            TRACE("Woken up by handle %p [%d].\n", handles[i], i);
                            mutex->count = 1;
                            if (waited) simulate_sched_quantum();
                            put_objects( objs, count );
                            return i;
                        }
                        else if (tid == ~0 && (tid = __sync_val_compare_and_swap( &mutex->tid, ~0, CURRENT_TID )) == ~0)
                        {
                            TRACE("Woken up by abandoned mutex %p [%d].\n", handles[i], i);
                            mutex->count = 1;
                            put_objects( objs, count );
                            return STATUS_ABANDONED_WAIT_0 + i;
                        }

                        futex_vector_set( &futexes[i], &mutex->tid, tid );
                        break;
                    }
                    case FSYNC_AUTO_EVENT:
                    case FSYNC_AUTO_SERVER:
                    {
                        struct event *event = obj->shm;

                        if (!waited && event->signaled)
                            try_yield_to_waiters(prev_pids[i]);

                        if (__sync_val_compare_and_swap( &event->signaled, 1, 0 ))
                        {
                            if (ac_odyssey && alertable)
                                usleep( 0 );

                            TRACE("Woken up by handle %p [%d].\n", handles[i], i);
                            if (waited) simulate_sched_quantum();
                            put_objects( objs, count );
                            return i;
                        }
                        futex_vector_set( &futexes[i], &event->signaled, 0 );
                        break;
                    }
                    case FSYNC_MANUAL_EVENT:
                    case FSYNC_MANUAL_SERVER:
                    case FSYNC_QUEUE:
                    {
                        struct event *event = obj->shm;

                        if (__atomic_load_n( &event->signaled, __ATOMIC_SEQ_CST ))
                        {
                            if (ac_odyssey && alertable)
                                usleep( 0 );

                            TRACE("Woken up by handle %p [%d].\n", handles[i], i);
                            if (waited) simulate_sched_quantum();
                            put_objects( objs, count );
                            return i;
                        }
                        futex_vector_set( &futexes[i], &event->signaled, 0 );
                        break;
                    }
                    default:
                        ERR("Invalid type %#x for handle %p.\n", obj->type, handles[i]);
                        assert(0);
                    }
                }
                else
                {
                    /* Avoid breaking things entirely. */
                    futex_vector_set( &futexes[i], &dummy_futex, dummy_futex );
                }
            }

            if (alertable)
            {
                /* We already checked if it was signaled; don't bother doing it again. */
                futex_vector_set( &futexes[i++], ntdll_get_thread_data()->fsync_apc_futex, 0 );
            }
            waitcount = i;

            /* Looks like everything is contended, so wait. */

            if (ac_odyssey && alertable)
                usleep( 0 );

            if (timeout && !timeout->QuadPart)
            {
                /* Unlike esync, we already know that we've timed out, so we
                 * can avoid a syscall. */
                TRACE("Wait timed out.\n");
                put_objects( objs, count );
                return STATUS_TIMEOUT;
            }

            ret = futex_wait_multiple( futexes, waitcount, timeout ? &end : NULL, clock_id );

            /* FUTEX_WAIT_MULTIPLE can succeed or return -EINTR, -EAGAIN,
             * -EFAULT/-EACCES, -ETIMEDOUT. In the first three cases we need to
             * try again, bad address is already handled by the fact that we
             * tried to read from it, so only break out on a timeout. */
            if (ret == -1 && errno == ETIMEDOUT)
            {
                TRACE("Wait timed out.\n");
                put_objects( objs, count );
                return STATUS_TIMEOUT;
            }
            else waited = TRUE;
        } /* while (1) */
    }
    else
    {
        /* Wait-all is a little trickier to implement correctly. Fortunately,
         * it's not as common.
         *
         * The idea is basically just to wait in sequence on every object in the
         * set. Then when we're done, try to grab them all in a tight loop. If
         * that fails, release any resources we've grabbed (and yes, we can
         * reliably do this—it's just mutexes and semaphores that we have to
         * put back, and in both cases we just put back 1), and if any of that
         * fails we start over.
         *
         * What makes this inherently bad is that we might temporarily grab a
         * resource incorrectly. Hopefully it'll be quick (and hey, it won't
         * block on wineserver) so nobody will notice. Besides, consider: if
         * object A becomes signaled but someone grabs it before we can grab it
         * and everything else, then they could just as well have grabbed it
         * before it became signaled. Similarly if object A was signaled and we
         * were blocking on object B, then B becomes available and someone grabs
         * A before we can, then they might have grabbed A before B became
         * signaled. In either case anyone who tries to wait on A or B will be
         * waiting for an instant while we put things back. */

        NTSTATUS status = STATUS_SUCCESS;
        int current;

        while (1)
        {
            BOOL abandoned;

tryagain:
            abandoned = FALSE;

            /* First step: try to wait on each object in sequence. */

            for (i = 0; i < count; i++)
            {
                struct fsync *obj = &objs[i];

                if (obj->type == FSYNC_MUTEX)
                {
                    struct mutex *mutex = obj->shm;

                    if (mutex->tid == CURRENT_TID)
                        continue;

                    while ((current = __atomic_load_n( &mutex->tid, __ATOMIC_SEQ_CST )))
                    {
                        status = do_single_wait( &mutex->tid, current, timeout ? &end : NULL, clock_id, alertable );
                        if (status != STATUS_PENDING)
                            break;
                    }
                }
                else if (obj->type)
                {
                    /* this works for semaphores too */
                    struct event *event = obj->shm;

                    while (!__atomic_load_n( &event->signaled, __ATOMIC_SEQ_CST ))
                    {
                        status = do_single_wait( &event->signaled, 0, timeout ? &end : NULL, clock_id, alertable );
                        if (status != STATUS_PENDING)
                            break;
                    }
                }

                if (status == STATUS_TIMEOUT)
                {
                    TRACE("Wait timed out.\n");
                    put_objects( objs, count );
                    return status;
                }
                else if (status == STATUS_USER_APC)
                    goto userapc;
            }

            /* If we got here and we haven't timed out, that means all of the
             * handles were signaled. Check to make sure they still are. */
            for (i = 0; i < count; i++)
            {
                struct fsync *obj = &objs[i];

                if (obj->type == FSYNC_MUTEX)
                {
                    struct mutex *mutex = obj->shm;
                    int tid = __atomic_load_n( &mutex->tid, __ATOMIC_SEQ_CST );

                    if (tid && tid != ~0 && tid != CURRENT_TID)
                        goto tryagain;
                }
                else if (obj->type)
                {
                    struct event *event = obj->shm;

                    if (!__atomic_load_n( &event->signaled, __ATOMIC_SEQ_CST ))
                        goto tryagain;
                }
            }

            /* Yep, still signaled. Now quick, grab everything. */
            for (i = 0; i < count; i++)
            {
                struct fsync *obj = &objs[i];
                if (!obj->type) continue;
                switch (obj->type)
                {
                case FSYNC_MUTEX:
                {
                    struct mutex *mutex = obj->shm;
                    int tid = __atomic_load_n( &mutex->tid, __ATOMIC_SEQ_CST );
                    if (tid == CURRENT_TID)
                        break;
                    if (tid && tid != ~0)
                        goto tooslow;
                    if (__sync_val_compare_and_swap( &mutex->tid, tid, CURRENT_TID ) != tid)
                        goto tooslow;
                    if (tid == ~0)
                        abandoned = TRUE;
                    break;
                }
                case FSYNC_SEMAPHORE:
                {
                    struct semaphore *semaphore = obj->shm;
                    int current, new;

                    new = __atomic_load_n( &semaphore->count, __ATOMIC_SEQ_CST );
                    while ((current = new))
                    {
                        if ((new = __sync_val_compare_and_swap( &semaphore->count, current, current - 1 )) == current)
                            break;
                    }
                    if (!current)
                        goto tooslow;
                    break;
                }
                case FSYNC_AUTO_EVENT:
                case FSYNC_AUTO_SERVER:
                {
                    struct event *event = obj->shm;
                    if (!__sync_val_compare_and_swap( &event->signaled, 1, 0 ))
                        goto tooslow;
                    break;
                }
                default:
                    /* If a manual-reset event changed between there and
                     * here, it's shouldn't be a problem. */
                    break;
                }
            }

            /* If we got here, we successfully waited on every object.
             * Make sure to let ourselves know that we grabbed the mutexes. */
            for (i = 0; i < count; i++)
            {
                if (objs[i].type == FSYNC_MUTEX)
                {
                    struct mutex *mutex = objs[i].shm;
                    mutex->count++;
                }
            }

            if (abandoned)
            {
                TRACE("Wait successful, but some object(s) were abandoned.\n");
                put_objects( objs, count );
                return STATUS_ABANDONED;
            }
            TRACE("Wait successful.\n");
            put_objects( objs, count );
            return STATUS_SUCCESS;

tooslow:
            for (--i; i >= 0; i--)
            {
                struct fsync *obj = &objs[i];
                if (!obj->type) continue;
                switch (obj->type)
                {
                case FSYNC_MUTEX:
                {
                    struct mutex *mutex = obj->shm;
                    /* HACK: This won't do the right thing with abandoned
                     * mutexes, but fixing it is probably more trouble than
                     * it's worth. */
                    __atomic_store_n( &mutex->tid, 0, __ATOMIC_SEQ_CST );
                    break;
                }
                case FSYNC_SEMAPHORE:
                {
                    struct semaphore *semaphore = obj->shm;
                    __sync_fetch_and_add( &semaphore->count, 1 );
                    break;
                }
                case FSYNC_AUTO_EVENT:
                case FSYNC_AUTO_SERVER:
                {
                    struct event *event = obj->shm;
                    __atomic_store_n( &event->signaled, 1, __ATOMIC_SEQ_CST );
                    break;
                }
                default:
                    /* doesn't need to be put back */
                    break;
                }
            }
        } /* while (1) */
    } /* else (wait-all) */

    assert(0);  /* shouldn't reach here... */

userapc:
    TRACE("Woken up by user APC.\n");

    put_objects( objs, count );

    /* We have to make a server call anyway to get the APC to execute, so just
     * delegate down to server_wait(). */
    ret = server_wait( NULL, 0, SELECT_INTERRUPTIBLE | SELECT_ALERTABLE, &zero );

    /* This can happen if we received a system APC, and the APC fd was woken up
     * before we got SIGUSR1. poll() doesn't return EINTR in that case. The
     * right thing to do seems to be to return STATUS_USER_APC anyway. */
    if (ret == STATUS_TIMEOUT) ret = STATUS_USER_APC;
    return ret;
#undef CURRENT_TID
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
    struct fsync obj;
    NTSTATUS ret;

    if (count && !get_object( handles[count - 1], &obj ))
    {
        if (obj.type == FSYNC_QUEUE)
        {
            msgwait = TRUE;
            server_set_msgwait( 1 );
        }
        put_object( &obj );
    }

    ret = __fsync_wait_objects( count, handles, wait_any, alertable, timeout );

    if (msgwait)
        server_set_msgwait( 0 );

    return ret;
}

NTSTATUS fsync_signal_and_wait( HANDLE signal, HANDLE wait, BOOLEAN alertable,
    const LARGE_INTEGER *timeout )
{
    struct fsync obj;
    NTSTATUS ret;

    if ((ret = get_object( signal, &obj ))) return ret;

    switch (obj.type)
    {
    case FSYNC_SEMAPHORE:
        ret = fsync_release_semaphore( signal, 1, NULL );
        break;
    case FSYNC_AUTO_EVENT:
    case FSYNC_MANUAL_EVENT:
        ret = fsync_set_event( signal, NULL );
        break;
    case FSYNC_MUTEX:
        ret = fsync_release_mutex( signal, NULL );
        break;
    default:
        ret = STATUS_OBJECT_TYPE_MISMATCH;
        break;
    }
    put_object( &obj );
    if (ret) return ret;

    return fsync_wait_objects( 1, &wait, TRUE, alertable, timeout );
}
