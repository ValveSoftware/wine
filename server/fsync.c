/*
 * futex-based synchronization objects
 *
 * Copyright (C) 2018 Zebediah Figura
 * Copyright (C) 2025 Paul Gofman for CodeWeavers
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

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/mman.h>
#include <stdint.h>
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef HAVE_SYS_SYSCALL_H
# include <sys/syscall.h>
#endif
#include <unistd.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winternl.h"

#include "handle.h"
#include "request.h"
#include "fsync.h"

#include "pshpack4.h"
#include "poppack.h"

#ifndef __NR_futex_waitv
#define __NR_futex_waitv 449
#endif

int do_fsync_cached = -1;

int fsync_check_support(void)
{
#ifdef __linux__
    syscall( __NR_futex_waitv, 0, 0, 0, 0, 0 );
    return getenv( "WINEFSYNC" ) && atoi(getenv( "WINEFSYNC" )) && errno != ENOSYS && errno != EPERM;
#else
    return 0;
#endif
}

static char shm_name[29];
static int shm_fd;
static off_t shm_size;
static void **shm_addrs;
static int shm_addrs_size;  /* length of the allocated shm_addrs array */

static int is_fsync_initialized;

static uint64_t *shm_idx_free_map;
static uint32_t shm_idx_free_map_size; /* uint64_t word count */
static uint32_t shm_idx_free_search_start_hint;

#define BITS_IN_FREE_MAP_WORD (8 * sizeof(*shm_idx_free_map))

static void shm_cleanup(void)
{
    close( shm_fd );
    if (shm_unlink( shm_name ) == -1)
        perror( "shm_unlink" );
}

void fsync_init(void)
{
    struct stat st;

    if (fstat( config_dir_fd, &st ) == -1)
        fatal_error( "cannot stat config dir\n" );

    if (st.st_ino != (unsigned long)st.st_ino)
        sprintf( shm_name, "/wine-%lx%08lx-fsync", (unsigned long)((unsigned long long)st.st_ino >> 32), (unsigned long)st.st_ino );
    else
        sprintf( shm_name, "/wine-%lx-fsync", (unsigned long)st.st_ino );

    if (!shm_unlink( shm_name ))
        fprintf( stderr, "fsync: warning: a previous shm file %s was not properly removed\n", shm_name );

    shm_fd = shm_open( shm_name, O_RDWR | O_CREAT | O_EXCL, 0644 );
    if (shm_fd == -1)
        perror( "shm_open" );

    shm_addrs = calloc( 128, sizeof(shm_addrs[0]) );
    shm_addrs_size = 128;

    shm_size = FSYNC_SHM_PAGE_SIZE;
    if (ftruncate( shm_fd, shm_size ) == -1)
        perror( "ftruncate" );

    is_fsync_initialized = 1;

    fprintf( stderr, "fsync: up and running.\n" );

    shm_idx_free_map_size = 256;
    shm_idx_free_map = malloc( shm_idx_free_map_size * sizeof(*shm_idx_free_map) );
    memset( shm_idx_free_map, 0xff, shm_idx_free_map_size * sizeof(*shm_idx_free_map) );
    shm_idx_free_map[0] &= ~(uint64_t)1; /* Avoid allocating shm_index 0. */

    atexit( shm_cleanup );
}

static struct list mutex_list = LIST_INIT(mutex_list);

struct fsync
{
    struct object  obj;
    unsigned int   shm_idx;
    enum fsync_type type;
    struct list     mutex_entry;
};

static void *get_shm( unsigned int idx )
{
    int entry  = (idx * 16) / FSYNC_SHM_PAGE_SIZE;
    int offset = (idx * 16) % FSYNC_SHM_PAGE_SIZE;

    if (entry >= shm_addrs_size)
    {
        int new_size = max(shm_addrs_size * 2, entry + 1);

        if (!(shm_addrs = realloc( shm_addrs, new_size * sizeof(shm_addrs[0]) )))
            fprintf( stderr, "fsync: couldn't expand shm_addrs array to size %d\n", entry + 1 );

        memset( shm_addrs + shm_addrs_size, 0, (new_size - shm_addrs_size) * sizeof(shm_addrs[0]) );

        shm_addrs_size = new_size;
    }

    if (!shm_addrs[entry])
    {
        void *addr = mmap( NULL, FSYNC_SHM_PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd,
                          (off_t)entry * FSYNC_SHM_PAGE_SIZE );
        if (addr == (void *)-1)
        {
            fprintf( stderr, "fsync: failed to map page %d (offset %#zx): ",
                     entry, (size_t)entry * FSYNC_SHM_PAGE_SIZE );
            perror( "mmap" );
        }

        if (debug_level)
            fprintf( stderr, "fsync: Mapping page %d at %p.\n", entry, addr );

        if (__sync_val_compare_and_swap( &shm_addrs[entry], 0, addr ))
            munmap( addr, FSYNC_SHM_PAGE_SIZE ); /* someone beat us to it */
    }

    return (void *)((unsigned long)shm_addrs[entry] + offset);
}

static int alloc_shm_idx_from_word( unsigned int word_index )
{
    int ret;

    if (!shm_idx_free_map[word_index]) return 0;

    ret = __builtin_ctzll( shm_idx_free_map[word_index] );
    shm_idx_free_map[word_index] &= ~((uint64_t)1 << ret);
    shm_idx_free_search_start_hint = shm_idx_free_map[word_index] ? word_index : word_index + 1;
    return word_index * BITS_IN_FREE_MAP_WORD + ret;
}

