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

#ifndef __WINE_WINEGSTREAMER_UNIXLIB_H
#define __WINE_WINEGSTREAMER_UNIXLIB_H

#include <stdint.h>
#include "windef.h"
#include "winternl.h"
#include "wtypes.h"
#include "mmreg.h"

#include "wine/unixlib.h"

typedef UINT32 wg_major_type;
enum wg_major_type
{
    WG_MAJOR_TYPE_UNKNOWN = 0,
    WG_MAJOR_TYPE_AUDIO,
    WG_MAJOR_TYPE_AUDIO_MPEG1,
    WG_MAJOR_TYPE_AUDIO_MPEG4,
    WG_MAJOR_TYPE_AUDIO_WMA,
    WG_MAJOR_TYPE_AUDIO_ENCODED,
    WG_MAJOR_TYPE_VIDEO,
    WG_MAJOR_TYPE_VIDEO_CINEPAK,
    WG_MAJOR_TYPE_VIDEO_H264,
    WG_MAJOR_TYPE_VIDEO_WMV,
    WG_MAJOR_TYPE_VIDEO_INDEO,
    WG_MAJOR_TYPE_VIDEO_MPEG1,
    WG_MAJOR_TYPE_VIDEO_ENCODED,
};

typedef UINT32 wg_audio_format;
enum wg_audio_format
{
    WG_AUDIO_FORMAT_UNKNOWN,

    WG_AUDIO_FORMAT_U8,
    WG_AUDIO_FORMAT_S16LE,
    WG_AUDIO_FORMAT_S24LE,
    WG_AUDIO_FORMAT_S32LE,
    WG_AUDIO_FORMAT_F32LE,
    WG_AUDIO_FORMAT_F64LE,
};

typedef UINT32 wg_video_format;
enum wg_video_format
{
    WG_VIDEO_FORMAT_UNKNOWN,

    WG_VIDEO_FORMAT_BGRA,
    WG_VIDEO_FORMAT_BGRx,
    WG_VIDEO_FORMAT_BGR,
    WG_VIDEO_FORMAT_RGBA,
    WG_VIDEO_FORMAT_RGB15,
    WG_VIDEO_FORMAT_RGB16,

    WG_VIDEO_FORMAT_AYUV,
    WG_VIDEO_FORMAT_I420,
    WG_VIDEO_FORMAT_NV12,
    WG_VIDEO_FORMAT_UYVY,
    WG_VIDEO_FORMAT_YUY2,
    WG_VIDEO_FORMAT_YV12,
    WG_VIDEO_FORMAT_YVYU,

    WG_VIDEO_FORMAT_WMV1,
    WG_VIDEO_FORMAT_WMV2,
    WG_VIDEO_FORMAT_WMV3,
    WG_VIDEO_FORMAT_WMVA,
    WG_VIDEO_FORMAT_WVC1,
};

struct wg_format
{
    wg_major_type major_type;

    union
    {
        /* Valid members for different audio formats:
         *
         * Uncompressed(PCM): channels, channel_mask, rate.
         * MPEG1: channels, rate, layer.
         * MPEG4: payload_type, codec_data_len, codec_data.
         * WMA: channels, rate, bitrate, depth, block_align, version, layer,
         *         payload_type, codec_data_len, codec_data, is_xma.
         * ENCODED: channels, rate, caps. */
        struct
        {
            wg_audio_format format;

            uint32_t channels;
            uint32_t channel_mask; /* In WinMM format. */
            uint32_t rate;
            uint32_t bitrate;
            uint32_t depth;
            uint32_t block_align;
            uint32_t version;
            uint32_t layer;
            uint32_t payload_type;
            uint32_t codec_data_len;
            unsigned char codec_data[64];
            UINT8 is_xma;
            char caps[512];
        } audio;

        /* Valid members for different video formats:
         *
         * Uncompressed(RGB and YUV): width, height, fps_n, fps_d, padding.
         * CINEPAK: width, height, fps_n, fps_d.
         * H264: width, height, fps_n, fps_d, profile, level, codec_data_len, codec_data.
         * WMV: width, height, fps_n, fps_d, codec_data_len, codec_data.
         * INDEO: width, height, fps_n, fps_d, version. */
        struct
        {
            wg_video_format format;

            /* Positive height indicates top-down video; negative height
             * indicates bottom-up video. */
            int32_t width, height;
            uint32_t fps_n, fps_d;
            RECT padding;
            uint32_t profile;
            uint32_t level;
            uint32_t version;
            uint32_t codec_data_len;
            unsigned char codec_data[64];
        } video;
        struct
        {
            int32_t width, height;
            uint32_t fps_n, fps_d;
        } video_mpeg1;
        struct
        {
            int32_t width, height;
            uint32_t fps_n, fps_d;
            char caps[512];
        } video_encoded;
    } u;
};

