/*
 * Copyright 2024 Ziqing Hui for CodeWeavers
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
    GstElement *audio_conv, *opus_dec, *caps_setter;
    GstPad *sink_pad, *src_pad; /* Ghost pads. */
} AudioConvBin;

typedef struct
{
    GstBinClass class;
} AudioConvBinClass;

G_DEFINE_TYPE(AudioConvBin, audio_conv_bin, GST_TYPE_BIN);
#define AUDIO_CONV_BIN_TYPE (audio_conv_bin_get_type())
#define AUDIO_CONV_BIN(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), AUDIO_CONV_BIN_TYPE, AudioConvBin))
GST_ELEMENT_REGISTER_DEFINE(protonaudioconverterbin, "protonaudioconverterbin",
        GST_RANK_MARGINAL + 1, AUDIO_CONV_BIN_TYPE);

static GstStaticPadTemplate audio_conv_bin_sink_template = GST_STATIC_PAD_TEMPLATE("sink",
        GST_PAD_SINK, GST_PAD_ALWAYS,
        GST_STATIC_CAPS("audio/x-wma;"));

static GstStaticPadTemplate audio_conv_bin_src_template = GST_STATIC_PAD_TEMPLATE("src",
        GST_PAD_SRC, GST_PAD_ALWAYS,
        GST_STATIC_CAPS("audio/x-raw, format=S16LE;"));

static void link_elements(GstElement *src_element, GstElement *sink_element)
{
    if (!gst_element_link(src_element, sink_element))
        GST_ERROR("Failed to link src element %"GST_PTR_FORMAT" to sink element %"GST_PTR_FORMAT".",
                src_element, sink_element);
}

static gboolean audio_conv_bin_sink_event(GstPad *pad, GstObject *parent, GstEvent *event)
{
    AudioConvBin * bin = AUDIO_CONV_BIN(parent);
    GstCaps *caps, *rate_caps;
    GstStructure *structure;
    GstPad *audio_conv_sink;
    gint override_rate;
    gboolean ret;

    GST_DEBUG_OBJECT(pad, "Got sink event %"GST_PTR_FORMAT".", event);

    switch (event->type)
    {
    case GST_EVENT_CAPS:
        gst_event_parse_caps(event, &caps);
        if ((structure = gst_caps_get_structure(caps, 0))
                && gst_structure_get_int(structure, "rate", &override_rate))
        {
            rate_caps = gst_caps_new_simple("audio/x-raw", "rate", G_TYPE_INT, override_rate, NULL);
            g_object_set(bin->caps_setter, "caps", rate_caps, NULL);
        }
        else
        {
            GST_WARNING("Event has no rate.");
        }

        /* Forward on to the real pad. */
        audio_conv_sink = gst_element_get_static_pad(bin->audio_conv, "sink");
        ret = gst_pad_send_event(audio_conv_sink, event);
        gst_object_unref(audio_conv_sink);
        return ret;

    default:
        return gst_pad_event_default(pad, parent, event);
    }
}

static void audio_conv_bin_class_init(AudioConvBinClass * klass)
{
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

    gst_element_class_set_metadata(element_class,
            "Proton audio converter with rate fixup",
            "Codec/Decoder/Audio",
            "Converts audio for Proton, fixing up samplerates",
            "Andrew Eikum <aeikum@codeweavers.com>, Ziqing Hui <zhui@codeweavers.com>");

    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&audio_conv_bin_sink_template));
    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&audio_conv_bin_src_template));
}

static void audio_conv_bin_init(AudioConvBin *bin)
{
    GstElement *element = GST_ELEMENT(bin);
    GstPad *sink, *src;

    bin->sink_pad = gst_ghost_pad_new_no_target_from_template("sink",
            gst_element_get_pad_template(element, "sink"));
    bin->src_pad = gst_ghost_pad_new_no_target_from_template("src",
           gst_element_get_pad_template(element, "src"));
    gst_pad_set_event_function(bin->sink_pad, GST_DEBUG_FUNCPTR(audio_conv_bin_sink_event));

    bin->audio_conv = create_element("protonaudioconverter", "protonmediaconverter");
    bin->opus_dec = create_element("opusdec", "base");
    bin->caps_setter = create_element("capssetter", "good");

    gst_bin_add(GST_BIN(bin), bin->audio_conv);
    gst_bin_add(GST_BIN(bin), bin->opus_dec);
    gst_bin_add(GST_BIN(bin), bin->caps_setter);

    link_elements(bin->audio_conv, bin->opus_dec);
    link_elements(bin->opus_dec, bin->caps_setter);

    sink = gst_element_get_static_pad(bin->audio_conv, "sink");
    src = gst_element_get_static_pad(bin->caps_setter, "src");
    gst_ghost_pad_set_target(GST_GHOST_PAD(bin->sink_pad), sink);
    gst_ghost_pad_set_target(GST_GHOST_PAD(bin->src_pad), src);
    gst_object_unref(src);
    gst_object_unref(sink);

    gst_element_add_pad(element, bin->sink_pad);
    gst_element_add_pad(element, bin->src_pad);

    GST_INFO("Initialized AudioConvBin %"GST_PTR_FORMAT": audio_conv %"GST_PTR_FORMAT", opus_dec %"GST_PTR_FORMAT", "
            "caps_setter %"GST_PTR_FORMAT", sink_pad %"GST_PTR_FORMAT", src_pad %"GST_PTR_FORMAT".",
            bin, bin->audio_conv, bin->opus_dec, bin->caps_setter, bin->sink_pad, bin->src_pad);
}
