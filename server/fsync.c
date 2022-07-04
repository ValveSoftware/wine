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

int do_fsync(void)
{
#ifdef __linux__
    static int do_fsync_cached = -1;

    if (do_fsync_cached == -1)
    {
        FILE *f;
        if ((f = fopen( "/sys/kernel/futex2/wait", "r" )))
        {
            fclose(f);
            do_fsync_cached = 0;
            fprintf( stderr, "fsync: old futex2 patches detected, disabling.\n" );
            return do_fsync_cached;
        }

        syscall( __NR_futex_waitv, 0, 0, 0, 0, 0);
        do_fsync_cached = getenv("WINEFSYNC") && atoi(getenv("WINEFSYNC")) && errno != ENOSYS;
    }

    return do_fsync_cached;
#else
    return 0;
#endif
}

static char shm_name[29];
static int shm_fd;
static off_t shm_size;
static void **shm_addrs;
static int shm_addrs_size;  /* length of the allocated shm_addrs array */
static long pagesize;

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

    pagesize = sysconf( _SC_PAGESIZE );

    shm_addrs = calloc( 128, sizeof(shm_addrs[0]) );
    shm_addrs_size = 128;

    shm_size = pagesize;
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

static void fsync_dump( struct object *obj, int verbose );
static unsigned int fsync_get_fsync_idx( struct object *obj, enum fsync_type *type );
static unsigned int fsync_map_access( struct object *obj, unsigned int access );
static void fsync_destroy( struct object *obj );

const struct object_ops fsync_ops =
{
    sizeof(struct fsync),      /* size */
    &no_type,                  /* type */
    fsync_dump,                /* dump */
    no_add_queue,              /* add_queue */
    NULL,                      /* remove_queue */
    NULL,                      /* signaled */
    NULL,                      /* get_esync_fd */
    fsync_get_fsync_idx,       /* get_fsync_idx */
    NULL,                      /* satisfied */
    no_signal,                 /* signal */
    no_get_fd,                 /* get_fd */
    fsync_map_access,          /* map_access */
    default_get_sd,            /* get_sd */
    default_set_sd,            /* set_sd */
    default_get_full_name,     /* get_full_name */
    no_lookup_name,            /* lookup_name */
    directory_link_name,       /* link_name */
    default_unlink_name,       /* unlink_name */
    no_open_file,              /* open_file */
    no_kernel_obj_list,        /* get_kernel_obj_list */
    no_close_handle,           /* close_handle */
    fsync_destroy              /* destroy */
};

static void fsync_dump( struct object *obj, int verbose )
{
    struct fsync *fsync = (struct fsync *)obj;
    assert( obj->ops == &fsync_ops );
    fprintf( stderr, "fsync idx=%d\n", fsync->shm_idx );
}

static unsigned int fsync_get_fsync_idx( struct object *obj, enum fsync_type *type)
{
    struct fsync *fsync = (struct fsync *)obj;
    *type = fsync->type;
    return fsync->shm_idx;
}

static unsigned int fsync_map_access( struct object *obj, unsigned int access )
{
    /* Sync objects have the same flags. */
    if (access & GENERIC_READ)    access |= STANDARD_RIGHTS_READ | EVENT_QUERY_STATE;
    if (access & GENERIC_WRITE)   access |= STANDARD_RIGHTS_WRITE | EVENT_MODIFY_STATE;
    if (access & GENERIC_EXECUTE) access |= STANDARD_RIGHTS_EXECUTE | SYNCHRONIZE;
    if (access & GENERIC_ALL)     access |= STANDARD_RIGHTS_ALL | EVENT_QUERY_STATE | EVENT_MODIFY_STATE;
    return access & ~(GENERIC_READ | GENERIC_WRITE | GENERIC_EXECUTE | GENERIC_ALL);
}

static void fsync_destroy( struct object *obj )
{
    struct fsync *fsync = (struct fsync *)obj;
    if (fsync->type == FSYNC_MUTEX)
        list_remove( &fsync->mutex_entry );
    fsync_free_shm_idx( fsync->shm_idx );
}

