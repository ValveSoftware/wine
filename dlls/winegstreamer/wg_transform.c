/*
 * GStreamer transform backend
 *
 * Copyright 2022 Rémi Bernon for CodeWeavers
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

#include "winternl.h"
#include "dshow.h"

#include "unix_private.h"

GST_DEBUG_CATEGORY_EXTERN(wine);
#define GST_CAT_DEFAULT wine

struct wg_transform
{
    GstPad *my_src, *my_sink;
};

static GstCaps *wg_format_to_caps_xwma(const struct wg_encoded_format *format)
{
    GstBuffer *buffer;
    GstCaps *caps;

    if (format->encoded_type == WG_ENCODED_TYPE_WMA)
        caps = gst_caps_new_empty_simple("audio/x-wma");
    else
        caps = gst_caps_new_empty_simple("audio/x-xma");

    if (format->u.xwma.version)
        gst_caps_set_simple(caps, "wmaversion", G_TYPE_INT, format->u.xwma.version, NULL);
    if (format->u.xwma.bitrate)
        gst_caps_set_simple(caps, "bitrate", G_TYPE_INT, format->u.xwma.bitrate, NULL);
    if (format->u.xwma.rate)
        gst_caps_set_simple(caps, "rate", G_TYPE_INT, format->u.xwma.rate, NULL);
    if (format->u.xwma.depth)
        gst_caps_set_simple(caps, "depth", G_TYPE_INT, format->u.xwma.depth, NULL);
    if (format->u.xwma.channels)
        gst_caps_set_simple(caps, "channels", G_TYPE_INT, format->u.xwma.channels, NULL);
    if (format->u.xwma.block_align)
        gst_caps_set_simple(caps, "block_align", G_TYPE_INT, format->u.xwma.block_align, NULL);

    if (format->u.xwma.codec_data_len)
    {
        buffer = gst_buffer_new_and_alloc(format->u.xwma.codec_data_len);
        gst_buffer_fill(buffer, 0, format->u.xwma.codec_data, format->u.xwma.codec_data_len);
        gst_caps_set_simple(caps, "codec_data", GST_TYPE_BUFFER, buffer, NULL);
        gst_buffer_unref(buffer);
    }

    return caps;
}

static GstCaps *wg_encoded_format_to_caps(const struct wg_encoded_format *format)
{
    switch (format->encoded_type)
    {
        case WG_ENCODED_TYPE_UNKNOWN:
            return NULL;
        case WG_ENCODED_TYPE_WMA:
        case WG_ENCODED_TYPE_XMA:
            return wg_format_to_caps_xwma(format);
    }
    assert(0);
    return NULL;
}

static GstFlowReturn transform_sink_chain_cb(GstPad *pad, GstObject *parent, GstBuffer *buffer)
{
    struct wg_transform *transform = gst_pad_get_element_private(pad);

    GST_INFO("transform %p, buffer %p.", transform, buffer);

    gst_buffer_unref(buffer);

    return GST_FLOW_OK;
}

NTSTATUS wg_transform_destroy(void *args)
{
    struct wg_transform *transform = args;

    if (transform->my_sink)
        g_object_unref(transform->my_sink);
    if (transform->my_src)
        g_object_unref(transform->my_src);

    free(transform);
    return S_OK;
}

NTSTATUS wg_transform_create(void *args)
{
    struct wg_transform_create_params *params = args;
    struct wg_encoded_format input_format = *params->input_format;
    struct wg_format output_format = *params->output_format;
    GstCaps *src_caps, *sink_caps;
    struct wg_transform *transform;
    GstPadTemplate *template;

    if (!init_gstreamer())
        return E_FAIL;

    if (!(transform = calloc(1, sizeof(*transform))))
        return E_OUTOFMEMORY;

    src_caps = wg_encoded_format_to_caps(&input_format);
    assert(src_caps);
    sink_caps = wg_format_to_caps(&output_format);
    assert(sink_caps);

    template = gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, src_caps);
    assert(template);
    transform->my_src = gst_pad_new_from_template(template, "src");
    g_object_unref(template);
    assert(transform->my_src);

    template = gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS, sink_caps);
    assert(template);
    transform->my_sink = gst_pad_new_from_template(template, "sink");
    g_object_unref(template);
    assert(transform->my_sink);

    gst_pad_set_element_private(transform->my_sink, transform);
    gst_pad_set_chain_function(transform->my_sink, transform_sink_chain_cb);

    GST_INFO("Created winegstreamer transform %p.", transform);
    params->transform = transform;

    gst_caps_unref(src_caps);
    gst_caps_unref(sink_caps);

    return S_OK;
}
