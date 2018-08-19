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

#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#ifdef HAVE_SYS_EVENTFD_H
# include <sys/eventfd.h>
#endif
#ifdef HAVE_SYS_MMAN_H
# include <sys/mman.h>
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#include <unistd.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winternl.h"
#include "wine/library.h"

#include "handle.h"
#include "request.h"
#include "file.h"
#include "esync.h"

int do_esync(void)
{
#ifdef HAVE_SYS_EVENTFD_H
    static int do_esync_cached = -1;

    if (do_esync_cached == -1)
        do_esync_cached = getenv("WINEESYNC") && atoi(getenv("WINEESYNC"));

    return do_esync_cached;
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

void esync_init(void)
{
    struct stat st;

    if (stat( wine_get_config_dir(), &st ) == -1)
        fatal_error( "cannot stat %s\n", wine_get_config_dir() );

    if (st.st_ino != (unsigned long)st.st_ino)
        sprintf( shm_name, "/wine-%lx%08lx-esync", (unsigned long)((unsigned long long)st.st_ino >> 32), (unsigned long)st.st_ino );
    else
        sprintf( shm_name, "/wine-%lx-esync", (unsigned long)st.st_ino );

    shm_unlink( shm_name );

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

struct esync
{
    struct object   obj;    /* object header */
    int             fd;     /* eventfd file descriptor */
    enum esync_type type;
    unsigned int    shm_idx;    /* index into the shared memory section */
};

static void esync_dump( struct object *obj, int verbose );
static int esync_get_esync_fd( struct object *obj, enum esync_type *type );
static unsigned int esync_map_access( struct object *obj, unsigned int access );
static void esync_destroy( struct object *obj );

const struct object_ops esync_ops =
{
    sizeof(struct esync),      /* size */
    esync_dump,                /* dump */
    no_get_type,               /* get_type */
    no_add_queue,              /* add_queue */
    NULL,                      /* remove_queue */
    NULL,                      /* signaled */
    esync_get_esync_fd,        /* get_esync_fd */
    NULL,                      /* satisfied */
    no_signal,                 /* signal */
    no_get_fd,                 /* get_fd */
    esync_map_access,          /* map_access */
    default_get_sd,            /* get_sd */
    default_set_sd,            /* set_sd */
    no_lookup_name,            /* lookup_name */
    directory_link_name,       /* link_name */
    default_unlink_name,       /* unlink_name */
    no_open_file,              /* open_file */
    no_close_handle,           /* close_handle */
    esync_destroy              /* destroy */
};

static void esync_dump( struct object *obj, int verbose )
{
    struct esync *esync = (struct esync *)obj;
    assert( obj->ops == &esync_ops );
    fprintf( stderr, "esync fd=%d\n", esync->fd );
}

static int esync_get_esync_fd( struct object *obj, enum esync_type *type )
{
    struct esync *esync = (struct esync *)obj;
    *type = esync->type;
    return esync->fd;
}

static unsigned int esync_map_access( struct object *obj, unsigned int access )
{
    /* Sync objects have the same flags. */
    if (access & GENERIC_READ)    access |= STANDARD_RIGHTS_READ | EVENT_QUERY_STATE;
    if (access & GENERIC_WRITE)   access |= STANDARD_RIGHTS_WRITE | EVENT_MODIFY_STATE;
    if (access & GENERIC_EXECUTE) access |= STANDARD_RIGHTS_EXECUTE | SYNCHRONIZE;
    if (access & GENERIC_ALL)     access |= STANDARD_RIGHTS_ALL | EVENT_QUERY_STATE | EVENT_MODIFY_STATE;
    return access & ~(GENERIC_READ | GENERIC_WRITE | GENERIC_EXECUTE | GENERIC_ALL);
}

static void esync_destroy( struct object *obj )
{
    struct esync *esync = (struct esync *)obj;
    close( esync->fd );
}

static int type_matches( enum esync_type type1, enum esync_type type2 )
{
    return (type1 == type2) ||
           ((type1 == ESYNC_AUTO_EVENT || type1 == ESYNC_MANUAL_EVENT) &&
            (type2 == ESYNC_AUTO_EVENT || type2 == ESYNC_MANUAL_EVENT));
}

static void *get_shm( unsigned int idx )
{
    int entry  = (idx * 8) / pagesize;
    int offset = (idx * 8) % pagesize;

    if (entry >= shm_addrs_size)
    {
        if (!(shm_addrs = realloc( shm_addrs, (entry + 1) * sizeof(shm_addrs[0]) )))
            fprintf( stderr, "esync: couldn't expand shm_addrs array to size %d\n", entry + 1 );

        memset( &shm_addrs[shm_addrs_size], 0, (entry + 1 - shm_addrs_size) * sizeof(shm_addrs[0]) );

        shm_addrs_size = entry + 1;
    }

    if (!shm_addrs[entry])
    {
        void *addr = mmap( NULL, pagesize, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, entry * pagesize );
        if (addr == (void *)-1)
        {
            fprintf( stderr, "esync: failed to map page %d (offset %#lx): ", entry, entry * pagesize );
            perror( "mmap" );
        }

        if (debug_level)
            fprintf( stderr, "esync: Mapping page %d at %p.\n", entry, addr );

        if (interlocked_cmpxchg_ptr( &shm_addrs[entry], addr, 0 ))
            munmap( addr, pagesize ); /* someone beat us to it */
    }

    return (void *)((unsigned long)shm_addrs[entry] + offset);
}

struct semaphore
{
    int max;
    int count;
};
C_ASSERT(sizeof(struct semaphore) == 8);

struct mutex
{
    DWORD tid;
    int count;    /* recursion count */
};
C_ASSERT(sizeof(struct mutex) == 8);

struct event
{
    int signaled;
    int locked;
};
C_ASSERT(sizeof(struct event) == 8);

static struct esync *create_esync( struct object *root, const struct unicode_str *name,
    unsigned int attr, int initval, int max, enum esync_type type,
    const struct security_descriptor *sd )
{
#ifdef HAVE_SYS_EVENTFD_H
    struct esync *esync;

    if ((esync = create_named_object( root, &esync_ops, name, attr, sd )))
    {
        if (get_error() != STATUS_OBJECT_NAME_EXISTS)
        {
            int flags = EFD_CLOEXEC | EFD_NONBLOCK;

            if (type == ESYNC_SEMAPHORE)
                flags |= EFD_SEMAPHORE;

            /* initialize it if it didn't already exist */
            esync->fd = eventfd( initval, flags );
            if (esync->fd == -1)
            {
                perror( "eventfd" );
                file_set_error();
                release_object( esync );
                return NULL;
            }
            esync->type = type;

            /* Use the fd as index, since that'll be unique across all
             * processes, but should hopefully end up also allowing reuse. */
            esync->shm_idx = esync->fd + 1; /* we keep index 0 reserved */
            while (esync->shm_idx * 8 >= shm_size)
            {
                /* Better expand the shm section. */
                shm_size += pagesize;
                if (ftruncate( shm_fd, shm_size ) == -1)
                {
                    fprintf( stderr, "esync: couldn't expand %s to size %ld: ",
                        shm_name, shm_size );
                    perror( "ftruncate" );
                }
            }

            /* Initialize the shared memory portion. We want to do this on the
             * server side to avoid a potential though unlikely race whereby
             * the same object is opened and used between the time it's created
             * and the time its shared memory portion is initialized. */
            switch (type)
            {
            case ESYNC_SEMAPHORE:
            {
                struct semaphore *semaphore = get_shm( esync->shm_idx );
                semaphore->max = max;
                semaphore->count = initval;
                break;
            }
            case ESYNC_AUTO_EVENT:
            case ESYNC_MANUAL_EVENT:
            {
                struct event *event = get_shm( esync->shm_idx );
                event->signaled = initval ? 1 : 0;
                event->locked = 0;
                break;
            }
            case ESYNC_MUTEX:
            {
                struct mutex *mutex = get_shm( esync->shm_idx );
                mutex->tid = initval ? 0 : current->id;
                mutex->count = initval ? 0 : 1;
                break;
            }
            default:
                assert( 0 );
            }
        }
        else
        {
            /* validate the type */
            if (!type_matches( type, esync->type ))
            {
                release_object( &esync->obj );
                set_error( STATUS_OBJECT_TYPE_MISMATCH );
                return NULL;
            }
        }
    }
    return esync;
#else
    /* FIXME: Provide a fallback implementation using pipe(). */
    set_error( STATUS_NOT_IMPLEMENTED );
    return NULL;
#endif
}

/* Create a file descriptor for an existing handle.
 * Caller must close the handle when it's done; it's not linked to an esync
 * server object in any way. */
int esync_create_fd( int initval, int flags )
{
#ifdef HAVE_SYS_EVENTFD_H
    int fd;

    fd = eventfd( initval, flags | EFD_CLOEXEC | EFD_NONBLOCK );
    if (fd == -1)
        perror( "eventfd" );

    return fd;
#else
    return -1;
#endif
}

/* Wake up a specific fd. */
void esync_wake_fd( int fd )
{
    static const uint64_t value = 1;

    if (write( fd, &value, sizeof(value) ) == -1)
        perror( "esync: write" );
}

/* Wake up a server-side esync object. */
void esync_wake_up( struct object *obj )
{
    enum esync_type dummy;
    int fd;

    if (obj->ops->get_esync_fd)
    {
        fd = obj->ops->get_esync_fd( obj, &dummy );
        esync_wake_fd( fd );
    }
}

void esync_clear( int fd )
{
    uint64_t value;

    /* we don't care about the return value */
    read( fd, &value, sizeof(value) );
}

static inline void small_pause(void)
{
#ifdef __i386__
    __asm__ __volatile__( "rep;nop" : : : "memory" );
#else
    __asm__ __volatile__( "" : : : "memory" );
#endif
}

/* Server-side event support. */
void esync_set_event( struct esync *esync )
{
    static const uint64_t value = 1;
    struct event *event = get_shm( esync->shm_idx );

    assert( esync->obj.ops == &esync_ops );
    assert( event != NULL );

    if (debug_level)
        fprintf( stderr, "esync_set_event() fd=%d\n", esync->fd );

    /* Acquire the spinlock. */
    while (interlocked_cmpxchg( &event->locked, 1, 0 ))
        small_pause();

    if (!interlocked_xchg( &event->signaled, 1 ))
    {
        if (write( esync->fd, &value, sizeof(value) ) == -1)
            perror( "esync: write" );
    }

    /* Release the spinlock. */
    event->locked = 0;
}

void esync_reset_event( struct esync *esync )
{
    static uint64_t value = 1;
    struct event *event = get_shm( esync->shm_idx );

    assert( esync->obj.ops == &esync_ops );
    assert( event != NULL );

    if (debug_level)
        fprintf( stderr, "esync_reset_event() fd=%d\n", esync->fd );

    /* Acquire the spinlock. */
    while (interlocked_cmpxchg( &event->locked, 1, 0 ))
        small_pause();

    /* Only bother signaling the fd if we weren't already signaled. */
    if (interlocked_xchg( &event->signaled, 0 ))
    {
        /* we don't care about the return value */
        read( esync->fd, &value, sizeof(value) );
    }

    /* Release the spinlock. */
    event->locked = 0;
}

DECL_HANDLER(create_esync)
{
    struct esync *esync;
    struct unicode_str name;
    struct object *root;
    const struct security_descriptor *sd;
    const struct object_attributes *objattr = get_req_object_attributes( &sd, &name, &root );

    if (!do_esync())
    {
        set_error( STATUS_NOT_IMPLEMENTED );
        return;
    }

    if (!req->type)
    {
        set_error( STATUS_INVALID_PARAMETER_4 );
        return;
    }

    if (!objattr) return;

    if ((esync = create_esync( root, &name, objattr->attributes, req->initval,
        req->max, req->type, sd )))
    {
        if (get_error() == STATUS_OBJECT_NAME_EXISTS)
            reply->handle = alloc_handle( current->process, esync, req->access, objattr->attributes );
        else
            reply->handle = alloc_handle_no_access_check( current->process, esync,
                                                          req->access, objattr->attributes );

        reply->type = esync->type;
        reply->shm_idx = esync->shm_idx;
        send_client_fd( current->process, esync->fd, reply->handle );
        release_object( esync );
    }

    if (root) release_object( root );
}

DECL_HANDLER(open_esync)
{
    struct unicode_str name = get_req_unicode_str();

    reply->handle = open_object( current->process, req->rootdir, req->access,
                                 &esync_ops, &name, req->attributes );

    /* send over the fd */
    if (reply->handle)
    {
        struct esync *esync;

        if (!(esync = (struct esync *)get_handle_obj( current->process, reply->handle,
                                                      0, &esync_ops )))
            return;

        if (!type_matches( req->type, esync->type ))
        {
            set_error( STATUS_OBJECT_TYPE_MISMATCH );
            release_object( esync );
            return;
        }

        reply->type = esync->type;
        reply->shm_idx = esync->shm_idx;

        send_client_fd( current->process, esync->fd, reply->handle );
        release_object( esync );
    }
}

/* Retrieve a file descriptor for an esync object which will be signaled by the
 * server. The client should only read from (i.e. wait on) this object. */
DECL_HANDLER(get_esync_fd)
{
    struct object *obj;
    enum esync_type type;
    int fd;

    if (!(obj = get_handle_obj( current->process, req->handle, SYNCHRONIZE, NULL )))
        return;

    if (obj->ops->get_esync_fd)
    {
        fd = obj->ops->get_esync_fd( obj, &type );
        reply->type = type;
        if (obj->ops == &esync_ops)
        {
            struct esync *esync = (struct esync *)obj;
            reply->shm_idx = esync->shm_idx;
        }
        else
            reply->shm_idx = 0;
        send_client_fd( current->process, fd, req->handle );
    }
    else
    {
        if (debug_level)
        {
            fprintf( stderr, "%04x: esync: can't wait on object: ", current->id );
            obj->ops->dump( obj, 0 );
        }
        set_error( STATUS_NOT_IMPLEMENTED );
    }

    release_object( obj );
}

/* Return the fd used for waiting on user APCs. */
DECL_HANDLER(get_esync_apc_fd)
{
    send_client_fd( current->process, current->esync_apc_fd, current->id );
}
