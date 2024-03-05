/*
 * Copyright 2024 Remi Bernon for CodeWeavers
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

#include "media-converter.h"

GST_DEBUG_CATEGORY_EXTERN(media_converter_debug);
#undef GST_CAT_DEFAULT
#define GST_CAT_DEFAULT media_converter_debug

typedef struct
{
    GstBin bin;
    GstElement *video_conv, *demuxer;
    GstPad *sink_pad; /* Ghost pad. */
    GstPad *inner_sink, *inner_src;
} ProtonDemuxer;

typedef struct
{
    GstBinClass class;
} ProtonDemuxerClass;

G_DEFINE_TYPE(ProtonDemuxer, proton_demuxer, GST_TYPE_BIN);
#define PROTON_DEMUXER_TYPE (proton_demuxer_get_type())
#define PROTON_DEMUXER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), PROTON_DEMUXER_TYPE, ProtonDemuxer))
GST_ELEMENT_REGISTER_DEFINE(protondemuxer, "protondemuxer", GST_RANK_MARGINAL, PROTON_DEMUXER_TYPE);

static GstStaticPadTemplate proton_demuxer_sink_template = GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK,
        GST_PAD_ALWAYS, GST_STATIC_CAPS("video/x-ms-asf; video/x-msvideo; video/mpeg; video/quicktime;"));
static GstStaticPadTemplate proton_demuxer_src_template = GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC,
        GST_PAD_ALWAYS, GST_STATIC_CAPS_ANY);

static gboolean proton_demuxer_sink_event(GstPad *pad, GstObject *parent, GstEvent *event)
{
    GST_DEBUG_OBJECT(pad, "Got sink event %"GST_PTR_FORMAT".", event);
    return gst_pad_event_default(pad, parent, event);
}

static void proton_demuxer_pad_added(GstElement *element, GstPad *pad, gpointer user)
{
    ProtonDemuxer *bin = PROTON_DEMUXER(user);
    GstPad *ghost_src, *src_pad;
    GstElement *decoder = NULL;
    GstEvent *event;
    GstCaps *caps;

    GST_DEBUG_OBJECT(element, "Got inner pad added %"GST_PTR_FORMAT".", pad);

    if ((caps = gst_pad_get_current_caps(pad)))
    {
        const char *mime_type = gst_structure_get_name(gst_caps_get_structure(caps, 0));
        GST_DEBUG_OBJECT(element, "Got inner pad caps %"GST_PTR_FORMAT".", caps);

        if (!strcmp(mime_type, "audio/x-vorbis"))
            decoder = create_element("vorbisdec", "base");
        else if (!strcmp(mime_type, "audio/x-opus"))
            decoder = create_element("opusdec", "base");

        gst_caps_unref(caps);
    }

    if (!decoder)
        ghost_src = gst_ghost_pad_new(GST_PAD_NAME(pad), pad);
    else
    {
        gst_bin_add(GST_BIN(bin), decoder);
        link_src_to_element(pad, decoder);

        src_pad = gst_element_get_static_pad(decoder, "src");
        ghost_src = gst_ghost_pad_new(GST_PAD_NAME(src_pad), src_pad);
        gst_object_unref(src_pad);

        gst_element_sync_state_with_parent(decoder);
    }

    if ((event = gst_pad_get_sticky_event(pad, GST_EVENT_STREAM_START, 0)))
    {
        gst_pad_store_sticky_event(ghost_src, event);
        gst_event_unref(event);
    }

    gst_pad_set_active(ghost_src, true);
    gst_element_add_pad(GST_ELEMENT(&bin->bin), ghost_src);
}

static void proton_demuxer_no_more_pads(GstElement *element, gpointer user)
{
    ProtonDemuxer *bin = PROTON_DEMUXER(user);
    GST_DEBUG_OBJECT(element, "Got inner no-more-pads.");
    gst_element_no_more_pads(GST_ELEMENT(&bin->bin));
}

static GstFlowReturn proton_demuxer_inner_sink_chain(GstPad *pad, GstObject *parent, GstBuffer *buffer)
{
    ProtonDemuxer *bin = PROTON_DEMUXER(gst_pad_get_element_private(pad));
    GST_DEBUG_OBJECT(pad, "Got inner sink buffer %"GST_PTR_FORMAT".", buffer);
    return gst_pad_push(bin->inner_src, buffer);
}

static gboolean proton_demuxer_inner_sink_event_caps(ProtonDemuxer *bin, GstEvent *event)
{
    GstCaps *src_caps, *any_caps;
    GstEvent *stream_start;
    GstPad *src;

    gst_event_parse_caps(event, &src_caps);

    if (!bin->demuxer)
    {
        if (!(any_caps = gst_caps_new_any()))
            return false;
        if (!(bin->demuxer = find_element(GST_ELEMENT_FACTORY_TYPE_DECODABLE, src_caps, any_caps)))
        {
            gst_caps_unref(any_caps);
            return false;
        }
        gst_caps_unref(any_caps);
        g_signal_connect(bin->demuxer, "pad-added", G_CALLBACK(proton_demuxer_pad_added), bin);
        g_signal_connect(bin->demuxer, "no-more-pads", G_CALLBACK(proton_demuxer_no_more_pads), bin);

        if ((src = gst_element_get_static_pad(bin->demuxer, "src")))
        {
            GstPad *ghost_src = gst_ghost_pad_new_no_target_from_template(GST_PAD_NAME(src),
               gst_element_get_pad_template(GST_ELEMENT(&bin->bin), "src"));
            gst_ghost_pad_set_target(GST_GHOST_PAD(ghost_src), src);
            gst_element_add_pad(GST_ELEMENT(&bin->bin), ghost_src);
            gst_object_unref(src);

            gst_element_no_more_pads(GST_ELEMENT(&bin->bin));
        }

        gst_bin_add(GST_BIN(bin), bin->demuxer);
        link_src_to_element(bin->inner_src, bin->demuxer);
        gst_pad_set_active(bin->inner_src, true);

        if ((stream_start = gst_pad_get_sticky_event(bin->inner_sink, GST_EVENT_STREAM_START, 0)))
            push_event(bin->inner_src, stream_start);

        gst_element_sync_state_with_parent(bin->demuxer);
    }

    return gst_pad_push_event(bin->inner_src, event);
}

