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
#include "wine/port.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdarg.h>
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
#include "windef.h"
#include "winternl.h"
#include "wine/library.h"

#include "handle.h"
#include "request.h"
#include "fsync.h"

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
    return 0;
#endif
}

static char shm_name[29];
static int shm_fd;
static off_t shm_size;
static void **shm_addrs;
static int shm_addrs_size;  /* length of the allocated shm_addrs array */
static long pagesize;

static void shm_cleanup(void)
{
    close( shm_fd );
    if (shm_unlink( shm_name ) == -1)
        perror( "shm_unlink" );
}

void fsync_init(void)
{
    struct stat st;

    if (stat( wine_get_config_dir(), &st ) == -1)
        fatal_error( "cannot stat %s\n", wine_get_config_dir() );

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

    atexit( shm_cleanup );
}

struct fsync
{
    struct object  obj;
    unsigned int   shm_idx;
    enum fsync_type type;
};

static void fsync_dump( struct object *obj, int verbose );
static void fsync_destroy( struct object *obj );

const struct object_ops fsync_ops =
{
    sizeof(struct fsync),      /* size */
    fsync_dump,                /* dump */
    no_get_type,               /* get_type */
    no_add_queue,              /* add_queue */
    NULL,                      /* remove_queue */
    NULL,                      /* signaled */
    NULL,                      /* get_esync_fd */
    NULL,                      /* get_fsync_idx */
    NULL,                      /* satisfied */
    no_signal,                 /* signal */
    no_get_fd,                 /* get_fd */
    no_map_access,             /* map_access */
    default_get_sd,            /* get_sd */
    default_set_sd,            /* set_sd */
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

static void fsync_destroy( struct object *obj )
{
}

static void *get_shm( unsigned int idx )
{
    int entry  = (idx * 8) / pagesize;
    int offset = (idx * 8) % pagesize;

    if (entry >= shm_addrs_size)
    {
        if (!(shm_addrs = realloc( shm_addrs, (entry + 1) * sizeof(shm_addrs[0]) )))
            fprintf( stderr, "fsync: couldn't expand shm_addrs array to size %d\n", entry + 1 );

        memset( &shm_addrs[shm_addrs_size], 0, (entry + 1 - shm_addrs_size) * sizeof(shm_addrs[0]) );

        shm_addrs_size = entry + 1;
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

        if (interlocked_cmpxchg_ptr( &shm_addrs[entry], addr, 0 ))
            munmap( addr, pagesize ); /* someone beat us to it */
    }

    return (void *)((unsigned long)shm_addrs[entry] + offset);
}

/* FIXME: This is rather inefficient... */
static unsigned int shm_idx_counter = 1;

unsigned int fsync_alloc_shm( int low, int high )
{
#ifdef __linux__
    int shm_idx = shm_idx_counter++;
    int *shm;

    while (shm_idx * 8 >= shm_size)
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

    return shm_idx;
#else
    return 0;
#endif
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
};

void fsync_wake_up( struct object *obj )
{
    struct fsync_event *event;
    enum fsync_type type;

    if (obj->ops->get_fsync_idx)
    {
        event = get_shm( obj->ops->get_fsync_idx( obj, &type ) );

        if (!__atomic_exchange_n( &event->signaled, 1, __ATOMIC_SEQ_CST ))
            futex_wake( &event->signaled, INT_MAX );
    }
}

void fsync_clear( struct object *obj )
{
    struct fsync_event *event;
    enum fsync_type type;

    if (obj->ops->get_fsync_idx)
    {
        event = get_shm( obj->ops->get_fsync_idx( obj, &type ) );

        __atomic_store_n( &event->signaled, 0, __ATOMIC_SEQ_CST );
    }
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
        reply->shm_idx = obj->ops->get_fsync_idx( obj, &type );
        reply->type = type;
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
