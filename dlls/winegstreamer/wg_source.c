/*
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
#include <stdio.h>

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/audio/audio.h>
#include <gst/base/base.h>
#include <gst/tag/tag.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "winternl.h"
#include "mferror.h"

#include "unix_private.h"

struct wg_source
{
    GstElement *container;
};

static struct wg_source *get_source(wg_source_t source)
{
    return (struct wg_source *)(ULONG_PTR)source;
}

NTSTATUS wg_source_create(void *args)
{
    struct wg_source_create_params *params = args;
    struct wg_source *source;

    if (!(source = calloc(1, sizeof(*source))))
        return STATUS_UNSUCCESSFUL;

    if (!(source->container = gst_bin_new("wg_source")))
        goto error;

    gst_element_set_state(source->container, GST_STATE_PAUSED);
    if (!gst_element_get_state(source->container, NULL, NULL, -1))
        goto error;

    params->source = (wg_source_t)(ULONG_PTR)source;
    GST_INFO("Created winegstreamer source %p.", source);
    return STATUS_SUCCESS;

error:
    if (source->container)
    {
        gst_element_set_state(source->container, GST_STATE_NULL);
        gst_object_unref(source->container);
    }
    free(source);

    GST_ERROR("Failed to create winegstreamer source.");
    return STATUS_UNSUCCESSFUL;
}

NTSTATUS wg_source_destroy(void *args)
{
    struct wg_source *source = get_source(*(wg_source_t *)args);

    GST_TRACE("source %p", source);

    gst_element_set_state(source->container, GST_STATE_NULL);
    gst_object_unref(source->container);
    free(source);

    return STATUS_SUCCESS;
}
