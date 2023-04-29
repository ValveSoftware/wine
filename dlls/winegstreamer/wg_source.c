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

    guint64 max_duration;
    GstPad *stream_pads[WG_SOURCE_MAX_STREAMS];
    guint stream_count;
};

static GstStream *source_get_stream(struct wg_source *source, guint index)
{
    return index >= source->stream_count ? NULL : gst_pad_get_stream(source->stream_pads[index]);
}

static GstCaps *source_get_stream_caps(struct wg_source *source, guint index)
{
    GstStream *stream;
    GstCaps *caps;
    if (!(stream = source_get_stream(source, index)))
        return NULL;
    caps = gst_stream_get_caps(stream);
    gst_object_unref(stream);
    return caps;
}

static gboolean src_event_seek(struct wg_source *source, GstEvent *event)
{
    guint32 seqnum = gst_event_get_seqnum(event);
    GstSeekType cur_type, stop_type;
    GstSeekFlags flags;
    GstFormat format;
    gint64 cur, stop;
    gdouble rate;

    gst_event_parse_seek(event, &rate, &format, &flags, &cur_type, &cur, &stop_type, &stop);
    gst_event_unref(event);
    if (format != GST_FORMAT_BYTES)
        return false;

    GST_TRACE("source %p, rate %f, format %s, flags %#x, cur_type %u, cur %#" G_GINT64_MODIFIER "x, "
            "stop_type %u, stop %#" G_GINT64_MODIFIER "x.", source, rate, gst_format_get_name(format),
            flags, cur_type, cur, stop_type, stop);

    if (flags & GST_SEEK_FLAG_FLUSH)
    {
        if (!(event = gst_event_new_flush_start()))
            GST_ERROR("Failed to allocate flush_start event");
        else
        {
            gst_event_set_seqnum(event, seqnum);
            if (!gst_pad_push_event(source->src_pad, event))
                GST_ERROR("Failed to push flush_start event");
        }
    }

    source->segment.start = cur;

    if (flags & GST_SEEK_FLAG_FLUSH)
    {
        if (!(event = gst_event_new_flush_stop(true)))
            GST_ERROR("Failed to allocate flush_stop event");
        else
        {
            gst_event_set_seqnum(event, seqnum);
            if (!gst_pad_push_event(source->src_pad, event))
                GST_ERROR("Failed to push flush_stop event");
        }
        source->valid_segment = false;
    }

    return true;
}

static gboolean src_event_cb(GstPad *pad, GstObject *parent, GstEvent *event)
{
    struct wg_source *source = gst_pad_get_element_private(pad);

    switch (GST_EVENT_TYPE(event))
    {
    case GST_EVENT_SEEK:
        return src_event_seek(source, event);
    default:
        return gst_pad_event_default(pad, parent, event);
    }
}

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

