/*
 * In-process synchronization primitives
 *
 * Copyright (C) 2021-2022 Elizabeth Figura for CodeWeavers
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
#include <stdint.h>
#include <stdio.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "winternl.h"

#include "file.h"
#include "handle.h"
#include "request.h"
#include "thread.h"
#include "user.h"

#include "fsync.h"

#include "ntsync_tmp.h"

#ifdef NTSYNC_IOC_EVENT_READ

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

int get_inproc_device_fd(void)
{
    static int fd = -2;
    if (fd == -2)
    {
        if (getenv( "PROTON_NO_NTSYNC" ) && atoi(getenv( "PROTON_NO_NTSYNC" )))
            fd = -1;
        else
            fd = open( "/dev/ntsync", O_CLOEXEC | O_RDONLY );
        if (fd >= 0)
        {
            do_fsync_cached = 0;
            fprintf( stderr, "ntsync: up and running.\n" );
        }
        else if (do_fsync()) fd = FSYNC_USED_BY_SERVER;
        else fprintf( stderr, "wineserver: using server-side synchronization.\n" );
    }
    return fd;
}

struct inproc_sync
{
    struct object          obj;  /* object header */
    enum inproc_sync_type  type;
    int                    fd;
    struct list            entry;
};

static struct list inproc_mutexes = LIST_INIT( inproc_mutexes );

static void inproc_sync_dump( struct object *obj, int verbose );
static int inproc_sync_signal( struct object *obj, unsigned int access, int signal );
static void inproc_sync_destroy( struct object *obj );

static const struct object_ops inproc_sync_ops =
{
    sizeof(struct inproc_sync), /* size */
    &no_type,                   /* type */
    inproc_sync_dump,           /* dump */
    no_add_queue,               /* add_queue */
    NULL,                       /* remove_queue */
    NULL,                       /* signaled */
    NULL,                       /* satisfied */
    inproc_sync_signal,         /* signal */
    no_get_fd,                  /* get_fd */
    default_get_sync,           /* get_sync */
    default_map_access,         /* map_access */
    default_get_sd,             /* get_sd */
    default_set_sd,             /* set_sd */
    default_get_full_name,      /* get_full_name */
    no_lookup_name,             /* lookup_name */
    directory_link_name,        /* link_name */
    default_unlink_name,        /* unlink_name */
    no_open_file,               /* open_file */
    no_kernel_obj_list,         /* get_kernel_obj_list */
    no_close_handle,            /* close_handle */
    inproc_sync_destroy,        /* destroy */
};

int get_inproc_sync_fd( struct inproc_sync *sync )
{
    if (!sync) return -1;
    return sync->fd;
}

struct inproc_sync *create_inproc_internal_sync( int manual, int signaled )
{
    struct ntsync_event_args args = {.signaled = signaled, .manual = manual};
    struct inproc_sync *event;

    if (!(event = alloc_object( &inproc_sync_ops ))) return NULL;
    if (do_fsync())
    {
        event->type = manual ? FSYNC_MANUAL_SERVER : FSYNC_AUTO_SERVER;
        event->fd = fsync_alloc_shm( signaled, 0xdeadbeef );
    }
    else
    {
        event->type = INPROC_SYNC_INTERNAL;
        event->fd   = ioctl( get_inproc_device_fd(), NTSYNC_IOC_CREATE_EVENT, &args );
    }
    list_init( &event->entry );

    if (event->fd == -1)
    {
        set_error( STATUS_TOO_MANY_OPENED_FILES );
        release_object( event );
        return NULL;
    }
    return event;
}

struct inproc_sync *create_inproc_event_sync( int manual, int signaled )
{
    struct ntsync_event_args args = {.signaled = signaled, .manual = manual};
    struct inproc_sync *event;

    if (!(event = alloc_object( &inproc_sync_ops ))) return NULL;
    if (do_fsync())
    {
        event->type = manual ? FSYNC_MANUAL_EVENT : FSYNC_AUTO_EVENT;
        event->fd = fsync_alloc_shm( signaled, 0xdeadbeef );
    }
    else
    {
        event->type = INPROC_SYNC_EVENT;
        event->fd   = ioctl( get_inproc_device_fd(), NTSYNC_IOC_CREATE_EVENT, &args );
    }
    list_init( &event->entry );

    if (event->fd == -1)
    {
        set_error( STATUS_TOO_MANY_OPENED_FILES );
        release_object( event );
        return NULL;
    }
    return event;
}

struct inproc_sync *create_inproc_mutex_sync( thread_id_t owner, unsigned int count )
{
    struct ntsync_mutex_args args = {.owner = owner, .count = count};
    struct inproc_sync *mutex;

    if (!(mutex = alloc_object( &inproc_sync_ops ))) return NULL;
    if (do_fsync())
    {
        mutex->type = FSYNC_MUTEX;
        mutex->fd = fsync_alloc_shm( owner, count );
    }
    else
    {
        mutex->type = INPROC_SYNC_MUTEX;
        mutex->fd   = ioctl( get_inproc_device_fd(), NTSYNC_IOC_CREATE_MUTEX, &args );
    }
    list_add_tail( &inproc_mutexes, &mutex->entry );

    if (mutex->fd == -1)
    {
        set_error( STATUS_TOO_MANY_OPENED_FILES );
        release_object( mutex );
        return NULL;
    }
    return mutex;
}

