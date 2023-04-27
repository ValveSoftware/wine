/*
 * winegstreamer Unix library interface
 *
 * Copyright 2020-2021 Zebediah Figura for CodeWeavers
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

#define GLIB_VERSION_MIN_REQUIRED GLIB_VERSION_2_30
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/audio/audio.h>
#include <gst/base/base.h>
#include <gst/tag/tag.h>
#include <gst/gl/gl.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "winternl.h"
#include "dshow.h"

#include "unix_private.h"

/* GStreamer callbacks may be called on threads not created by Wine, and
 * therefore cannot access the Wine TEB. This means that we must use GStreamer
 * debug logging instead of Wine debug logging. In order to be safe we forbid
 * any use of Wine debug logging in this entire file. */

GST_DEBUG_CATEGORY(wine);

GstGLDisplay *gl_display;

GstStreamType stream_type_from_caps(GstCaps *caps)
{
    const gchar *media_type;

    if (!caps || !gst_caps_get_size(caps))
        return GST_STREAM_TYPE_UNKNOWN;

    media_type = gst_structure_get_name(gst_caps_get_structure(caps, 0));
    if (g_str_has_prefix(media_type, "video/")
            || g_str_has_prefix(media_type, "image/"))
        return GST_STREAM_TYPE_VIDEO;
    if (g_str_has_prefix(media_type, "audio/"))
        return GST_STREAM_TYPE_AUDIO;
    if (g_str_has_prefix(media_type, "text/")
            || g_str_has_prefix(media_type, "subpicture/")
            || g_str_has_prefix(media_type, "closedcaption/"))
        return GST_STREAM_TYPE_TEXT;

    return GST_STREAM_TYPE_UNKNOWN;
}

GstElement *create_element(const char *name, const char *plugin_set)
{
    GstElement *element;

    if (!(element = gst_element_factory_make(name, NULL)))
        fprintf(stderr, "winegstreamer: failed to create %s, are %u-bit GStreamer \"%s\" plugins installed?\n",
                name, 8 * (unsigned int)sizeof(void *), plugin_set);
    return element;
}

GstElement *find_element(GstElementFactoryListType type, GstCaps *src_caps, GstCaps *sink_caps)
{
    GstElement *element = NULL;
    GList *tmp, *transforms;
    const gchar *name;

    if (!(transforms = gst_element_factory_list_get_elements(type, GST_RANK_MARGINAL)))
        goto done;

    tmp = gst_element_factory_list_filter(transforms, src_caps, GST_PAD_SINK, FALSE);
    gst_plugin_feature_list_free(transforms);
    if (!(transforms = tmp))
        goto done;

    tmp = gst_element_factory_list_filter(transforms, sink_caps, GST_PAD_SRC, FALSE);
    gst_plugin_feature_list_free(transforms);
    if (!(transforms = tmp))
        goto done;

    transforms = g_list_sort(transforms, gst_plugin_feature_rank_compare_func);
    for (tmp = transforms; tmp != NULL && element == NULL; tmp = tmp->next)
    {
        name = gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(tmp->data));
        if (!(element = gst_element_factory_create(GST_ELEMENT_FACTORY(tmp->data), NULL)))
            GST_WARNING("Failed to create %s element.", name);
    }
    gst_plugin_feature_list_free(transforms);

done:
    if (element)
    {
        GST_DEBUG("Created %s element %p.", name, element);
    }
    else
    {
        gchar *src_str = gst_caps_to_string(src_caps), *sink_str = gst_caps_to_string(sink_caps);
        GST_WARNING("Failed to create element matching caps %s / %s.", src_str, sink_str);
        g_free(sink_str);
        g_free(src_str);
    }

    return element;
}

bool append_element(GstElement *container, GstElement *element, GstElement **first, GstElement **last)
{
    gchar *name = gst_element_get_name(element);
    bool success = false;

    if (!gst_bin_add(GST_BIN(container), element) ||
            !gst_element_sync_state_with_parent(element) ||
            (*last && !gst_element_link(*last, element)))
    {
        GST_ERROR("Failed to link %s element.", name);
    }
    else
    {
        GST_DEBUG("Linked %s element %p.", name, element);
        if (!*first)
            *first = element;
        *last = element;
        success = true;
    }

    g_free(name);
    return success;
}

bool link_src_to_element(GstPad *src_pad, GstElement *element)
{
    GstPadLinkReturn ret;
    GstPad *sink_pad;

    if (!(sink_pad = gst_element_get_static_pad(element, "sink")))
    {
        gchar *name = gst_element_get_name(element);
        GST_ERROR("Failed to find sink pad on %s", name);
        g_free(name);
        return false;
    }
    if ((ret = gst_pad_link(src_pad, sink_pad)))
    {
        gchar *src_name = gst_pad_get_name(src_pad), *sink_name = gst_pad_get_name(sink_pad);
        GST_ERROR("Failed to link element pad %s with pad %s", src_name, sink_name);
        g_free(sink_name);
        g_free(src_name);
    }
    gst_object_unref(sink_pad);
    return !ret;
}