enum wg_sample_flag
{
    WG_SAMPLE_FLAG_INCOMPLETE = 1,
    WG_SAMPLE_FLAG_HAS_PTS = 2,
    WG_SAMPLE_FLAG_HAS_DURATION = 4,
    WG_SAMPLE_FLAG_SYNC_POINT = 8,
    WG_SAMPLE_FLAG_DISCONTINUITY = 0x10,
};

struct wg_sample
{
    /* timestamp and duration are in 100-nanosecond units. */
    UINT64 pts;
    UINT64 duration;
    LONG refcount; /* unix refcount */
    UINT32 flags;
    UINT32 max_size;
    UINT32 size;
    UINT64 data; /* pointer to user memory */
};

struct wg_parser_buffer
{
    /* pts and duration are in 100-nanosecond units. */
    UINT64 pts, duration;
    UINT32 size;
    UINT32 stream;
    UINT8 discontinuity, preroll, delta, has_pts, has_duration;
};
C_ASSERT(sizeof(struct wg_parser_buffer) == 32);

typedef UINT32 wg_parser_type;
enum wg_parser_type
{
    WG_PARSER_DECODEBIN,
    WG_PARSER_AVIDEMUX,
    WG_PARSER_WAVPARSE,
    WG_PARSER_URIDECODEBIN,
};

typedef UINT64 wg_parser_t;
typedef UINT64 wg_parser_stream_t;
typedef UINT64 wg_source_t;
typedef UINT64 wg_transform_t;
typedef UINT64 wg_muxer_t;

struct wg_init_gstreamer_params
{
    UINT8 trace_on;
    UINT8 warn_on;
    UINT8 err_on;
};

struct wg_parser_create_params
{
    wg_parser_t parser;
    wg_parser_type type;
    UINT8 output_compressed;
    UINT8 use_opengl;
    UINT8 err_on;
    UINT8 warn_on;
};

struct wg_parser_connect_params
{
    wg_parser_t parser;
    const WCHAR *uri;
    UINT64 file_size;
};

struct wg_parser_get_next_read_offset_params
{
    wg_parser_t parser;
    UINT32 size;
    UINT64 offset;
};

struct wg_parser_push_data_params
{
    wg_parser_t parser;
    const void *data;
    UINT32 size;
};

struct wg_parser_get_stream_count_params
{
    wg_parser_t parser;
    UINT32 count;
};

struct wg_parser_get_stream_params
{
    wg_parser_t parser;
    UINT32 index;
    wg_parser_stream_t stream;
};

struct wg_parser_stream_get_preferred_format_params
{
    wg_parser_stream_t stream;
    struct wg_format *format;
};

struct wg_parser_stream_get_codec_format_params
{
    wg_parser_stream_t stream;
    struct wg_format *format;
};

#define STREAM_ENABLE_FLAG_FLIP_RGB 0x1

struct wg_parser_stream_enable_params
{
    wg_parser_stream_t stream;
    const struct wg_format *format;
    uint32_t flags;
};

struct wg_parser_stream_get_buffer_params
{
    wg_parser_t parser;
    wg_parser_stream_t stream;
    struct wg_parser_buffer *buffer;
};

struct wg_parser_stream_copy_buffer_params
{
    wg_parser_stream_t stream;
    void *data;
    UINT32 offset;
    UINT32 size;
};

struct wg_parser_stream_notify_qos_params
{
    wg_parser_stream_t stream;
    UINT8 underflow;
    DOUBLE proportion;
    INT64 diff;
    UINT64 timestamp;
};

struct wg_parser_stream_get_duration_params
{
    wg_parser_stream_t stream;
    UINT64 duration;
};

typedef UINT64 wg_parser_tag;
enum wg_parser_tag
{
    WG_PARSER_TAG_LANGUAGE,
    WG_PARSER_TAG_NAME,
    WG_PARSER_TAG_COUNT
};

struct wg_parser_stream_get_tag_params
{
    wg_parser_stream_t stream;
    wg_parser_tag tag;
    char *buffer;
    UINT32 *size;
};

struct wg_parser_stream_seek_params
{
    wg_parser_stream_t stream;
    DOUBLE rate;
    UINT64 start_pos, stop_pos;
    DWORD start_flags, stop_flags;
};

struct wg_source_create_params
{
    const char *url;
    UINT64 file_size;
    const void *data;
    UINT32 size;
    char mime_type[256];
    wg_source_t source;
};

struct wg_source_get_stream_count_params
{
    wg_source_t source;
    UINT32 stream_count;
};