static gboolean proton_demuxer_inner_sink_event(GstPad *pad, GstObject *parent, GstEvent *event)
{
    ProtonDemuxer *bin = PROTON_DEMUXER(gst_pad_get_element_private(pad));

    GST_DEBUG_OBJECT(pad, "Got inner sink event %"GST_PTR_FORMAT".", event);

    if (event->type == GST_EVENT_CAPS)
        return proton_demuxer_inner_sink_event_caps(bin, event);
    if (!bin->demuxer)
        return gst_pad_event_default(pad, parent, event);
    if (event->type == GST_EVENT_STREAM_START)
    {
        GstEvent *stream_start;
        if ((stream_start = gst_pad_get_sticky_event(bin->inner_src, GST_EVENT_STREAM_START, 0)))
            push_event(bin->inner_src, stream_start);
        return gst_pad_event_default(pad, parent, event);
    }

    return gst_pad_push_event(bin->inner_src, event);
}

static gboolean proton_demuxer_inner_src_query(GstPad *pad, GstObject *parent, GstQuery *query)
{
    ProtonDemuxer *bin = PROTON_DEMUXER(gst_pad_get_element_private(pad));

    GST_DEBUG_OBJECT(pad, "Got inner src query %"GST_PTR_FORMAT".", query);

    if (!bin->demuxer)
        return gst_pad_query_default(pad, parent, query);
    return gst_pad_peer_query(bin->inner_sink, query);
}

static gboolean proton_demuxer_inner_src_event(GstPad *pad, GstObject *parent, GstEvent *event)
{
    ProtonDemuxer *bin = PROTON_DEMUXER(gst_pad_get_element_private(pad));
    GST_DEBUG_OBJECT(pad, "Got inner src event %"GST_PTR_FORMAT".", event);
    return gst_pad_push_event(bin->inner_sink, event);
}

static void proton_demuxer_class_init(ProtonDemuxerClass * klass)
{
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

    /* wg_parser autoplugging ordering relies on the element "Proton video converter" name */
    gst_element_class_set_metadata(element_class, "Proton video converter", "Codec/Demuxer", "Demuxes video for Proton",
            "Andrew Eikum <aeikum@codeweavers.com>, Ziqing Hui <zhui@codeweavers.com>");

    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&proton_demuxer_sink_template));
    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&proton_demuxer_src_template));
}

static void proton_demuxer_init(ProtonDemuxer *bin)
{
    GstStaticPadTemplate inner_sink_template = GST_STATIC_PAD_TEMPLATE("inner-sink",
            GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS_ANY);
    GstStaticPadTemplate inner_src_template = GST_STATIC_PAD_TEMPLATE("inner-src",
            GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS_ANY);
    GstElement *element = GST_ELEMENT(bin);
    GstPad *sink;

    bin->sink_pad = gst_ghost_pad_new_no_target_from_template("sink", gst_element_get_pad_template(element, "sink"));
    gst_pad_set_event_function(bin->sink_pad, GST_DEBUG_FUNCPTR(proton_demuxer_sink_event));

    bin->inner_sink = gst_pad_new_from_static_template(&inner_sink_template, "inner-sink");
    gst_pad_set_chain_function(bin->inner_sink, GST_DEBUG_FUNCPTR(proton_demuxer_inner_sink_chain));
    gst_pad_set_event_function(bin->inner_sink, GST_DEBUG_FUNCPTR(proton_demuxer_inner_sink_event));
    gst_pad_set_element_private(bin->inner_sink, bin);

    bin->inner_src = gst_pad_new_from_static_template(&inner_src_template, "inner-src");
    gst_pad_set_query_function(bin->inner_src, GST_DEBUG_FUNCPTR(proton_demuxer_inner_src_query));
    gst_pad_set_event_function(bin->inner_src, GST_DEBUG_FUNCPTR(proton_demuxer_inner_src_event));
    gst_pad_set_element_private(bin->inner_src, bin);

    bin->video_conv = create_element("protonvideoconverter", "protonmediaconverter");
    gst_bin_add(GST_BIN(bin), bin->video_conv);
    link_element_to_sink(bin->video_conv, bin->inner_sink);
    gst_pad_set_active(bin->inner_sink, true);

    sink = gst_element_get_static_pad(bin->video_conv, "sink");
    gst_ghost_pad_set_target(GST_GHOST_PAD(bin->sink_pad), sink);
    gst_object_unref(sink);

    gst_element_add_pad(element, bin->sink_pad);

    GST_INFO("Initialized ProtonDemuxer %"GST_PTR_FORMAT": video_conv %"GST_PTR_FORMAT", demuxer %"GST_PTR_FORMAT", "
            "sink_pad %"GST_PTR_FORMAT".", bin, bin->video_conv, bin->demuxer, bin->sink_pad);
}
