/*
 * GStreamer splitter + decoder, adapted from parser.c
 *
 * Copyright 2010 Maarten Lankhorst for CodeWeavers
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

#ifndef __GST_PRIVATE_INCLUDED__
#define __GST_PRIVATE_INCLUDED__

#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define COBJMACROS
#define NONAMELESSSTRUCT
#define NONAMELESSUNION
#include "dshow.h"
#include "mfidl.h"
#include "wmsdk.h"
#include "wine/debug.h"
#include "wine/strmbase.h"

#include "unixlib.h"

bool array_reserve(void **elements, size_t *capacity, size_t count, size_t size) DECLSPEC_HIDDEN;

static inline const char *debugstr_time(REFERENCE_TIME time)
{
    ULONGLONG abstime = time >= 0 ? time : -time;
    unsigned int i = 0, j = 0;
    char buffer[23], rev[23];

    while (abstime || i <= 8)
    {
        buffer[i++] = '0' + (abstime % 10);
        abstime /= 10;
        if (i == 7) buffer[i++] = '.';
    }
    if (time < 0) buffer[i++] = '-';

    while (i--) rev[j++] = buffer[i];
    while (rev[j-1] == '0' && rev[j-2] != '.') --j;
    rev[j] = 0;

    return wine_dbg_sprintf("%s", rev);
}

#define MEDIATIME_FROM_BYTES(x) ((LONGLONG)(x) * 10000000)

struct wg_parser *wg_parser_create(enum wg_parser_type type, bool unlimited_buffering) DECLSPEC_HIDDEN;
void wg_parser_destroy(struct wg_parser *parser) DECLSPEC_HIDDEN;

HRESULT wg_parser_connect(struct wg_parser *parser, uint64_t file_size, const WCHAR *uri) DECLSPEC_HIDDEN;
HRESULT wg_parser_connect_unseekable(struct wg_parser *parser, const struct wg_format *in_format,
            uint32_t stream_count, const struct wg_format *out_formats, const struct wg_rect *apertures) DECLSPEC_HIDDEN;
void wg_parser_disconnect(struct wg_parser *parser) DECLSPEC_HIDDEN;

bool wg_parser_get_next_read_offset(struct wg_parser *parser, uint64_t *offset, uint32_t *size) DECLSPEC_HIDDEN;
void wg_parser_push_data(struct wg_parser *parser, enum wg_read_result result, const void *data, uint32_t size) DECLSPEC_HIDDEN;

uint32_t wg_parser_get_stream_count(struct wg_parser *parser) DECLSPEC_HIDDEN;
struct wg_parser_stream *wg_parser_get_stream(struct wg_parser *parser, uint32_t index) DECLSPEC_HIDDEN;

void wg_parser_stream_get_preferred_format(struct wg_parser_stream *stream, struct wg_format *format) DECLSPEC_HIDDEN;
void wg_parser_stream_enable(struct wg_parser_stream *stream, const struct wg_format *format, const struct wg_rect *aperture, uint32_t flags) DECLSPEC_HIDDEN;
void wg_parser_stream_disable(struct wg_parser_stream *stream) DECLSPEC_HIDDEN;

bool wg_parser_stream_get_buffer(struct wg_parser_stream *stream, struct wg_parser_buffer *buffer) DECLSPEC_HIDDEN;
bool wg_parser_stream_copy_buffer(struct wg_parser_stream *stream,
        void *data, uint32_t offset, uint32_t size) DECLSPEC_HIDDEN;
void wg_parser_stream_release_buffer(struct wg_parser_stream *stream) DECLSPEC_HIDDEN;
void wg_parser_stream_notify_qos(struct wg_parser_stream *stream,
        bool underflow, double proportion, int64_t diff, uint64_t timestamp) DECLSPEC_HIDDEN;

/* Returns the duration in 100-nanosecond units. */
uint64_t wg_parser_stream_get_duration(struct wg_parser_stream *stream) DECLSPEC_HIDDEN;
bool wg_parser_stream_get_language(struct wg_parser_stream *stream, char *buffer, uint32_t size) DECLSPEC_HIDDEN;
/* start_pos and stop_pos are in 100-nanosecond units. */
void wg_parser_stream_seek(struct wg_parser_stream *stream, double rate,
        uint64_t start_pos, uint64_t stop_pos, DWORD start_flags, DWORD stop_flags) DECLSPEC_HIDDEN;
bool wg_parser_stream_drain(struct wg_parser_stream *stream) DECLSPEC_HIDDEN;

struct wg_transform *wg_transform_create(const struct wg_encoded_format *input_format,
                const struct wg_format *output_format) DECLSPEC_HIDDEN;
void wg_transform_destroy(struct wg_transform *transform) DECLSPEC_HIDDEN;
HRESULT wg_transform_push_data(struct wg_transform *transform, const void *data, uint32_t size) DECLSPEC_HIDDEN;
HRESULT wg_transform_read_data(struct wg_transform *transform, struct wg_sample *sample) DECLSPEC_HIDDEN;

unsigned int wg_format_get_max_size(const struct wg_format *format);

HRESULT avi_splitter_create(IUnknown *outer, IUnknown **out) DECLSPEC_HIDDEN;
HRESULT decodebin_parser_create(IUnknown *outer, IUnknown **out) DECLSPEC_HIDDEN;
HRESULT mpeg_splitter_create(IUnknown *outer, IUnknown **out) DECLSPEC_HIDDEN;
HRESULT wave_parser_create(IUnknown *outer, IUnknown **out) DECLSPEC_HIDDEN;
HRESULT wma_decoder_create(IUnknown *outer, IUnknown **out) DECLSPEC_HIDDEN;

