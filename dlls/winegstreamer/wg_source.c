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

#define WG_SOURCE_MAX_STREAMS 32

struct wg_source
{
    GstPad *src_pad;
    GstElement *container;
    GstSegment segment;
    bool valid_segment;

    GstPad *stream_pads[WG_SOURCE_MAX_STREAMS];
    guint stream_count;
};

static gboolean src_query_duration(struct wg_source *source, GstQuery *query)
{
    GstFormat format;

    gst_query_parse_duration(query, &format, NULL);
    GST_TRACE("source %p, format %s", source, gst_format_get_name(format));
    if (format != GST_FORMAT_BYTES)
        return false;

    gst_query_set_duration(query, format, source->segment.stop);
    return true;
}

static gboolean src_query_scheduling(struct wg_source *source, GstQuery *query)
{
    GST_TRACE("source %p", source);
    gst_query_set_scheduling(query, GST_SCHEDULING_FLAG_SEEKABLE, 1, -1, 0);
    gst_query_add_scheduling_mode(query, GST_PAD_MODE_PUSH);
    return true;
}

static gboolean src_query_cb(GstPad *pad, GstObject *parent, GstQuery *query)
{
    struct wg_source *source = gst_pad_get_element_private(pad);

    switch (GST_QUERY_TYPE(query))
    {
    case GST_QUERY_DURATION:
        return src_query_duration(source, query);
    case GST_QUERY_SCHEDULING:
        return src_query_scheduling(source, query);
    default:
        return gst_pad_query_default(pad, parent, query);
    }
}

static GstFlowReturn sink_chain_cb(GstPad *pad, GstObject *parent, GstBuffer *buffer)
{
    struct wg_soutce *source = gst_pad_get_element_private(pad);
    GST_TRACE("source %p, pad %p, buffer %p.", source, pad, buffer);
    gst_buffer_unref(buffer);
    return GST_FLOW_EOS;
}

static GstEvent *create_stream_start_event(const char *stream_id)
{
    GstStream *stream;
    GstEvent *event;

    if (!(stream = gst_stream_new(stream_id, NULL, GST_STREAM_TYPE_UNKNOWN, 0)))
        return NULL;
    if ((event = gst_event_new_stream_start(stream_id)))
    {
        gst_event_set_stream(event, stream);
        gst_object_unref(stream);
    }

    return event;
}

static void pad_added_cb(GstElement *element, GstPad *pad, gpointer user)
{
    struct wg_source *source = user;
    GstPad *sink_pad;
    guint index;

    GST_TRACE("source %p, element %p, pad %p.", source, element, pad);
    if ((index = source->stream_count++) >= ARRAY_SIZE(source->stream_pads))
    {
        GST_FIXME("Not enough sink pads, need %u", source->stream_count);
        return;
    }

    sink_pad = source->stream_pads[index];
    if (gst_pad_link(pad, sink_pad) < 0 || !gst_pad_set_active(sink_pad, true))
        GST_ERROR("Failed to link new pad to sink pad %p", sink_pad);
}

