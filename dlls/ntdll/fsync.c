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

#include "config.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_SYS_MMAN_H
# include <sys/mman.h>
#endif
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
#include "wine/library.h"
#include "wine/server.h"

#include "ntdll_misc.h"
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

enum fsync_type
{
    FSYNC_SEMAPHORE = 1,
};

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
        shm_addrs_size = entry + 1;
        if (!(shm_addrs = RtlReAllocateHeap( GetProcessHeap(), HEAP_ZERO_MEMORY,
                shm_addrs, shm_addrs_size * sizeof(shm_addrs[0]) )))
            ERR("Failed to grow shm_addrs array to size %d.\n", shm_addrs_size);
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
            void *ptr = wine_anon_mmap( NULL, FSYNC_LIST_BLOCK_SIZE * sizeof(struct fsync),
                                        PROT_READ | PROT_WRITE, 0 );
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

    RtlFreeHeap( GetProcessHeap(), 0, objattr );
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

    if (stat( wine_get_config_dir(), &st ) == -1)
        ERR("Cannot stat %s\n", wine_get_config_dir());

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

    shm_addrs = RtlAllocateHeap( GetProcessHeap(), HEAP_ZERO_MEMORY, 128 * sizeof(shm_addrs[0]) );
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