bool link_element_to_sink(GstElement *element, GstPad *sink_pad)
{
    GstPadLinkReturn ret;
    GstPad *src_pad;

    if (!(src_pad = gst_element_get_static_pad(element, "src")))
    {
        gchar *name = gst_element_get_name(element);
        GST_ERROR("Failed to find src pad on %s", name);
        g_free(name);
        return false;
    }
    if ((ret = gst_pad_link(src_pad, sink_pad)))
    {
        gchar *src_name = gst_pad_get_name(src_pad), *sink_name = gst_pad_get_name(sink_pad);
        GST_ERROR("Failed to link pad %s with element pad %s", src_name, sink_name);
        g_free(sink_name);
        g_free(src_name);
    }
    gst_object_unref(src_pad);
    return !ret;
}

GstCaps *detect_caps_from_data(const char *url, const void *data, guint size)
{
    const char *extension = url ? strrchr(url, '.') : NULL;
    GstTypeFindProbability probability;
    GstCaps *caps;
    gchar *str;

    if (!(caps = gst_type_find_helper_for_data_with_extension(NULL, data, size,
            extension ? extension + 1 : NULL, &probability)))
    {
        GST_ERROR("Failed to detect caps for url %s, data %p, size %u", url, data, size);
        return NULL;
    }

    str = gst_caps_to_string(caps);
    if (probability > GST_TYPE_FIND_POSSIBLE)
        GST_INFO("Detected caps %s with probability %u for url %s, data %p, size %u",
                str, probability, url, data, size);
    else
        GST_FIXME("Detected caps %s with probability %u for url %s, data %p, size %u",
                str, probability, url, data, size);
    g_free(str);

    return caps;
}

GstPad *create_pad_with_caps(GstPadDirection direction, GstCaps *caps)
{
    GstCaps *pad_caps = caps ? gst_caps_ref(caps) : gst_caps_new_any();
    const char *name = direction == GST_PAD_SRC ? "src" : "sink";
    GstPadTemplate *template;
    GstPad *pad;

    if (!pad_caps || !(template = gst_pad_template_new(name, direction, GST_PAD_ALWAYS, pad_caps)))
        return NULL;
    pad = gst_pad_new_from_template(template, "src");
    g_object_unref(template);
    gst_caps_unref(pad_caps);
    return pad;
}

GstBuffer *create_buffer_from_bytes(const void *data, guint size)
{
    GstBuffer *buffer;

    if (!(buffer = gst_buffer_new_and_alloc(size)))
        GST_ERROR("Failed to allocate buffer for %#x bytes\n", size);
    else
    {
        gst_buffer_fill(buffer, 0, data, size);
        gst_buffer_set_size(buffer, size);
    }

    return buffer;
}

NTSTATUS wg_init_gstreamer(void *arg)
{
    static GstGLContext *gl_context;

    char arg0[] = "wine";
    char arg1[] = "--gst-disable-registry-fork";
    char *args[] = {arg0, arg1, NULL};
    int argc = ARRAY_SIZE(args) - 1;
    char **argv = args;
    GError *err;

    const char *e;

    if ((e = getenv("WINE_GST_REGISTRY_DIR")))
    {
        char gst_reg[PATH_MAX];
#if defined(__x86_64__)
        const char *arch = "/registry.x86_64.bin";
#elif defined(__i386__)
        const char *arch = "/registry.i386.bin";
#else
#error Bad arch
#endif
        strcpy(gst_reg, e);
        strcat(gst_reg, arch);
        setenv("GST_REGISTRY_1_0", gst_reg, 1);
    }

    if (!gst_init_check(&argc, &argv, &err))
    {
        fprintf(stderr, "winegstreamer: failed to initialize GStreamer: %s\n", err->message);
        g_error_free(err);
        return STATUS_UNSUCCESSFUL;
    }

    GST_DEBUG_CATEGORY_INIT(wine, "WINE", GST_DEBUG_FG_RED, "Wine GStreamer support");

    GST_INFO("GStreamer library version %s; wine built with %d.%d.%d.",
            gst_version_string(), GST_VERSION_MAJOR, GST_VERSION_MINOR, GST_VERSION_MICRO);

    if (!(gl_display = gst_gl_display_new()))
        GST_ERROR("Failed to create OpenGL display");
    else
    {
        GError *error = NULL;
        gboolean ret;

        GST_OBJECT_LOCK(gl_display);
        ret = gst_gl_display_create_context(gl_display, NULL, &gl_context, &error);
        GST_OBJECT_UNLOCK(gl_display);
        g_clear_error(&error);

        if (ret)
            gst_gl_display_add_context(gl_display, gl_context);
        else
        {
            GST_ERROR("Failed to create OpenGL context");
            gst_object_unref(gl_display);
            gl_display = NULL;
        }
    }

    return STATUS_SUCCESS;
}