struct inproc_sync *create_inproc_semaphore_sync( unsigned int initial, unsigned int max )
{
    struct ntsync_sem_args args = {.count = initial, .max = max};
    struct inproc_sync *sem;

    if (!(sem = alloc_object( &inproc_sync_ops ))) return NULL;

    if (do_fsync())
    {
        sem->type = FSYNC_SEMAPHORE;
        sem->fd = fsync_alloc_shm( initial, max );
    }
    else
    {
        sem->type = INPROC_SYNC_SEMAPHORE;
        sem->fd   = ioctl( get_inproc_device_fd(), NTSYNC_IOC_CREATE_SEM, &args );
    }
    list_init( &sem->entry );

    if (sem->fd == -1)
    {
        set_error( STATUS_TOO_MANY_OPENED_FILES );
        release_object( sem );
        return NULL;
    }
    return sem;
}

static void inproc_sync_dump( struct object *obj, int verbose )
{
    struct inproc_sync *sync = (struct inproc_sync *)obj;
    assert( obj->ops == &inproc_sync_ops );
    fprintf( stderr, "Inproc sync type=%d, fd=%d\n", sync->type, sync->fd );
}

void signal_inproc_sync( struct inproc_sync *sync )
{
    __u32 count;
    if (debug_level) fprintf( stderr, "set_inproc_event %d\n", sync->fd );
    if (do_fsync()) fsync_set_event( sync->fd );
    else            ioctl( sync->fd, NTSYNC_IOC_EVENT_SET, &count );
}

void reset_inproc_sync( struct inproc_sync *sync )
{
    __u32 count;
    if (debug_level) fprintf( stderr, "reset_inproc_event %d\n", sync->fd );
    if (do_fsync()) fsync_reset_event( sync->fd );
    else            ioctl( sync->fd, NTSYNC_IOC_EVENT_RESET, &count );
}

static int inproc_sync_signal( struct object *obj, unsigned int access, int signal )
{
    struct inproc_sync *sync = (struct inproc_sync *)obj;
    assert( obj->ops == &inproc_sync_ops );

    if (!do_fsync())
        assert( sync->type == INPROC_SYNC_INTERNAL || sync->type == INPROC_SYNC_EVENT ); /* never called for mutex / semaphore */
    assert( signal == 0 || signal == 1 ); /* never called from signal_object */

    if (signal) signal_inproc_sync( sync );
    else reset_inproc_sync( sync );
    return 1;
}

static void inproc_sync_destroy( struct object *obj )
{
    struct inproc_sync *sync = (struct inproc_sync *)obj;
    assert( obj->ops == &inproc_sync_ops );
    list_remove( &sync->entry );
    if (do_fsync()) fsync_free_shm_idx( sync->fd );
    else            close( sync->fd );
}

void abandon_inproc_mutexes( thread_id_t tid )
{
    struct inproc_sync *mutex;

    if (do_fsync())
    {
        LIST_FOR_EACH_ENTRY( mutex, &inproc_mutexes, struct inproc_sync, entry )
            fsync_abandon_mutex( mutex->fd, tid );
        return;
    }

    LIST_FOR_EACH_ENTRY( mutex, &inproc_mutexes, struct inproc_sync, entry )
        ioctl( mutex->fd, NTSYNC_IOC_MUTEX_KILL, &tid );
}

static int get_obj_inproc_sync( struct object *obj, int *type )
{
    struct object *sync;
    int fd = -1;

    if (!(sync = get_obj_sync( obj ))) return -1;
    if (sync->ops == &inproc_sync_ops)
    {
        struct inproc_sync *inproc = (struct inproc_sync *)sync;
        *type = inproc->type;
        fd = inproc->fd;
    }

    release_object( sync );
    return fd;
}

#else /* NTSYNC_IOC_EVENT_READ */

int get_inproc_device_fd(void)
{
    return -1;
}

int get_inproc_sync_fd( struct inproc_sync *sync )
{
    return -1;
}

struct inproc_sync *create_inproc_internal_sync( int manual, int signaled )
{
    return NULL;
}

struct inproc_sync *create_inproc_event_sync( int manual, int signaled )
{
    return NULL;
}

struct inproc_sync *create_inproc_mutex_sync( thread_id_t owner, unsigned int count )
{
    return NULL;
}

struct inproc_sync *create_inproc_semaphore_sync( unsigned int initial, unsigned int max )
{
    return NULL;
}

void signal_inproc_sync( struct inproc_sync *sync )
{
}

void reset_inproc_sync( struct inproc_sync *sync )
{
}

void abandon_inproc_mutexes( thread_id_t tid )
{
}

static int get_obj_inproc_sync( struct object *obj, int *type )
{
    return -1;
}

#endif /* NTSYNC_IOC_EVENT_READ */

DECL_HANDLER(get_inproc_sync_fd)
{
    struct object *obj;
    int fd;

    if (!(obj = get_handle_obj( current->process, req->handle, 0, NULL ))) return;

    reply->access = get_handle_access( current->process, req->handle );

    if ((fd = get_obj_inproc_sync( obj, &reply->type )) < 0) set_error( STATUS_NOT_IMPLEMENTED );
    else if (do_fsync()) reply->fsync_shm_idx = fsync_grab_shm_idx( fd );
    else send_client_fd( current->process, fd, req->handle );

    release_object( obj );
}
