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

#include <stdio.h>
#include <stdarg.h>
#ifdef HAVE_SYS_EVENTFD_H
# include <sys/eventfd.h>
#endif

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winternl.h"

#include "handle.h"
#include "request.h"
#include "file.h"
#include "esync.h"

int do_esync(void)
{
#ifdef HAVE_SYS_EVENTFD_H
    static int do_esync_cached = -1;

    if (do_esync_cached == -1)
        do_esync_cached = (getenv("WINEESYNC") != NULL);

    return do_esync_cached;
#else
    return 0;
#endif
}

struct esync
{
    struct object   obj;    /* object header */
    int             fd;     /* eventfd file descriptor */
    enum esync_type type;
};

static void esync_dump( struct object *obj, int verbose );
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
    NULL,                      /* get_esync_fd */
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

static struct esync *create_esync( struct object *root, const struct unicode_str *name,
    unsigned int attr, int initval, int flags, enum esync_type type,
    const struct security_descriptor *sd )
{
#ifdef HAVE_SYS_EVENTFD_H
    struct esync *esync;

    if ((esync = create_named_object( root, &esync_ops, name, attr, sd )))
    {
        if (get_error() != STATUS_OBJECT_NAME_EXISTS)
        {
            /* initialize it if it didn't already exist */
            esync->fd = eventfd( initval, flags | EFD_CLOEXEC | EFD_NONBLOCK );
            if (esync->fd == -1)
            {
                perror( "eventfd" );
                file_set_error();
                release_object( esync );
                return NULL;
            }
            esync->type = type;
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

/* Wake up a server-side esync object. */
void esync_wake_up( struct object *obj )
{
    static const uint64_t value = 1;
    enum esync_type dummy;
    int fd;

    if (obj->ops->get_esync_fd)
    {
        fd = obj->ops->get_esync_fd( obj, &dummy );

        if (write( fd, &value, sizeof(value) ) == -1)
            perror( "esync: write" );
    }
}

void esync_clear( int fd )
{
    uint64_t value;

    /* we don't care about the return value */
    read( fd, &value, sizeof(value) );
}

/* Server-side event support. */
void esync_set_event( struct esync *esync )
{
    static const uint64_t value = 1;

    assert( esync->obj.ops == &esync_ops );
    if (write( esync->fd, &value, sizeof(value) ) == -1)
        perror( "esync: write" );
}

void esync_reset_event( struct esync *esync )
{
    static uint64_t value = 1;

    assert( esync->obj.ops == &esync_ops );

    /* we don't care about the return value */
    read( esync->fd, &value, sizeof(value) );
}

DECL_HANDLER(create_esync)
{
    struct esync *esync;
    struct unicode_str name;
    struct object *root;
    const struct security_descriptor *sd;
    const struct object_attributes *objattr = get_req_object_attributes( &sd, &name, &root );

    if (!objattr) return;

    if ((esync = create_esync( root, &name, objattr->attributes, req->initval, req->flags, req->type, sd )))
    {
        if (get_error() == STATUS_OBJECT_NAME_EXISTS)
            reply->handle = alloc_handle( current->process, esync, req->access, objattr->attributes );
        else
            reply->handle = alloc_handle_no_access_check( current->process, esync,
                                                          req->access, objattr->attributes );

        reply->type = esync->type;
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