unsigned int fsync_alloc_shm( int low, int high )
{
#ifdef __linux__
    unsigned int i;
    int shm_idx;
    int *shm;

    /* this is arguably a bit of a hack, but we need some way to prevent
     * allocating shm for the master socket */
    if (!is_fsync_initialized)
        return 0;

    /* shm_idx_free_search_start_hint is always at the first word with a free index or before that. */
    for (i = shm_idx_free_search_start_hint; i < shm_idx_free_map_size; ++i)
        if ((shm_idx = alloc_shm_idx_from_word( i ))) break;

    if (!shm_idx)
    {
        uint32_t old_size, new_size;
        uint64_t *new_alloc;

        old_size = shm_idx_free_map_size;
        new_size = old_size + 256;
        new_alloc = realloc( shm_idx_free_map, new_size * sizeof(*new_alloc) );
        if (!new_alloc)
        {
            fprintf( stderr, "fsync: couldn't expand shm_idx_free_map to size %zd.",
                new_size * sizeof(*new_alloc) );
            return 0;
        }
        memset( new_alloc + old_size, 0xff, (new_size - old_size) * sizeof(*new_alloc) );
        shm_idx_free_map = new_alloc;
        shm_idx_free_map_size = new_size;
        shm_idx = alloc_shm_idx_from_word( old_size );
    }

    while (shm_idx * 16 >= shm_size)
    {
        /* Better expand the shm section. */
        shm_size += FSYNC_SHM_PAGE_SIZE;
        if (ftruncate( shm_fd, shm_size ) == -1)
        {
            fprintf( stderr, "fsync: couldn't expand %s to size %jd: ",
                shm_name, shm_size );
            perror( "ftruncate" );
        }
    }

    shm = get_shm( shm_idx );
    assert(shm);
    shm[0] = low;
    shm[1] = high;
    shm[2] = 1; /* Reference count. */
    shm[3] = 0; /* Last reference process id. */

    return shm_idx;
#else
    return 0;
#endif
}

void fsync_free_shm_idx( int shm_idx )
{
    unsigned int idx;
    uint64_t mask;
    int *shm;

    if (!shm_idx) return;
    assert( shm_idx > 0 && shm_idx < shm_idx_free_map_size * BITS_IN_FREE_MAP_WORD );

    shm = get_shm( shm_idx );
    if (shm[2] <= 0)
    {
        fprintf( stderr, "wineserver: fsync err: shm refcount is %d.\n", shm[2] );
        return;
    }

    if (__atomic_sub_fetch( &shm[2], 1, __ATOMIC_SEQ_CST ))
    {
        /* Sync object is still referenced in a process. */
        return;
    }

    idx = shm_idx / BITS_IN_FREE_MAP_WORD;
    mask = (uint64_t)1 << (shm_idx % BITS_IN_FREE_MAP_WORD);
    assert( !(shm_idx_free_map[idx] & mask) );
    shm_idx_free_map[idx] |= mask;
    if (idx < shm_idx_free_search_start_hint)
        shm_idx_free_search_start_hint = idx;
}

/* Try to cleanup the shared mem indices locked by the wait on the killed processes.
 * This is not fully reliable but should avoid leaking the majority of indices on
 * process kill. */
void fsync_cleanup_process_shm_indices( process_id_t id )
{
    uint64_t free_word;
    unsigned int i, j;
    void *shmbase;
    int *shm;

    for (i = 0; i < shm_idx_free_map_size; ++i)
    {
        free_word = shm_idx_free_map[i];
        if (free_word == ~(uint64_t)0) continue;
        shmbase = get_shm( i * BITS_IN_FREE_MAP_WORD );
        for (j = !i; j < BITS_IN_FREE_MAP_WORD; ++j)
        {
            shm = (int *)((char *)shmbase + j * 16);
            if (!(free_word & ((uint64_t)1 << j)) && shm[3] == id
                  && __atomic_load_n( &shm[2], __ATOMIC_SEQ_CST ) == 1)
                fsync_free_shm_idx( i * BITS_IN_FREE_MAP_WORD + j );
        }
    }
}

int fsync_grab_shm_idx( unsigned int shm_idx )
{
    int *shm;

    shm = get_shm( shm_idx );
    __atomic_add_fetch( &shm[2], 1, __ATOMIC_SEQ_CST );
    return shm_idx;
}

static inline int futex_wake( int *addr, int val )
{
    return syscall( __NR_futex, addr, 1, val, NULL, 0, 0 );
}

/* shm layout for events or event-like objects. */
struct fsync_event
{
    int signaled;
    int unused;
    int ref;
    int last_pid;
};

void fsync_set_event( unsigned int shm_idx )
{
    struct fsync_event *event = get_shm( shm_idx );

    if (!__atomic_exchange_n( &event->signaled, 1, __ATOMIC_SEQ_CST ))
        futex_wake( &event->signaled, INT_MAX );
}

void fsync_reset_event( unsigned int shm_idx )
{
    struct fsync_event *event = get_shm( shm_idx );

    __atomic_store_n( &event->signaled, 0, __ATOMIC_SEQ_CST );
}

struct mutex
{
    int tid;
    int count;  /* recursion count */
};

void fsync_abandon_mutex( unsigned int shm_idx, thread_id_t tid )
{
    struct mutex *mutex = get_shm( shm_idx );

    if (mutex->tid != tid) return;

    if (debug_level)
        fprintf( stderr, "fsync_abandon_mutexes() idx=%d\n", shm_idx );

    mutex->tid = ~0;
    mutex->count = 0;
    futex_wake( &mutex->tid, INT_MAX );
}

DECL_HANDLER(fsync_free_shm_idx)
{
    if (!req->shm_idx || req->shm_idx >= shm_idx_free_map_size * BITS_IN_FREE_MAP_WORD)
    {
        set_error( STATUS_INVALID_PARAMETER );
        return;
    }
    fsync_free_shm_idx( req->shm_idx );
}