static void *get_shm( unsigned int idx )
{
    int entry  = (idx * 16) / pagesize;
    int offset = (idx * 16) % pagesize;

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
        void *addr = mmap( NULL, pagesize, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, entry * pagesize );
        if (addr == (void *)-1)
        {
            fprintf( stderr, "fsync: failed to map page %d (offset %#lx): ", entry, entry * pagesize );
            perror( "mmap" );
        }

        if (debug_level)
            fprintf( stderr, "fsync: Mapping page %d at %p.\n", entry, addr );

        if (__sync_val_compare_and_swap( &shm_addrs[entry], 0, addr ))
            munmap( addr, pagesize ); /* someone beat us to it */
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
        shm_size += pagesize;
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

    assert( shm_idx );
    assert( shm_idx < shm_idx_free_map_size * BITS_IN_FREE_MAP_WORD );

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

static int type_matches( enum fsync_type type1, enum fsync_type type2 )
{
    return (type1 == type2) ||
           ((type1 == FSYNC_AUTO_EVENT || type1 == FSYNC_MANUAL_EVENT) &&
            (type2 == FSYNC_AUTO_EVENT || type2 == FSYNC_MANUAL_EVENT));
}

struct fsync *create_fsync( struct object *root, const struct unicode_str *name,
    unsigned int attr, int low, int high, enum fsync_type type,
    const struct security_descriptor *sd )
{
#ifdef __linux__
    struct fsync *fsync;

    if ((fsync = create_named_object( root, &fsync_ops, name, attr, sd )))
    {
        if (get_error() != STATUS_OBJECT_NAME_EXISTS)
        {
            /* initialize it if it didn't already exist */

            /* Initialize the shared memory portion. We want to do this on the
             * server side to avoid a potential though unlikely race whereby
             * the same object is opened and used between the time it's created
             * and the time its shared memory portion is initialized. */

            fsync->shm_idx = fsync_alloc_shm( low, high );
            fsync->type = type;
            if (type == FSYNC_MUTEX)
                list_add_tail( &mutex_list, &fsync->mutex_entry );
        }
        else
        {
            /* validate the type */
            if (!type_matches( type, fsync->type ))
            {
                release_object( &fsync->obj );
                set_error( STATUS_OBJECT_TYPE_MISMATCH );
                return NULL;
            }
        }
    }

    return fsync;
#else
    set_error( STATUS_NOT_IMPLEMENTED );
    return NULL;
#endif
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

void fsync_wake_futex( unsigned int shm_idx )
{
    struct fsync_event *event;

    if (debug_level)
        fprintf( stderr, "fsync_wake_futex: index %u\n", shm_idx );

    if (!shm_idx)
        return;

    event = get_shm( shm_idx );
    if (!__atomic_exchange_n( &event->signaled, 1, __ATOMIC_SEQ_CST ))
        futex_wake( &event->signaled, INT_MAX );
}

void fsync_wake_up( struct object *obj )
{
    enum fsync_type type;

    if (debug_level)
        fprintf( stderr, "fsync_wake_up: object %p\n", obj );

    if (obj->ops->get_fsync_idx)
        fsync_wake_futex( obj->ops->get_fsync_idx( obj, &type ) );
}

void fsync_clear_futex( unsigned int shm_idx )
{
    struct fsync_event *event;

    if (debug_level)
        fprintf( stderr, "fsync_clear_futex: index %u\n", shm_idx );

    if (!shm_idx)
        return;

    event = get_shm( shm_idx );
    __atomic_store_n( &event->signaled, 0, __ATOMIC_SEQ_CST );
}

void fsync_clear( struct object *obj )
{
    enum fsync_type type;

    if (debug_level)
        fprintf( stderr, "fsync_clear: object %p\n", obj );

    if (obj->ops->get_fsync_idx)
        fsync_clear_futex( obj->ops->get_fsync_idx( obj, &type ) );
}

void fsync_set_event( struct fsync *fsync )
{
    struct fsync_event *event = get_shm( fsync->shm_idx );
    assert( fsync->obj.ops == &fsync_ops );

    if (!__atomic_exchange_n( &event->signaled, 1, __ATOMIC_SEQ_CST ))
        futex_wake( &event->signaled, INT_MAX );
}

void fsync_reset_event( struct fsync *fsync )
{
    struct fsync_event *event = get_shm( fsync->shm_idx );
    assert( fsync->obj.ops == &fsync_ops );

    __atomic_store_n( &event->signaled, 0, __ATOMIC_SEQ_CST );
}

struct mutex
{
    int tid;
    int count;  /* recursion count */
};

void fsync_abandon_mutexes( struct thread *thread )
{
    struct fsync *fsync;

    LIST_FOR_EACH_ENTRY( fsync, &mutex_list, struct fsync, mutex_entry )
    {
        struct mutex *mutex = get_shm( fsync->shm_idx );

        if (mutex->tid == thread->id)
        {
            if (debug_level)
                fprintf( stderr, "fsync_abandon_mutexes() idx=%d\n", fsync->shm_idx );
            mutex->tid = ~0;
            mutex->count = 0;
            futex_wake( &mutex->tid, INT_MAX );
        }
    }
}

DECL_HANDLER(create_fsync)
{
    struct fsync *fsync;
    struct unicode_str name;
    struct object *root;
    const struct security_descriptor *sd;
    const struct object_attributes *objattr = get_req_object_attributes( &sd, &name, &root );

    if (!do_fsync())
    {
        set_error( STATUS_NOT_IMPLEMENTED );
        return;
    }

    if (!objattr) return;

    if ((fsync = create_fsync( root, &name, objattr->attributes, req->low,
                               req->high, req->type, sd )))
    {
        if (get_error() == STATUS_OBJECT_NAME_EXISTS)
            reply->handle = alloc_handle( current->process, fsync, req->access, objattr->attributes );
        else
            reply->handle = alloc_handle_no_access_check( current->process, fsync,
                                                          req->access, objattr->attributes );

        reply->shm_idx = fsync->shm_idx;
        reply->type = fsync->type;
        release_object( fsync );
    }

    if (root) release_object( root );
}

DECL_HANDLER(open_fsync)
{
    struct unicode_str name = get_req_unicode_str();

    reply->handle = open_object( current->process, req->rootdir, req->access,
                                 &fsync_ops, &name, req->attributes );

    if (reply->handle)
    {
        struct fsync *fsync;

        if (!(fsync = (struct fsync *)get_handle_obj( current->process, reply->handle,
                                                      0, &fsync_ops )))
            return;

        if (!type_matches( req->type, fsync->type ))
        {
            set_error( STATUS_OBJECT_TYPE_MISMATCH );
            release_object( fsync );
            return;
        }

        reply->type = fsync->type;
        reply->shm_idx = fsync->shm_idx;
        release_object( fsync );
    }
}

/* Retrieve the index of a shm section which will be signaled by the server. */
DECL_HANDLER(get_fsync_idx)
{
    struct object *obj;
    enum fsync_type type;

    if (!(obj = get_handle_obj( current->process, req->handle, SYNCHRONIZE, NULL )))
        return;

    if (obj->ops->get_fsync_idx)
    {
        int *shm;

        reply->shm_idx = obj->ops->get_fsync_idx( obj, &type );
        reply->type = type;
        shm = get_shm( reply->shm_idx );
        __atomic_add_fetch( &shm[2], 1, __ATOMIC_SEQ_CST );
    }
    else
    {
        if (debug_level)
        {
            fprintf( stderr, "%04x: fsync: can't wait on object: ", current->id );
            obj->ops->dump( obj, 0 );
        }
        set_error( STATUS_NOT_IMPLEMENTED );
    }

    release_object( obj );
}

DECL_HANDLER(get_fsync_apc_idx)
{
    reply->shm_idx = current->fsync_apc_idx;
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