struct wg_source_get_duration_params
{
    wg_source_t source;
    UINT64 duration;
};

struct wg_source_get_position_params
{
    wg_source_t source;
    UINT64 read_offset;
};

struct wg_source_set_position_params
{
    wg_source_t source;
    UINT64 time;
};

struct wg_source_push_data_params
{
    wg_source_t source;
    const void *data;
    UINT32 size;
};

struct wg_source_read_data_params
{
    wg_source_t source;
    UINT32 index;
    struct wg_sample *sample;
};

struct wg_source_get_stream_format_params
{
    wg_source_t source;
    UINT32 index;
    struct wg_format format;
};

struct wg_source_get_stream_tag_params
{
    wg_source_t source;
    UINT32 index;
    wg_parser_tag tag;
    UINT32 size;
    char *buffer;
};

struct wg_source_set_stream_flags_params
{
    wg_source_t source;
    UINT32 index;
    UINT32 select;
};

struct wg_transform_attrs
{
    UINT32 output_plane_align;
    UINT32 input_queue_length;
    BOOL allow_size_change;
    BOOL low_latency;
};

struct wg_transform_create_params
{
    wg_transform_t transform;
    const struct wg_format *input_format;
    const struct wg_format *output_format;
    const struct wg_transform_attrs *attrs;
};

struct wg_transform_push_data_params
{
    wg_transform_t transform;
    struct wg_sample *sample;
    HRESULT result;
};

struct wg_transform_read_data_params
{
    wg_transform_t transform;
    struct wg_sample *sample;
    struct wg_format *format;
    HRESULT result;
};

struct wg_transform_set_output_format_params
{
    wg_transform_t transform;
    const struct wg_format *format;
};

struct wg_transform_get_status_params
{
    wg_transform_t transform;
    UINT32 accepts_input;
};

struct wg_transform_notify_qos_params
{
    wg_transform_t transform;
    UINT8 underflow;
    DOUBLE proportion;
    INT64 diff;
    UINT64 timestamp;
};

struct wg_muxer_create_params
{
    wg_muxer_t muxer;
    const char *format;
};

struct wg_muxer_add_stream_params
{
    wg_muxer_t muxer;
    UINT32 stream_id;
    const struct wg_format *format;
};

struct wg_muxer_push_sample_params
{
    wg_muxer_t muxer;
    struct wg_sample *sample;
    UINT32 stream_id;
};

struct wg_muxer_read_data_params
{
    wg_muxer_t muxer;
    void *buffer;
    UINT32 size;
    UINT64 offset;
};

enum unix_funcs
{
    unix_wg_init_gstreamer,

    unix_wg_parser_create,
    unix_wg_parser_destroy,

    unix_wg_parser_connect,
    unix_wg_parser_disconnect,

    unix_wg_parser_get_next_read_offset,
    unix_wg_parser_push_data,

    unix_wg_parser_get_stream_count,
    unix_wg_parser_get_stream,

    unix_wg_parser_stream_get_preferred_format,
    unix_wg_parser_stream_get_codec_format,
    unix_wg_parser_stream_enable,
    unix_wg_parser_stream_disable,

    unix_wg_parser_stream_get_buffer,
    unix_wg_parser_stream_copy_buffer,
    unix_wg_parser_stream_release_buffer,
    unix_wg_parser_stream_notify_qos,

    unix_wg_parser_stream_get_duration,
    unix_wg_parser_stream_get_tag,
    unix_wg_parser_stream_seek,

    unix_wg_source_create,
    unix_wg_source_destroy,
    unix_wg_source_get_stream_count,
    unix_wg_source_get_duration,
    unix_wg_source_get_position,
    unix_wg_source_set_position,
    unix_wg_source_push_data,
    unix_wg_source_read_data,
    unix_wg_source_get_stream_format,
    unix_wg_source_get_stream_tag,
    unix_wg_source_set_stream_flags,

    unix_wg_transform_create,
    unix_wg_transform_destroy,
    unix_wg_transform_set_output_format,

    unix_wg_transform_push_data,
    unix_wg_transform_read_data,
    unix_wg_transform_get_status,
    unix_wg_transform_drain,
    unix_wg_transform_flush,
    unix_wg_transform_notify_qos,

    unix_wg_muxer_create,
    unix_wg_muxer_destroy,
    unix_wg_muxer_add_stream,
    unix_wg_muxer_start,
    unix_wg_muxer_push_sample,
    unix_wg_muxer_read_data,
    unix_wg_muxer_finalize,

    unix_wg_funcs_count,
};

#endif /* __WINE_WINEGSTREAMER_UNIXLIB_H */
