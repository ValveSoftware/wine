/*
 * GStreamer task pool
 *
 * Copyright 2023 Rémi Bernon for CodeWeavers
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
#include <stdarg.h>

#include <gst/gst.h>

#include "unix_private.h"

typedef struct
{
    GstTaskPool parent;
} WgTaskPool;

typedef struct
{
    GstTaskPoolClass parent_class;
} WgTaskPoolClass;

G_DEFINE_TYPE(WgTaskPool, wg_task_pool, GST_TYPE_TASK_POOL);

static void wg_task_pool_prepare(GstTaskPool *pool, GError **error)
{
    GST_LOG("pool %p, error %p", pool, error);
}

static void wg_task_pool_cleanup(GstTaskPool *pool)
{
    GST_LOG("pool %p", pool);
}

static gpointer wg_task_pool_push(GstTaskPool *pool, GstTaskPoolFunction func, gpointer data, GError **error)
{
    pthread_t *tid;
    gint res;

    GST_LOG("pool %p, func %p, data %p, error %p", pool, func, data, error);

    if (!(tid = malloc(sizeof(*tid))) || !(res = pthread_create(tid, NULL, (void *)func, data)))
        return tid;

    g_set_error(error, G_THREAD_ERROR, G_THREAD_ERROR_AGAIN, "Error creating thread: %s", g_strerror(res));
    free(tid);

    return NULL;
}

static void wg_task_pool_join(GstTaskPool *pool, gpointer id)
{
    pthread_t *tid = id;

    GST_LOG("pool %p, id %p", pool, id);

    pthread_join(*tid, NULL);
    free(tid);
}

static void wg_task_pool_class_init(WgTaskPoolClass *klass)
{
    GstTaskPoolClass *parent_class = (GstTaskPoolClass *)klass;
    parent_class->prepare = wg_task_pool_prepare;
    parent_class->cleanup = wg_task_pool_cleanup;
    parent_class->push = wg_task_pool_push;
    parent_class->join = wg_task_pool_join;
}

static void wg_task_pool_init(WgTaskPool *pool)
{
}

GstTaskPool *wg_task_pool_new(void)
{
    return g_object_new(wg_task_pool_get_type(), NULL);
}