static gboolean src_query_seeking(struct wg_source *source, GstQuery *query)
{
    GstFormat format;

    gst_query_parse_seeking(query, &format, NULL, NULL, NULL);
    GST_TRACE("source %p, format %s", source, gst_format_get_name(format));
    if (format != GST_FORMAT_BYTES)
        return false;

    gst_query_set_seeking(query, GST_FORMAT_BYTES, 1, 0, source->segment.stop);
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
    case GST_QUERY_SEEKING:
        return src_query_seeking(source, query);
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

static gboolean sink_event_caps(struct wg_source *source, GstPad *pad, GstEvent *event)
{
    GstStream *stream;
    GstCaps *caps;
    gchar *str;

    gst_event_parse_caps(event, &caps);
    str = gst_caps_to_string(caps);
    GST_TRACE("source %p, pad %p, caps %s", source, pad, str);
    g_free(str);

    if ((stream = gst_pad_get_stream(pad)))
    {
        gst_stream_set_caps(stream, gst_caps_copy(caps));
        gst_stream_set_stream_type(stream, stream_type_from_caps(caps));
        gst_object_unref(stream);
    }

    gst_event_unref(event);
    return !!stream;
}

static gboolean sink_event_tag(struct wg_source *source, GstPad *pad, GstEvent *event)
{
    GstTagList *new_tags;
    GstStream *stream;

    gst_event_parse_tag(event, &new_tags);
    GST_TRACE("source %p, pad %p, new_tags %p", source, pad, new_tags);

    if ((stream = gst_pad_get_stream(pad)))
    {
        GstTagList *old_tags = gst_stream_get_tags(stream);
        if ((new_tags = gst_tag_list_merge(old_tags, new_tags, GST_TAG_MERGE_REPLACE)))
        {
            gst_stream_set_tags(stream, new_tags);
            gst_tag_list_unref(new_tags);
        }
        if (old_tags)
            gst_tag_list_unref(old_tags);
        gst_object_unref(stream);
    }

    gst_event_unref(event);
    return stream && new_tags;
}

static gboolean sink_event_stream_start(struct wg_source *source, GstPad *pad, GstEvent *event)
{
    guint group, flags;
    GstStream *stream;
    gint64 duration;
    const gchar *id;

    gst_event_parse_stream_start(event, &id);
    gst_event_parse_stream(event, &stream);
    gst_event_parse_stream_flags(event, &flags);
    if (!gst_event_parse_group_id(event, &group))
        group = -1;
    if (gst_pad_peer_query_duration(pad, GST_FORMAT_TIME, &duration) && GST_CLOCK_TIME_IS_VALID(duration))
        source->max_duration = max(source->max_duration, duration);

    GST_TRACE("source %p, pad %p, stream %p, id %s, flags %#x, group %d, duration %" GST_TIME_FORMAT,
            source, pad, stream, id, flags, group, GST_TIME_ARGS(duration));

    gst_event_unref(event);
    return true;
}

static gboolean sink_event_cb(GstPad *pad, GstObject *parent, GstEvent *event)
{
    struct wg_source *source = gst_pad_get_element_private(pad);

    switch (GST_EVENT_TYPE(event))
    {
    case GST_EVENT_CAPS:
        return sink_event_caps(source, pad, event);
    case GST_EVENT_TAG:
        return sink_event_tag(source, pad, event);
    case GST_EVENT_STREAM_START:
        return sink_event_stream_start(source, pad, event);
    default:
        return gst_pad_event_default(pad, parent, event);
    }
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
    char stream_id[256];
    GstFlowReturn ret;
    GstPad *sink_pad;
    GstEvent *event;
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

    snprintf(stream_id, ARRAY_SIZE(stream_id), "wg_source/%03u", index);
    if (!(event = create_stream_start_event(stream_id)))
        GST_ERROR("Failed to create stream event for sink pad %p", sink_pad);
    else
    {
        if ((ret = gst_pad_store_sticky_event(pad, event)) < 0)
            GST_ERROR("Failed to create pad %p stream, ret %d", sink_pad, ret);
        if ((ret = gst_pad_store_sticky_event(sink_pad, event)) < 0)
            GST_ERROR("Failed to create pad %p stream, ret %d", sink_pad, ret);
        gst_event_unref(event);
    }
}

NTSTATUS wg_source_create(void *args)
{
    struct wg_source_create_params *params = args;
    GstElement *first = NULL, *last = NULL, *element;
    GstCaps *src_caps, *any_caps;
    struct wg_source *source;
    const gchar *media_type;
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

    media_type = gst_structure_get_name(gst_caps_get_structure(src_caps, 0));
    if (!strcmp(media_type, "video/quicktime"))
        strcpy(params->mime_type, "video/mp4");
    else if (!strcmp(media_type, "video/x-msvideo"))
        strcpy(params->mime_type, "video/avi");
    else
        lstrcpynA(params->mime_type, media_type, ARRAY_SIZE(params->mime_type));

    if (!(source->container = gst_bin_new("wg_source")))
        goto error;
    if (!(source->src_pad = create_pad_with_caps(GST_PAD_SRC, src_caps)))
        goto error;
    gst_pad_set_element_private(source->src_pad, source);
    gst_pad_set_query_function(source->src_pad, src_query_cb);
    gst_pad_set_event_function(source->src_pad, src_event_cb);

    for (i = 0; i < ARRAY_SIZE(source->stream_pads); i++)
    {
        if (!(source->stream_pads[i] = create_pad_with_caps(GST_PAD_SINK, NULL)))
            goto error;
        gst_pad_set_element_private(source->stream_pads[i], source);
        gst_pad_set_chain_function(source->stream_pads[i], sink_chain_cb);
        gst_pad_set_event_function(source->stream_pads[i], sink_event_cb);
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

NTSTATUS wg_source_get_status(void *args)
{
    struct wg_source_get_status_params *params = args;
    struct wg_source *source = params->source;
    UINT i, stream_count;
    GstCaps *caps;

    GST_TRACE("source %p", source);

    for (i = 0, stream_count = source->stream_count; i < stream_count; i++)
    {
        if (!(caps = source_get_stream_caps(source, i)))
            return STATUS_PENDING;
        gst_caps_unref(caps);
    }

    params->stream_count = stream_count;
    params->duration = source->max_duration / 100;
    params->read_offset = source->segment.start;
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

NTSTATUS wg_source_get_stream_format(void *args)
{
    struct wg_source_get_stream_format_params *params = args;
    struct wg_source *source = params->source;
    guint index = params->index;
    GstCaps *caps;

    GST_TRACE("source %p, index %u", source, index);

    if (!(caps = source_get_stream_caps(source, index)))
        return STATUS_UNSUCCESSFUL;
    wg_format_from_caps(&params->format, caps);

    gst_caps_unref(caps);
    return STATUS_SUCCESS;
}