NTSTATUS wg_source_create(void *args)
{
    struct wg_source_create_params *params = args;
    GstElement *first = NULL, *last = NULL, *element;
    GstCaps *src_caps, *any_caps;
    struct wg_source *source;
    GstEvent *event;
    GstPad *peer;
    guint i;

    if (!(src_caps = detect_caps_from_data(params->url, params->data, params->size)))
        return STATUS_UNSUCCESSFUL;
    if (!(source = calloc(1, sizeof(*source))))
    {
        gst_caps_unref(src_caps);
        return STATUS_UNSUCCESSFUL;
    }
    gst_segment_init(&source->segment, GST_FORMAT_BYTES);
    source->segment.stop = params->file_size;

    if (!(source->container = gst_bin_new("wg_source")))
        goto error;
    if (!(source->src_pad = create_pad_with_caps(GST_PAD_SRC, src_caps)))
        goto error;
    gst_pad_set_element_private(source->src_pad, source);
    gst_pad_set_query_function(source->src_pad, src_query_cb);

    for (i = 0; i < ARRAY_SIZE(source->stream_pads); i++)
    {
        if (!(source->stream_pads[i] = create_pad_with_caps(GST_PAD_SINK, NULL)))
            goto error;
        gst_pad_set_element_private(source->stream_pads[i], source);
        gst_pad_set_chain_function(source->stream_pads[i], sink_chain_cb);
    }

    if (!(any_caps = gst_caps_new_any()))
        goto error;
    if (!(element = find_element(GST_ELEMENT_FACTORY_TYPE_DECODABLE, src_caps, any_caps))
            || !append_element(source->container, element, &first, &last))
    {
        gst_caps_unref(any_caps);
        goto error;
    }
    g_signal_connect(element, "pad-added", G_CALLBACK(pad_added_cb), source);
    gst_caps_unref(any_caps);

    if (!link_src_to_element(source->src_pad, first))
        goto error;
    if (!gst_pad_set_active(source->src_pad, true))
        goto error;

    /* try to link the first output pad, some demuxers only have static pads */
    if ((peer = gst_element_get_static_pad(last, "src")))
    {
        GstPad *sink_pad = source->stream_pads[0];
        if (gst_pad_link(peer, sink_pad) < 0 || !gst_pad_set_active(sink_pad, true))
            GST_ERROR("Failed to link static source pad %p", peer);
        else
            source->stream_count++;
        gst_object_unref(peer);
    }

    gst_element_set_state(source->container, GST_STATE_PAUSED);
    if (!gst_element_get_state(source->container, NULL, NULL, -1))
        goto error;

    if (!(event = create_stream_start_event("wg_source"))
            || !gst_pad_push_event(source->src_pad, event))
        goto error;
    gst_caps_unref(src_caps);

    params->source = source;
    GST_INFO("Created winegstreamer source %p.", source);
    return STATUS_SUCCESS;

error:
    if (source->container)
    {
        gst_element_set_state(source->container, GST_STATE_NULL);
        gst_object_unref(source->container);
    }
    for (i = 0; i < ARRAY_SIZE(source->stream_pads) && source->stream_pads[i]; i++)
        gst_object_unref(source->stream_pads[i]);
    if (source->src_pad)
        gst_object_unref(source->src_pad);
    free(source);

    gst_caps_unref(src_caps);

    GST_ERROR("Failed to create winegstreamer source.");
    return STATUS_UNSUCCESSFUL;
}

NTSTATUS wg_source_destroy(void *args)
{
    struct wg_source *source = args;
    guint i;

    GST_TRACE("source %p", source);

    gst_element_set_state(source->container, GST_STATE_NULL);
    gst_object_unref(source->container);
    for (i = 0; i < ARRAY_SIZE(source->stream_pads); i++)
        gst_object_unref(source->stream_pads[i]);
    gst_object_unref(source->src_pad);
    free(source);

    return STATUS_SUCCESS;
}

NTSTATUS wg_source_push_data(void *args)
{
    struct wg_source_push_data_params *params = args;
    struct wg_source *source = params->source;
    GstFlowReturn ret = GST_FLOW_OK;
    GstBuffer *buffer;
    GstEvent *event;

    GST_TRACE("source %p, data %p, size %#x", source, params->data, params->size);

    if (!source->valid_segment)
    {
        if (!(event = gst_event_new_segment(&source->segment))
                || !gst_pad_push_event(source->src_pad, event))
            GST_ERROR("Failed to push new segment event");
        source->valid_segment = true;
    }

    if (!(buffer = create_buffer_from_bytes(params->data, params->size)))
    {
        GST_WARNING("Failed to allocate buffer for data");
        return STATUS_UNSUCCESSFUL;
    }

    source->segment.start += params->size;
    if ((ret = gst_pad_push(source->src_pad, buffer)) && ret != GST_FLOW_EOS)
    {
        GST_WARNING("Failed to push data buffer, ret %d", ret);
        source->segment.start -= params->size;
        return STATUS_UNSUCCESSFUL;
    }

    if (source->segment.start != source->segment.stop)
        return STATUS_SUCCESS;

    if (!(event = gst_event_new_eos())
        || !gst_pad_push_event(source->src_pad, event))
        GST_WARNING("Failed to push EOS event");

    return STATUS_SUCCESS;
}