bool amt_from_wg_format(AM_MEDIA_TYPE *mt, const struct wg_format *format, bool wm);
bool amt_to_wg_format(const AM_MEDIA_TYPE *mt, struct wg_format *format);

BOOL init_gstreamer(void) DECLSPEC_HIDDEN;

extern HRESULT mfplat_get_class_object(REFCLSID rclsid, REFIID riid, void **obj) DECLSPEC_HIDDEN;
extern HRESULT mfplat_DllRegisterServer(void) DECLSPEC_HIDDEN;

IMFMediaType *mf_media_type_from_wg_format(const struct wg_format *format) DECLSPEC_HIDDEN;
void mf_media_type_to_wg_format(IMFMediaType *type, struct wg_format *format) DECLSPEC_HIDDEN;
void mf_media_type_to_wg_encoded_format(IMFMediaType *type, struct wg_encoded_format *format) DECLSPEC_HIDDEN;

HRESULT winegstreamer_stream_handler_create(REFIID riid, void **obj) DECLSPEC_HIDDEN;
HRESULT winegstreamer_create_media_source_from_uri(const WCHAR *uri, IUnknown **out_media_source) DECLSPEC_HIDDEN;

HRESULT aac_decoder_create(REFIID riid, void **ret) DECLSPEC_HIDDEN;
HRESULT h264_decoder_create(REFIID riid, void **ret) DECLSPEC_HIDDEN;
HRESULT audio_converter_create(REFIID riid, void **ret) DECLSPEC_HIDDEN;
HRESULT color_converter_create(REFIID riid, void **ret) DECLSPEC_HIDDEN;
HRESULT gstreamer_scheme_handler_construct(REFIID riid, void **obj) DECLSPEC_HIDDEN;

enum decoder_type
{
    DECODER_TYPE_H264,
    DECODER_TYPE_AAC,
};
HRESULT decode_transform_create(REFIID riid, void **obj, enum decoder_type) DECLSPEC_HIDDEN;

struct wm_stream
{
    struct wm_reader *reader;
    struct wg_parser_stream *wg_stream;
    struct wg_format format;
    WMT_STREAM_SELECTION selection;
    WORD index;
    bool eos;
    bool allocate_output;
    bool allocate_stream;
    /* Note that we only pretend to read compressed samples, and instead output
     * uncompressed samples regardless of whether we are configured to read
     * compressed samples. Rather, the behaviour of the reader objects differs
     * in nontrivial ways depending on this field. */
    bool read_compressed;
};

struct wm_reader
{
    IWMHeaderInfo3 IWMHeaderInfo3_iface;
    IWMLanguageList IWMLanguageList_iface;
    IWMPacketSize2 IWMPacketSize2_iface;
    IWMProfile3 IWMProfile3_iface;
    IWMReaderPlaylistBurn IWMReaderPlaylistBurn_iface;
    IWMReaderTimecode IWMReaderTimecode_iface;
    LONG refcount;
    CRITICAL_SECTION cs;

    QWORD start_time;

    IStream *source_stream;
    HANDLE file;
    HANDLE read_thread;
    bool read_thread_shutdown;
    struct wg_parser *wg_parser;

    struct wm_stream *streams;
    WORD stream_count;

    IWMReaderCallbackAdvanced *callback_advanced;

    const struct wm_reader_ops *ops;
};

struct wm_reader_ops
{
    void *(*query_interface)(struct wm_reader *reader, REFIID iid);
    void (*destroy)(struct wm_reader *reader);
};

void wm_reader_cleanup(struct wm_reader *reader);
HRESULT wm_reader_close(struct wm_reader *reader);
HRESULT wm_reader_get_max_stream_size(struct wm_reader *reader, WORD stream_number, DWORD *size);
HRESULT wm_reader_get_output_format(struct wm_reader *reader, DWORD output,
        DWORD index, IWMOutputMediaProps **props);
HRESULT wm_reader_get_output_format_count(struct wm_reader *reader, DWORD output, DWORD *count);
HRESULT wm_reader_get_output_props(struct wm_reader *reader, DWORD output,
        IWMOutputMediaProps **props);
struct wm_stream *wm_reader_get_stream_by_stream_number(struct wm_reader *reader,
        WORD stream_number);
HRESULT wm_reader_get_stream_sample(struct wm_reader *reader, WORD stream_number,
        INSSBuffer **ret_sample, QWORD *pts, QWORD *duration, DWORD *flags, WORD *ret_stream_number);
HRESULT wm_reader_get_stream_selection(struct wm_reader *reader,
        WORD stream_number, WMT_STREAM_SELECTION *selection);
void wm_reader_init(struct wm_reader *reader, const struct wm_reader_ops *ops);
HRESULT wm_reader_open_file(struct wm_reader *reader, const WCHAR *filename);
HRESULT wm_reader_open_stream(struct wm_reader *reader, IStream *stream);
void wm_reader_seek(struct wm_reader *reader, QWORD start, LONGLONG duration);
HRESULT wm_reader_set_allocate_for_output(struct wm_reader *reader, DWORD output, BOOL allocate);
HRESULT wm_reader_set_allocate_for_stream(struct wm_reader *reader, WORD stream_number, BOOL allocate);
HRESULT wm_reader_set_output_props(struct wm_reader *reader, DWORD output,
        IWMOutputMediaProps *props);
HRESULT wm_reader_set_read_compressed(struct wm_reader *reader,
        WORD stream_number, BOOL compressed);
HRESULT wm_reader_set_streams_selected(struct wm_reader *reader, WORD count,
        const WORD *stream_numbers, const WMT_STREAM_SELECTION *selections);

#endif /* __GST_PRIVATE_INCLUDED__ */
