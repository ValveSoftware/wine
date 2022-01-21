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
    GstElement *container;
    GstPad *my_src, *my_sink;
    GstPad *their_sink, *their_src;
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

    if (transform->container)
        gst_element_set_state(transform->container, GST_STATE_NULL);

    if (transform->their_src && transform->my_sink)
        gst_pad_unlink(transform->their_src, transform->my_sink);
    if (transform->their_sink && transform->my_src)
        gst_pad_unlink(transform->my_src, transform->their_sink);

    if (transform->their_sink)
        g_object_unref(transform->their_sink);
    if (transform->their_src)
        g_object_unref(transform->their_src);

    if (transform->container)
        g_object_unref(transform->container);

    if (transform->my_sink)
        g_object_unref(transform->my_sink);
    if (transform->my_src)
        g_object_unref(transform->my_src);

    free(transform);
    return S_OK;
}

static GstElement *try_create_transform(GstCaps *src_caps, GstCaps *sink_caps)
{
    GstElement *element = NULL;
    GList *tmp, *transforms;
    gchar *type;

    transforms = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_ANY,
            GST_RANK_MARGINAL);

    tmp = gst_element_factory_list_filter(transforms, src_caps, GST_PAD_SINK, FALSE);
    gst_plugin_feature_list_free(transforms);
    transforms = tmp;

    tmp = gst_element_factory_list_filter(transforms, sink_caps, GST_PAD_SRC, FALSE);
    gst_plugin_feature_list_free(transforms);
    transforms = tmp;

    transforms = g_list_sort(transforms, gst_plugin_feature_rank_compare_func);
    for (tmp = transforms; tmp != NULL && element == NULL; tmp = tmp->next)
    {
        type = gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(tmp->data));
        element = gst_element_factory_create(GST_ELEMENT_FACTORY(tmp->data), NULL);
        if (!element)
            GST_WARNING("Failed to create %s element.", type);
    }
    gst_plugin_feature_list_free(transforms);

    if (element)
        GST_INFO("Created %s element %p.", type, element);
    else
    {
        gchar *src_str = gst_caps_to_string(src_caps), *sink_str = gst_caps_to_string(sink_caps);
        GST_WARNING("Failed to create transform matching caps %s / %s.", src_str, sink_str);
        g_free(sink_str);
        g_free(src_str);
    }

    return element;
}

static bool transform_append_element(struct wg_transform *transform, GstElement *element,
        GstElement **first, GstElement **last)
{
    gchar *name = gst_element_get_name(element);

    if (!gst_bin_add(GST_BIN(transform->container), element))
    {
        GST_ERROR("Failed to add %s element to bin.", name);
        g_free(name);
        return false;
    }

    if (*last && !gst_element_link(*last, element))
    {
        GST_ERROR("Failed to link %s element.", name);
        g_free(name);
        return false;
    }

    GST_INFO("Created %s element %p.", name, element);
    g_free(name);

    if (!*first)
        *first = element;

    *last = element;
    return true;
}

NTSTATUS wg_transform_create(void *args)
{
    struct wg_transform_create_params *params = args;
    struct wg_encoded_format input_format = *params->input_format;
    struct wg_format output_format = *params->output_format;
    GstElement *first = NULL, *last = NULL, *element;
    struct wg_transform *transform;
    GstCaps *src_caps, *sink_caps;
    GstPadTemplate *template;
    int ret;

    if (!init_gstreamer())
        return E_FAIL;

    if (!(transform = calloc(1, sizeof(*transform))))
        return E_OUTOFMEMORY;

    src_caps = wg_encoded_format_to_caps(&input_format);
    assert(src_caps);
    sink_caps = wg_format_to_caps(&output_format);
    assert(sink_caps);

    transform->container = gst_bin_new("wg_transform");
    assert(transform->container);

    if (!(element = try_create_transform(src_caps, sink_caps)) ||
            !transform_append_element(transform, element, &first, &last))
        goto failed;

    if (!(transform->their_sink = gst_element_get_static_pad(first, "sink")))
    {
        GST_ERROR("Failed to find target sink pad.");
        goto failed;
    }
    if (!(transform->their_src = gst_element_get_static_pad(last, "src")))
    {
        GST_ERROR("Failed to find target src pad.");
        goto failed;
    }

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

    if ((ret = gst_pad_link(transform->my_src, transform->their_sink)) < 0)
    {
        GST_ERROR("Failed to link sink pads, error %d.", ret);
        goto failed;
    }
    if ((ret = gst_pad_link(transform->their_src, transform->my_sink)) < 0)
    {
        GST_ERROR("Failed to link source pads, error %d.", ret);
        goto failed;
    }

    if (!(ret = gst_pad_set_active(transform->my_sink, 1)))
        GST_WARNING("Failed to activate my_sink.");
    if (!(ret = gst_pad_set_active(transform->my_src, 1)))
        GST_WARNING("Failed to activate my_src.");

    gst_element_set_state(transform->container, GST_STATE_PAUSED);
    ret = gst_element_get_state(transform->container, NULL, NULL, -1);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        GST_ERROR("Failed to play stream.\n");
        goto failed;
    }

    GST_INFO("Created winegstreamer transform %p.", transform);
    params->transform = transform;

failed:
    gst_caps_unref(src_caps);
    gst_caps_unref(sink_caps);

    if (params->transform)
        return S_OK;

    wg_transform_destroy(transform);
    return E_FAIL;
}
