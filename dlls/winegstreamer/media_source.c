/* GStreamer Media Source
 *
 * Copyright 2020 Derek Lesho
 * Copyright 2020 Zebediah Figura for CodeWeavers
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

#include "gst_private.h"

#include "mfapi.h"
#include "mferror.h"

#include "wine/list.h"

WINE_DEFAULT_DEBUG_CHANNEL(mfplat);

struct media_stream
{
    IMFMediaStream IMFMediaStream_iface;
    LONG ref;

    IMFMediaSource *media_source;
    IMFMediaEventQueue *event_queue;
    IMFStreamDescriptor *descriptor;

    struct wg_parser_stream *wg_stream;

    IUnknown **token_queue;
    LONG token_queue_count;
    LONG token_queue_cap;

    DWORD stream_id;
    BOOL active;
    BOOL eos;
};

enum source_async_op
{
    SOURCE_ASYNC_START,
    SOURCE_ASYNC_PAUSE,
    SOURCE_ASYNC_STOP,
    SOURCE_ASYNC_REQUEST_SAMPLE,
};

struct source_async_command
{
    IUnknown IUnknown_iface;
    LONG refcount;
    enum source_async_op op;
    union
    {
        struct
        {
            IMFPresentationDescriptor *descriptor;
            GUID format;
            PROPVARIANT position;
        } start;
        struct
        {
            struct media_stream *stream;
            IUnknown *token;
        } request_sample;
    } u;
};

struct media_source
{
    IMFMediaSource IMFMediaSource_iface;
    IMFGetService IMFGetService_iface;
    IMFRateSupport IMFRateSupport_iface;
    IMFRateControl IMFRateControl_iface;
    IMFAsyncCallback async_commands_callback;
    LONG ref;
    DWORD async_commands_queue;
    IMFMediaEventQueue *event_queue;
    IMFByteStream *byte_stream;

    CRITICAL_SECTION cs;

    struct wg_parser *wg_parser;

    struct media_stream **streams;
    ULONG stream_count;
    IMFPresentationDescriptor *pres_desc;
    enum
    {
        SOURCE_OPENING,
        SOURCE_STOPPED,
        SOURCE_PAUSED,
        SOURCE_RUNNING,
        SOURCE_SHUTDOWN,
    } state;
    float rate;

    HANDLE read_thread;
    bool read_thread_shutdown;
};

static inline struct media_stream *impl_from_IMFMediaStream(IMFMediaStream *iface)
{
    return CONTAINING_RECORD(iface, struct media_stream, IMFMediaStream_iface);
}

static inline struct media_source *impl_from_IMFMediaSource(IMFMediaSource *iface)
{
    return CONTAINING_RECORD(iface, struct media_source, IMFMediaSource_iface);
}

static inline struct media_source *impl_from_IMFGetService(IMFGetService *iface)
{
    return CONTAINING_RECORD(iface, struct media_source, IMFGetService_iface);
}

static inline struct media_source *impl_from_IMFRateSupport(IMFRateSupport *iface)
{
    return CONTAINING_RECORD(iface, struct media_source, IMFRateSupport_iface);
}

static inline struct media_source *impl_from_IMFRateControl(IMFRateControl *iface)
{
    return CONTAINING_RECORD(iface, struct media_source, IMFRateControl_iface);
}

static inline struct media_source *impl_from_async_commands_callback_IMFAsyncCallback(IMFAsyncCallback *iface)
{
    return CONTAINING_RECORD(iface, struct media_source, async_commands_callback);
}

static inline struct source_async_command *impl_from_async_command_IUnknown(IUnknown *iface)
{
    return CONTAINING_RECORD(iface, struct source_async_command, IUnknown_iface);
}

static HRESULT WINAPI source_async_command_QueryInterface(IUnknown *iface, REFIID riid, void **obj)
{
    if (IsEqualIID(riid, &IID_IUnknown))
    {
        *obj = iface;
        IUnknown_AddRef(iface);
        return S_OK;
    }

    WARN("Unsupported interface %s.\n", debugstr_guid(riid));
    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI source_async_command_AddRef(IUnknown *iface)
{
    struct source_async_command *command = impl_from_async_command_IUnknown(iface);
    return InterlockedIncrement(&command->refcount);
}

static ULONG WINAPI source_async_command_Release(IUnknown *iface)
{
    struct source_async_command *command = impl_from_async_command_IUnknown(iface);
    ULONG refcount = InterlockedDecrement(&command->refcount);

    if (!refcount)
    {
        if (command->op == SOURCE_ASYNC_START)
            PropVariantClear(&command->u.start.position);
        else if (command->op == SOURCE_ASYNC_REQUEST_SAMPLE)
        {
            if (command->u.request_sample.token)
                IUnknown_Release(command->u.request_sample.token);
        }
        free(command);
    }

    return refcount;
}

static const IUnknownVtbl source_async_command_vtbl =
{
    source_async_command_QueryInterface,
    source_async_command_AddRef,
    source_async_command_Release,
};

static HRESULT source_create_async_op(enum source_async_op op, struct source_async_command **ret)
{
    struct source_async_command *command;

    if (!(command = calloc(1, sizeof(*command))))
        return E_OUTOFMEMORY;

    command->IUnknown_iface.lpVtbl = &source_async_command_vtbl;
    command->op = op;

    *ret = command;

    return S_OK;
}

static HRESULT WINAPI callback_QueryInterface(IMFAsyncCallback *iface, REFIID riid, void **obj)
{
    TRACE("%p, %s, %p.\n", iface, debugstr_guid(riid), obj);

    if (IsEqualIID(riid, &IID_IMFAsyncCallback) ||
            IsEqualIID(riid, &IID_IUnknown))
    {
        *obj = iface;
        IMFAsyncCallback_AddRef(iface);
        return S_OK;
    }

    WARN("Unsupported %s.\n", debugstr_guid(riid));
    *obj = NULL;
    return E_NOINTERFACE;
}

static HRESULT WINAPI callback_GetParameters(IMFAsyncCallback *iface,
        DWORD *flags, DWORD *queue)
{
    return E_NOTIMPL;
}

static ULONG WINAPI source_async_commands_callback_AddRef(IMFAsyncCallback *iface)
{
    struct media_source *source = impl_from_async_commands_callback_IMFAsyncCallback(iface);
    return IMFMediaSource_AddRef(&source->IMFMediaSource_iface);
}

static ULONG WINAPI source_async_commands_callback_Release(IMFAsyncCallback *iface)
{
    struct media_source *source = impl_from_async_commands_callback_IMFAsyncCallback(iface);
    return IMFMediaSource_Release(&source->IMFMediaSource_iface);
}

static IMFStreamDescriptor *stream_descriptor_from_id(IMFPresentationDescriptor *pres_desc, DWORD id, BOOL *selected)
{
    ULONG sd_count;
    IMFStreamDescriptor *ret;
    unsigned int i;

    if (FAILED(IMFPresentationDescriptor_GetStreamDescriptorCount(pres_desc, &sd_count)))
        return NULL;

    for (i = 0; i < sd_count; i++)
    {
        DWORD stream_id;

        if (FAILED(IMFPresentationDescriptor_GetStreamDescriptorByIndex(pres_desc, i, selected, &ret)))
            return NULL;

        if (SUCCEEDED(IMFStreamDescriptor_GetStreamIdentifier(ret, &stream_id)) && stream_id == id)
            return ret;

        IMFStreamDescriptor_Release(ret);
    }
    return NULL;
}

static BOOL enqueue_token(struct media_stream *stream, IUnknown *token)
{
    if (stream->token_queue_count == stream->token_queue_cap)
    {
        IUnknown **buf;
        stream->token_queue_cap = stream->token_queue_cap * 2 + 1;
        buf = realloc(stream->token_queue, stream->token_queue_cap * sizeof(*buf));
        if (buf)
            stream->token_queue = buf;
        else
        {
            stream->token_queue_cap = stream->token_queue_count;
            return FALSE;
        }
    }
    stream->token_queue[stream->token_queue_count++] = token;
    return TRUE;
}

static void flush_token_queue(struct media_stream *stream, BOOL send)
{
    struct media_source *source = impl_from_IMFMediaSource(stream->media_source);
    LONG i;

    for (i = 0; i < stream->token_queue_count; i++)
    {
        if (send)
        {
            HRESULT hr;
            struct source_async_command *command;
            if (SUCCEEDED(hr = source_create_async_op(SOURCE_ASYNC_REQUEST_SAMPLE, &command)))
            {
                command->u.request_sample.stream = stream;
                command->u.request_sample.token = stream->token_queue[i];

                hr = MFPutWorkItem(source->async_commands_queue, &source->async_commands_callback,
                        &command->IUnknown_iface);
            }
            if (FAILED(hr))
                WARN("Could not enqueue sample request, hr %#lx\n", hr);
        }
        else if (stream->token_queue[i])
            IUnknown_Release(stream->token_queue[i]);
    }
    free(stream->token_queue);
    stream->token_queue = NULL;
    stream->token_queue_count = 0;
    stream->token_queue_cap = 0;
}

static void start_pipeline(struct media_source *source, struct source_async_command *command)
{
    PROPVARIANT *position = &command->u.start.position;
    BOOL seek_message = source->state != SOURCE_STOPPED && position->vt != VT_EMPTY;
    unsigned int i;

    /* seek to beginning on stop->play */
    if (source->state == SOURCE_STOPPED && position->vt == VT_EMPTY)
    {
        position->vt = VT_I8;
        position->hVal.QuadPart = 0;
    }

    for (i = 0; i < source->stream_count; i++)
    {
        struct media_stream *stream;
        IMFStreamDescriptor *sd;
        IMFMediaTypeHandler *mth;
        IMFMediaType *current_mt;
        DWORD stream_id;
        BOOL was_active;
        BOOL selected;

        stream = source->streams[i];

        IMFStreamDescriptor_GetStreamIdentifier(stream->descriptor, &stream_id);

        sd = stream_descriptor_from_id(command->u.start.descriptor, stream_id, &selected);
        IMFStreamDescriptor_Release(sd);

        was_active = stream->active;
        stream->active = selected;

        if (selected)
        {
            struct wg_format format;

            IMFStreamDescriptor_GetMediaTypeHandler(stream->descriptor, &mth);
            IMFMediaTypeHandler_GetCurrentMediaType(mth, &current_mt);

            mf_media_type_to_wg_format(current_mt, &format);
            wg_parser_stream_enable(stream->wg_stream, &format, 0);

            IMFMediaType_Release(current_mt);
            IMFMediaTypeHandler_Release(mth);
        }
        else
        {
            wg_parser_stream_disable(stream->wg_stream);
        }

        if (position->vt != VT_EMPTY)
            stream->eos = FALSE;

        if (selected)
        {
            TRACE("Stream %u (%p) selected\n", i, stream);
            IMFMediaEventQueue_QueueEventParamUnk(source->event_queue,
                was_active ? MEUpdatedStream : MENewStream, &GUID_NULL,
                S_OK, (IUnknown*) &stream->IMFMediaStream_iface);

            IMFMediaEventQueue_QueueEventParamVar(stream->event_queue,
                seek_message ? MEStreamSeeked : MEStreamStarted, &GUID_NULL, S_OK, position);
        }
    }

    IMFMediaEventQueue_QueueEventParamVar(source->event_queue,
        seek_message ? MESourceSeeked : MESourceStarted,
        &GUID_NULL, S_OK, position);

    source->state = SOURCE_RUNNING;

    if (position->vt == VT_I8)
        wg_parser_stream_seek(source->streams[0]->wg_stream, 1.0, position->hVal.QuadPart, 0,
                AM_SEEKING_AbsolutePositioning, AM_SEEKING_NoPositioning);

    for (i = 0; i < source->stream_count; i++)
        flush_token_queue(source->streams[i], position->vt == VT_EMPTY);
}

static void pause_pipeline(struct media_source *source)
{
    unsigned int i;
    HRESULT hr;

    for (i = 0; i < source->stream_count; i++)
    {
        struct media_stream *stream = source->streams[i];
        if (stream->active && FAILED(hr = IMFMediaEventQueue_QueueEventParamVar(stream->event_queue, MEStreamPaused,
                    &GUID_NULL, S_OK, NULL)))
            WARN("Failed to queue MEStreamPaused event, hr %#lx\n", hr);
    }

    IMFMediaEventQueue_QueueEventParamVar(source->event_queue, MESourcePaused, &GUID_NULL, S_OK, NULL);

    source->state = SOURCE_PAUSED;
}

static void stop_pipeline(struct media_source *source)
{
    unsigned int i;
    HRESULT hr;

    for (i = 0; i < source->stream_count; i++)
    {
        struct media_stream *stream = source->streams[i];
        if (stream->active && FAILED(hr = IMFMediaEventQueue_QueueEventParamVar(stream->event_queue, MEStreamStopped,
                    &GUID_NULL, S_OK, NULL)))
            WARN("Failed to queue MEStreamStopped event, hr %#lx\n", hr);
    }

    IMFMediaEventQueue_QueueEventParamVar(source->event_queue, MESourceStopped, &GUID_NULL, S_OK, NULL);

    source->state = SOURCE_STOPPED;

    for (i = 0; i < source->stream_count; i++)
        flush_token_queue(source->streams[i], FALSE);
}

static void dispatch_end_of_presentation(struct media_source *source)
{
    PROPVARIANT empty = {.vt = VT_EMPTY};
    unsigned int i;

    /* A stream has ended, check whether all have */
    for (i = 0; i < source->stream_count; i++)
    {
        struct media_stream *stream = source->streams[i];
        if (stream->active && !stream->eos)
            return;
    }

    IMFMediaEventQueue_QueueEventParamVar(source->event_queue, MEEndOfPresentation, &GUID_NULL, S_OK, &empty);
}

static void send_buffer(struct media_stream *stream, const struct wg_parser_buffer *wg_buffer, IUnknown *token)
{
    IMFMediaBuffer *buffer;
    IMFSample *sample;
    HRESULT hr;
    BYTE *data;

    if (FAILED(hr = MFCreateSample(&sample)))
    {
        ERR("Failed to create sample, hr %#lx.\n", hr);
        return;
    }

    if (FAILED(hr = MFCreateMemoryBuffer(wg_buffer->size, &buffer)))
    {
        ERR("Failed to create buffer, hr %#lx.\n", hr);
        IMFSample_Release(sample);
        return;
    }

    if (FAILED(hr = IMFSample_AddBuffer(sample, buffer)))
    {
        ERR("Failed to add buffer, hr %#lx.\n", hr);
        goto out;
    }

    if (FAILED(hr = IMFMediaBuffer_SetCurrentLength(buffer, wg_buffer->size)))
    {
        ERR("Failed to set size, hr %#lx.\n", hr);
        goto out;
    }

    if (FAILED(hr = IMFMediaBuffer_Lock(buffer, &data, NULL, NULL)))
    {
        ERR("Failed to lock buffer, hr %#lx.\n", hr);
        goto out;
    }

    if (!wg_parser_stream_copy_buffer(stream->wg_stream, data, 0, wg_buffer->size))
    {
        wg_parser_stream_release_buffer(stream->wg_stream);
        IMFMediaBuffer_Unlock(buffer);
        goto out;
    }
    wg_parser_stream_release_buffer(stream->wg_stream);

    if (FAILED(hr = IMFMediaBuffer_Unlock(buffer)))
    {
        ERR("Failed to unlock buffer, hr %#lx.\n", hr);
        goto out;
    }

    if (FAILED(hr = IMFSample_SetSampleTime(sample, wg_buffer->pts)))
    {
        ERR("Failed to set sample time, hr %#lx.\n", hr);
        goto out;
    }

    if (FAILED(hr = IMFSample_SetSampleDuration(sample, wg_buffer->duration)))
    {
        ERR("Failed to set sample duration, hr %#lx.\n", hr);
        goto out;
    }

    if (token)
        IMFSample_SetUnknown(sample, &MFSampleExtension_Token, token);

    IMFMediaEventQueue_QueueEventParamUnk(stream->event_queue, MEMediaSample,
            &GUID_NULL, S_OK, (IUnknown *)sample);

out:
    IMFMediaBuffer_Release(buffer);
    IMFSample_Release(sample);
}

static void wait_on_sample(struct media_stream *stream, IUnknown *token)
{
    struct media_source *source = impl_from_IMFMediaSource(stream->media_source);
    PROPVARIANT empty_var = {.vt = VT_EMPTY};
    struct wg_parser_buffer buffer;

    TRACE("%p, %p\n", stream, token);

    if (wg_parser_stream_get_buffer(source->wg_parser, stream->wg_stream, &buffer))
    {
        send_buffer(stream, &buffer, token);
    }
    else
    {
        stream->eos = TRUE;
        IMFMediaEventQueue_QueueEventParamVar(stream->event_queue, MEEndOfStream, &GUID_NULL, S_OK, &empty_var);
        dispatch_end_of_presentation(source);
    }
}

static HRESULT WINAPI source_async_commands_Invoke(IMFAsyncCallback *iface, IMFAsyncResult *result)
{
    struct media_source *source = impl_from_async_commands_callback_IMFAsyncCallback(iface);
    struct source_async_command *command;
    IUnknown *state;
    HRESULT hr;

    if (FAILED(hr = IMFAsyncResult_GetState(result, &state)))
        return hr;

    EnterCriticalSection(&source->cs);

    command = impl_from_async_command_IUnknown(state);
    switch (command->op)
    {
        case SOURCE_ASYNC_START:
            if (source->state != SOURCE_SHUTDOWN)
                start_pipeline(source, command);
            break;
        case SOURCE_ASYNC_PAUSE:
            if (source->state != SOURCE_SHUTDOWN)
                pause_pipeline(source);
            break;
        case SOURCE_ASYNC_STOP:
            if (source->state != SOURCE_SHUTDOWN)
                stop_pipeline(source);
            break;
        case SOURCE_ASYNC_REQUEST_SAMPLE:
            if (source->state == SOURCE_PAUSED)
                enqueue_token(command->u.request_sample.stream, command->u.request_sample.token);
            else if (source->state == SOURCE_RUNNING)
                wait_on_sample(command->u.request_sample.stream, command->u.request_sample.token);
            break;
    }

    LeaveCriticalSection(&source->cs);

    IUnknown_Release(state);

    return S_OK;
}

static const IMFAsyncCallbackVtbl source_async_commands_callback_vtbl =
{
    callback_QueryInterface,
    source_async_commands_callback_AddRef,
    source_async_commands_callback_Release,
    callback_GetParameters,
    source_async_commands_Invoke,
};

static DWORD CALLBACK read_thread(void *arg)
{
    struct media_source *source = arg;
    IMFByteStream *byte_stream = source->byte_stream;
    size_t buffer_size = 4096;
    uint64_t file_size;
    void *data;

    if (!(data = malloc(buffer_size)))
        return 0;

    IMFByteStream_GetLength(byte_stream, &file_size);

    TRACE("Starting read thread for media source %p.\n", source);

    while (!source->read_thread_shutdown)
    {
        uint64_t offset;
        ULONG ret_size;
        uint32_t size;
        HRESULT hr;

        if (!wg_parser_get_next_read_offset(source->wg_parser, &offset, &size))
            continue;

        if (offset >= file_size)
            size = 0;
        else if (offset + size >= file_size)
            size = file_size - offset;

        /* Some IMFByteStreams (including the standard file-based stream) return
         * an error when reading past the file size. */
        if (!size)
        {
            wg_parser_push_data(source->wg_parser, data, 0);
            continue;
        }

        if (!array_reserve(&data, &buffer_size, size, 1))
        {
            free(data);
            return 0;
        }

        ret_size = 0;

        if (SUCCEEDED(hr = IMFByteStream_SetCurrentPosition(byte_stream, offset)))
            hr = IMFByteStream_Read(byte_stream, data, size, &ret_size);
        if (FAILED(hr))
            ERR("Failed to read %u bytes at offset %I64u, hr %#lx.\n", size, offset, hr);
        else if (ret_size != size)
            ERR("Unexpected short read: requested %u bytes, got %lu.\n", size, ret_size);
        wg_parser_push_data(source->wg_parser, SUCCEEDED(hr) ? data : NULL, ret_size);
    }

    free(data);
    TRACE("Media source is shutting down; exiting.\n");
    return 0;
}

static HRESULT WINAPI media_stream_QueryInterface(IMFMediaStream *iface, REFIID riid, void **out)
{
    struct media_stream *stream = impl_from_IMFMediaStream(iface);

    TRACE("%p, %s, %p.\n", iface, debugstr_guid(riid), out);

    if (IsEqualIID(riid, &IID_IMFMediaStream) ||
        IsEqualIID(riid, &IID_IMFMediaEventGenerator) ||
        IsEqualIID(riid, &IID_IUnknown))
    {
        *out = &stream->IMFMediaStream_iface;
    }
    else
    {
        FIXME("(%s, %p)\n", debugstr_guid(riid), out);
        *out = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*out);
    return S_OK;
}

static ULONG WINAPI media_stream_AddRef(IMFMediaStream *iface)
{
    struct media_stream *stream = impl_from_IMFMediaStream(iface);
    ULONG ref = InterlockedIncrement(&stream->ref);

    TRACE("%p, refcount %lu.\n", iface, ref);

    return ref;
}

static ULONG WINAPI media_stream_Release(IMFMediaStream *iface)
{
    struct media_stream *stream = impl_from_IMFMediaStream(iface);
    ULONG ref = InterlockedDecrement(&stream->ref);

    TRACE("%p, refcount %lu.\n", iface, ref);

    if (!ref)
    {
        IMFMediaSource_Release(stream->media_source);
        IMFStreamDescriptor_Release(stream->descriptor);
        IMFMediaEventQueue_Release(stream->event_queue);
        flush_token_queue(stream, FALSE);
        free(stream);
    }

    return ref;
}

static HRESULT WINAPI media_stream_GetEvent(IMFMediaStream *iface, DWORD flags, IMFMediaEvent **event)
{
    struct media_stream *stream = impl_from_IMFMediaStream(iface);

    TRACE("%p, %#lx, %p.\n", iface, flags, event);

    return IMFMediaEventQueue_GetEvent(stream->event_queue, flags, event);
}

static HRESULT WINAPI media_stream_BeginGetEvent(IMFMediaStream *iface, IMFAsyncCallback *callback, IUnknown *state)
{
    struct media_stream *stream = impl_from_IMFMediaStream(iface);

    TRACE("%p, %p, %p.\n", iface, callback, state);

    return IMFMediaEventQueue_BeginGetEvent(stream->event_queue, callback, state);
}

static HRESULT WINAPI media_stream_EndGetEvent(IMFMediaStream *iface, IMFAsyncResult *result, IMFMediaEvent **event)
{
    struct media_stream *stream = impl_from_IMFMediaStream(iface);

    TRACE("%p, %p, %p.\n", stream, result, event);

    return IMFMediaEventQueue_EndGetEvent(stream->event_queue, result, event);
}

static HRESULT WINAPI media_stream_QueueEvent(IMFMediaStream *iface, MediaEventType event_type, REFGUID ext_type,
        HRESULT hr, const PROPVARIANT *value)
{
    struct media_stream *stream = impl_from_IMFMediaStream(iface);

    TRACE("%p, %lu, %s, %#lx, %p.\n", iface, event_type, debugstr_guid(ext_type), hr, value);

    return IMFMediaEventQueue_QueueEventParamVar(stream->event_queue, event_type, ext_type, hr, value);
}

static HRESULT WINAPI media_stream_GetMediaSource(IMFMediaStream *iface, IMFMediaSource **out)
{
    struct media_stream *stream = impl_from_IMFMediaStream(iface);
    struct media_source *source = impl_from_IMFMediaSource(stream->media_source);
    HRESULT hr = S_OK;

    TRACE("%p, %p.\n", iface, out);

    EnterCriticalSection(&source->cs);

    if (source->state == SOURCE_SHUTDOWN)
        hr = MF_E_SHUTDOWN;
    else
    {
        IMFMediaSource_AddRef(&source->IMFMediaSource_iface);
        *out = &source->IMFMediaSource_iface;
    }

    LeaveCriticalSection(&source->cs);

    return hr;
}

static HRESULT WINAPI media_stream_GetStreamDescriptor(IMFMediaStream* iface, IMFStreamDescriptor **descriptor)
{
    struct media_stream *stream = impl_from_IMFMediaStream(iface);
    struct media_source *source = impl_from_IMFMediaSource(stream->media_source);
    HRESULT hr = S_OK;

    TRACE("%p, %p.\n", iface, descriptor);

    EnterCriticalSection(&source->cs);

    if (source->state == SOURCE_SHUTDOWN)
        hr = MF_E_SHUTDOWN;
    else
    {
        IMFStreamDescriptor_AddRef(stream->descriptor);
        *descriptor = stream->descriptor;
    }

    LeaveCriticalSection(&source->cs);

    return hr;
}

static HRESULT WINAPI media_stream_RequestSample(IMFMediaStream *iface, IUnknown *token)
{
    struct media_stream *stream = impl_from_IMFMediaStream(iface);
    struct media_source *source = impl_from_IMFMediaSource(stream->media_source);
    struct source_async_command *command;
    HRESULT hr;

    TRACE("%p, %p.\n", iface, token);

    EnterCriticalSection(&source->cs);

    if (source->state == SOURCE_SHUTDOWN)
        hr = MF_E_SHUTDOWN;
    else if (!stream->active)
        hr = MF_E_MEDIA_SOURCE_WRONGSTATE;
    else if (stream->eos)
        hr = MF_E_END_OF_STREAM;
    else if (SUCCEEDED(hr = source_create_async_op(SOURCE_ASYNC_REQUEST_SAMPLE, &command)))
    {
        command->u.request_sample.stream = stream;
        if (token)
            IUnknown_AddRef(token);
        command->u.request_sample.token = token;

        hr = MFPutWorkItem(source->async_commands_queue, &source->async_commands_callback, &command->IUnknown_iface);
    }

    LeaveCriticalSection(&source->cs);

    return hr;
}

static const IMFMediaStreamVtbl media_stream_vtbl =
{
    media_stream_QueryInterface,
    media_stream_AddRef,
    media_stream_Release,
    media_stream_GetEvent,
    media_stream_BeginGetEvent,
    media_stream_EndGetEvent,
    media_stream_QueueEvent,
    media_stream_GetMediaSource,
    media_stream_GetStreamDescriptor,
    media_stream_RequestSample
};

static HRESULT media_stream_create(IMFMediaSource *source, DWORD id,
        struct media_stream **out)
{
    struct wg_parser *wg_parser = impl_from_IMFMediaSource(source)->wg_parser;
    struct media_stream *object;
    HRESULT hr;

    TRACE("source %p, id %lu.\n", source, id);

    if (!(object = calloc(1, sizeof(*object))))
        return E_OUTOFMEMORY;

    object->IMFMediaStream_iface.lpVtbl = &media_stream_vtbl;
    object->ref = 1;

    if (FAILED(hr = MFCreateEventQueue(&object->event_queue)))
    {
        free(object);
        return hr;
    }

    IMFMediaSource_AddRef(source);
    object->media_source = source;
    object->stream_id = id;

    object->active = FALSE;
    object->eos = FALSE;
    object->wg_stream = wg_parser_get_stream(wg_parser, id);

    TRACE("Created stream object %p.\n", object);

    *out = object;
    return S_OK;
}

static HRESULT media_stream_init_desc(struct media_stream *stream)
{
    IMFMediaTypeHandler *type_handler = NULL;
    IMFMediaType *stream_types[8];
    struct wg_format format;
    DWORD type_count = 0;
    HRESULT hr = S_OK;
    unsigned int i;

    wg_parser_stream_get_preferred_format(stream->wg_stream, &format);

    if (format.major_type == WG_MAJOR_TYPE_VIDEO)
    {
        /* These are the most common native output types of decoders:
            https://docs.microsoft.com/en-us/windows/win32/medfound/mft-decoder-expose-output-types-in-native-order */
        static const GUID *const video_types[] =
        {
            &MFVideoFormat_NV12,
            &MFVideoFormat_YV12,
            &MFVideoFormat_YUY2,
            &MFVideoFormat_IYUV,
            &MFVideoFormat_I420,
            &MFVideoFormat_ARGB32,
            &MFVideoFormat_RGB32,
        };

        IMFMediaType *base_type = mf_media_type_from_wg_format(&format);
        GUID base_subtype;

        if (!base_type)
        {
            hr = MF_E_INVALIDMEDIATYPE;
            goto done;
        }

        IMFMediaType_GetGUID(base_type, &MF_MT_SUBTYPE, &base_subtype);

        stream_types[0] = base_type;
        type_count = 1;

        for (i = 0; i < ARRAY_SIZE(video_types); i++)
        {
            IMFMediaType *new_type;

            if (IsEqualGUID(&base_subtype, video_types[i]))
                continue;

            if (FAILED(hr = MFCreateMediaType(&new_type)))
                goto done;
            stream_types[type_count++] = new_type;

            if (FAILED(hr = IMFMediaType_CopyAllItems(base_type, (IMFAttributes *) new_type)))
                goto done;
            if (FAILED(hr = IMFMediaType_SetGUID(new_type, &MF_MT_SUBTYPE, video_types[i])))
                goto done;
        }
    }
    else if (format.major_type == WG_MAJOR_TYPE_AUDIO)
    {
        /* Expose at least one PCM and one floating point type for the
           consumer to pick from. */
        static const enum wg_audio_format audio_types[] =
        {
            WG_AUDIO_FORMAT_S16LE,
            WG_AUDIO_FORMAT_F32LE,
        };

        if ((stream_types[0] = mf_media_type_from_wg_format(&format)))
            type_count = 1;

        for (i = 0; i < ARRAY_SIZE(audio_types); i++)
        {
            struct wg_format new_format;
            if (format.u.audio.format == audio_types[i])
                continue;
            new_format = format;
            new_format.u.audio.format = audio_types[i];
            if ((stream_types[type_count] = mf_media_type_from_wg_format(&new_format)))
                type_count++;
        }
    }
    else
    {
        if ((stream_types[0] = mf_media_type_from_wg_format(&format)))
            type_count = 1;
    }

    assert(type_count <= ARRAY_SIZE(stream_types));

    if (!type_count)
    {
        ERR("Failed to establish an IMFMediaType from any of the possible stream caps!\n");
        return E_FAIL;
    }

    if (FAILED(hr = MFCreateStreamDescriptor(stream->stream_id, type_count, stream_types, &stream->descriptor)))
        goto done;

    if (FAILED(hr = IMFStreamDescriptor_GetMediaTypeHandler(stream->descriptor, &type_handler)))
    {
        IMFStreamDescriptor_Release(stream->descriptor);
        goto done;
    }

    if (FAILED(hr = IMFMediaTypeHandler_SetCurrentMediaType(type_handler, stream_types[0])))
    {
        IMFStreamDescriptor_Release(stream->descriptor);
        goto done;
    }

done:
    if (type_handler)
        IMFMediaTypeHandler_Release(type_handler);
    for (i = 0; i < type_count; i++)
        IMFMediaType_Release(stream_types[i]);
    return hr;
}

static HRESULT WINAPI media_source_get_service_QueryInterface(IMFGetService *iface, REFIID riid, void **obj)
{
    struct media_source *source = impl_from_IMFGetService(iface);
    return IMFMediaSource_QueryInterface(&source->IMFMediaSource_iface, riid, obj);
}

static ULONG WINAPI media_source_get_service_AddRef(IMFGetService *iface)
{
    struct media_source *source = impl_from_IMFGetService(iface);
    return IMFMediaSource_AddRef(&source->IMFMediaSource_iface);
}

static ULONG WINAPI media_source_get_service_Release(IMFGetService *iface)
{
    struct media_source *source = impl_from_IMFGetService(iface);
    return IMFMediaSource_Release(&source->IMFMediaSource_iface);
}

static HRESULT WINAPI media_source_get_service_GetService(IMFGetService *iface, REFGUID service, REFIID riid, void **obj)
{
    struct media_source *source = impl_from_IMFGetService(iface);

    TRACE("%p, %s, %s, %p.\n", iface, debugstr_guid(service), debugstr_guid(riid), obj);

    *obj = NULL;

    if (IsEqualGUID(service, &MF_RATE_CONTROL_SERVICE))
    {
        if (IsEqualIID(riid, &IID_IMFRateSupport))
        {
            *obj = &source->IMFRateSupport_iface;
        }
        else if (IsEqualIID(riid, &IID_IMFRateControl))
        {
            *obj = &source->IMFRateControl_iface;
        }
    }
    else
        FIXME("Unsupported service %s.\n", debugstr_guid(service));

    if (*obj)
        IUnknown_AddRef((IUnknown *)*obj);

    return *obj ? S_OK : E_NOINTERFACE;
}

static const IMFGetServiceVtbl media_source_get_service_vtbl =
{
    media_source_get_service_QueryInterface,
    media_source_get_service_AddRef,
    media_source_get_service_Release,
    media_source_get_service_GetService,
};

static HRESULT WINAPI media_source_rate_support_QueryInterface(IMFRateSupport *iface, REFIID riid, void **obj)
{
    struct media_source *source = impl_from_IMFRateSupport(iface);
    return IMFMediaSource_QueryInterface(&source->IMFMediaSource_iface, riid, obj);
}

static ULONG WINAPI media_source_rate_support_AddRef(IMFRateSupport *iface)
{
    struct media_source *source = impl_from_IMFRateSupport(iface);
    return IMFMediaSource_AddRef(&source->IMFMediaSource_iface);
}

static ULONG WINAPI media_source_rate_support_Release(IMFRateSupport *iface)
{
    struct media_source *source = impl_from_IMFRateSupport(iface);
    return IMFMediaSource_Release(&source->IMFMediaSource_iface);
}

static HRESULT WINAPI media_source_rate_support_GetSlowestRate(IMFRateSupport *iface, MFRATE_DIRECTION direction, BOOL thin, float *rate)
{
    TRACE("%p, %d, %d, %p.\n", iface, direction, thin, rate);

    *rate = 0.0f;

    return S_OK;
}

static HRESULT WINAPI media_source_rate_support_GetFastestRate(IMFRateSupport *iface, MFRATE_DIRECTION direction, BOOL thin, float *rate)
{
    TRACE("%p, %d, %d, %p.\n", iface, direction, thin, rate);

    *rate = direction == MFRATE_FORWARD ? 1e6f : -1e6f;

    return S_OK;
}

static HRESULT WINAPI media_source_rate_support_IsRateSupported(IMFRateSupport *iface, BOOL thin, float rate,
        float *nearest_rate)
{
    TRACE("%p, %d, %f, %p.\n", iface, thin, rate, nearest_rate);

    if (nearest_rate)
        *nearest_rate = rate;

    return rate >= -1e6f && rate <= 1e6f ? S_OK : MF_E_UNSUPPORTED_RATE;
}

static const IMFRateSupportVtbl media_source_rate_support_vtbl =
{
    media_source_rate_support_QueryInterface,
    media_source_rate_support_AddRef,
    media_source_rate_support_Release,
    media_source_rate_support_GetSlowestRate,
    media_source_rate_support_GetFastestRate,
    media_source_rate_support_IsRateSupported,
};

static HRESULT WINAPI media_source_rate_control_QueryInterface(IMFRateControl *iface, REFIID riid, void **obj)
{
    struct media_source *source = impl_from_IMFRateControl(iface);
    return IMFMediaSource_QueryInterface(&source->IMFMediaSource_iface, riid, obj);
}

static ULONG WINAPI media_source_rate_control_AddRef(IMFRateControl *iface)
{
    struct media_source *source = impl_from_IMFRateControl(iface);
    return IMFMediaSource_AddRef(&source->IMFMediaSource_iface);
}

static ULONG WINAPI media_source_rate_control_Release(IMFRateControl *iface)
{
    struct media_source *source = impl_from_IMFRateControl(iface);
    return IMFMediaSource_Release(&source->IMFMediaSource_iface);
}

static HRESULT WINAPI media_source_rate_control_SetRate(IMFRateControl *iface, BOOL thin, float rate)
{
    struct media_source *source = impl_from_IMFRateControl(iface);
    HRESULT hr;

    FIXME("%p, %d, %f.\n", iface, thin, rate);

    if (rate < 0.0f)
        return MF_E_REVERSE_UNSUPPORTED;

    if (thin)
        return MF_E_THINNING_UNSUPPORTED;

    if (FAILED(hr = IMFRateSupport_IsRateSupported(&source->IMFRateSupport_iface, thin, rate, NULL)))
        return hr;

    EnterCriticalSection(&source->cs);
    source->rate = rate;
    LeaveCriticalSection(&source->cs);

    return IMFMediaEventQueue_QueueEventParamVar(source->event_queue, MESourceRateChanged, &GUID_NULL, S_OK, NULL);
}

static HRESULT WINAPI media_source_rate_control_GetRate(IMFRateControl *iface, BOOL *thin, float *rate)
{
    struct media_source *source = impl_from_IMFRateControl(iface);

    TRACE("%p, %p, %p.\n", iface, thin, rate);

    if (thin)
        *thin = FALSE;

    EnterCriticalSection(&source->cs);
    *rate = source->rate;
    LeaveCriticalSection(&source->cs);

    return S_OK;
}

static const IMFRateControlVtbl media_source_rate_control_vtbl =
{
    media_source_rate_control_QueryInterface,
    media_source_rate_control_AddRef,
    media_source_rate_control_Release,
    media_source_rate_control_SetRate,
    media_source_rate_control_GetRate,
};

static HRESULT WINAPI media_source_QueryInterface(IMFMediaSource *iface, REFIID riid, void **out)
{
    struct media_source *source = impl_from_IMFMediaSource(iface);

    TRACE("%p, %s, %p.\n", iface, debugstr_guid(riid), out);

    if (IsEqualIID(riid, &IID_IMFMediaSource) ||
        IsEqualIID(riid, &IID_IMFMediaEventGenerator) ||
        IsEqualIID(riid, &IID_IUnknown))
    {
        *out = &source->IMFMediaSource_iface;
    }
    else if (IsEqualIID(riid, &IID_IMFGetService))
    {
        *out = &source->IMFGetService_iface;
    }
    else
    {
        FIXME("%s, %p.\n", debugstr_guid(riid), out);
        *out = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*out);
    return S_OK;
}

static ULONG WINAPI media_source_AddRef(IMFMediaSource *iface)
{
    struct media_source *source = impl_from_IMFMediaSource(iface);
    ULONG ref = InterlockedIncrement(&source->ref);

    TRACE("%p, refcount %lu.\n", iface, ref);

    return ref;
}

static ULONG WINAPI media_source_Release(IMFMediaSource *iface)
{
    struct media_source *source = impl_from_IMFMediaSource(iface);
    ULONG ref = InterlockedDecrement(&source->ref);

    TRACE("%p, refcount %lu.\n", iface, ref);

    if (!ref)
    {
        IMFMediaSource_Shutdown(iface);
        IMFPresentationDescriptor_Release(source->pres_desc);
        IMFMediaEventQueue_Release(source->event_queue);
        IMFByteStream_Release(source->byte_stream);
        wg_parser_destroy(source->wg_parser);
        source->cs.DebugInfo->Spare[0] = 0;
        DeleteCriticalSection(&source->cs);
        free(source);
    }

    return ref;
}

static HRESULT WINAPI media_source_GetEvent(IMFMediaSource *iface, DWORD flags, IMFMediaEvent **event)
{
    struct media_source *source = impl_from_IMFMediaSource(iface);

    TRACE("%p, %#lx, %p.\n", iface, flags, event);

    return IMFMediaEventQueue_GetEvent(source->event_queue, flags, event);
}

static HRESULT WINAPI media_source_BeginGetEvent(IMFMediaSource *iface, IMFAsyncCallback *callback, IUnknown *state)
{
    struct media_source *source = impl_from_IMFMediaSource(iface);

    TRACE("%p, %p, %p.\n", iface, callback, state);

    return IMFMediaEventQueue_BeginGetEvent(source->event_queue, callback, state);
}

static HRESULT WINAPI media_source_EndGetEvent(IMFMediaSource *iface, IMFAsyncResult *result, IMFMediaEvent **event)
{
    struct media_source *source = impl_from_IMFMediaSource(iface);

    TRACE("%p, %p, %p.\n", iface, result, event);

    return IMFMediaEventQueue_EndGetEvent(source->event_queue, result, event);
}

static HRESULT WINAPI media_source_QueueEvent(IMFMediaSource *iface, MediaEventType event_type, REFGUID ext_type,
        HRESULT hr, const PROPVARIANT *value)
{
    struct media_source *source = impl_from_IMFMediaSource(iface);

    TRACE("%p, %lu, %s, %#lx, %p.\n", iface, event_type, debugstr_guid(ext_type), hr, value);

    return IMFMediaEventQueue_QueueEventParamVar(source->event_queue, event_type, ext_type, hr, value);
}

static HRESULT WINAPI media_source_GetCharacteristics(IMFMediaSource *iface, DWORD *characteristics)
{
    struct media_source *source = impl_from_IMFMediaSource(iface);
    HRESULT hr = S_OK;

    TRACE("%p, %p.\n", iface, characteristics);

    EnterCriticalSection(&source->cs);

    if (source->state == SOURCE_SHUTDOWN)
        hr = MF_E_SHUTDOWN;
    else
        *characteristics = MFMEDIASOURCE_CAN_SEEK | MFMEDIASOURCE_CAN_PAUSE;

    LeaveCriticalSection(&source->cs);

    return hr;
}

static HRESULT WINAPI media_source_CreatePresentationDescriptor(IMFMediaSource *iface, IMFPresentationDescriptor **descriptor)
{
    struct media_source *source = impl_from_IMFMediaSource(iface);
    HRESULT hr;

    TRACE("%p, %p.\n", iface, descriptor);

    EnterCriticalSection(&source->cs);

    if (source->state == SOURCE_SHUTDOWN)
        hr = MF_E_SHUTDOWN;
    else
        hr = IMFPresentationDescriptor_Clone(source->pres_desc, descriptor);

    LeaveCriticalSection(&source->cs);

    return hr;
}

static HRESULT WINAPI media_source_Start(IMFMediaSource *iface, IMFPresentationDescriptor *descriptor,
                                     const GUID *time_format, const PROPVARIANT *position)
{
    struct media_source *source = impl_from_IMFMediaSource(iface);
    struct source_async_command *command;
    HRESULT hr;

    TRACE("%p, %p, %p, %p.\n", iface, descriptor, time_format, position);

    EnterCriticalSection(&source->cs);

    if (source->state == SOURCE_SHUTDOWN)
        hr = MF_E_SHUTDOWN;
    else if (!(IsEqualIID(time_format, &GUID_NULL)))
        hr = MF_E_UNSUPPORTED_TIME_FORMAT;
    else if (SUCCEEDED(hr = source_create_async_op(SOURCE_ASYNC_START, &command)))
    {
        command->u.start.descriptor = descriptor;
        command->u.start.format = *time_format;
        PropVariantCopy(&command->u.start.position, position);

        hr = MFPutWorkItem(source->async_commands_queue, &source->async_commands_callback, &command->IUnknown_iface);
    }

    LeaveCriticalSection(&source->cs);

    return hr;
}

static HRESULT WINAPI media_source_Stop(IMFMediaSource *iface)
{
    struct media_source *source = impl_from_IMFMediaSource(iface);
    struct source_async_command *command;
    HRESULT hr;

    TRACE("%p.\n", iface);

    EnterCriticalSection(&source->cs);

    if (source->state == SOURCE_SHUTDOWN)
        hr = MF_E_SHUTDOWN;
    else if (SUCCEEDED(hr = source_create_async_op(SOURCE_ASYNC_STOP, &command)))
        hr = MFPutWorkItem(source->async_commands_queue, &source->async_commands_callback, &command->IUnknown_iface);

    LeaveCriticalSection(&source->cs);

    return hr;
}

static HRESULT WINAPI media_source_Pause(IMFMediaSource *iface)
{
    struct media_source *source = impl_from_IMFMediaSource(iface);
    struct source_async_command *command;
    HRESULT hr;

    TRACE("%p.\n", iface);

    EnterCriticalSection(&source->cs);

    if (source->state == SOURCE_SHUTDOWN)
        hr = MF_E_SHUTDOWN;
    else if (source->state != SOURCE_RUNNING)
        hr = MF_E_INVALID_STATE_TRANSITION;
    else if (SUCCEEDED(hr = source_create_async_op(SOURCE_ASYNC_PAUSE, &command)))
        hr = MFPutWorkItem(source->async_commands_queue,
                &source->async_commands_callback, &command->IUnknown_iface);

    LeaveCriticalSection(&source->cs);

    return S_OK;
}

static HRESULT WINAPI media_source_Shutdown(IMFMediaSource *iface)
{
    struct media_source *source = impl_from_IMFMediaSource(iface);

    TRACE("%p.\n", iface);

    EnterCriticalSection(&source->cs);

    if (source->state == SOURCE_SHUTDOWN)
    {
        LeaveCriticalSection(&source->cs);
        return MF_E_SHUTDOWN;
    }

    source->state = SOURCE_SHUTDOWN;

    wg_parser_disconnect(source->wg_parser);

    source->read_thread_shutdown = true;
    WaitForSingleObject(source->read_thread, INFINITE);
    CloseHandle(source->read_thread);

    IMFMediaEventQueue_Shutdown(source->event_queue);
    IMFByteStream_Close(source->byte_stream);

    while (source->stream_count--)
    {
        struct media_stream *stream = source->streams[source->stream_count];
        IMFMediaEventQueue_Shutdown(stream->event_queue);
        IMFMediaStream_Release(&stream->IMFMediaStream_iface);
    }
    free(source->streams);

    MFUnlockWorkQueue(source->async_commands_queue);

    LeaveCriticalSection(&source->cs);

    return S_OK;
}

static const IMFMediaSourceVtbl IMFMediaSource_vtbl =
{
    media_source_QueryInterface,
    media_source_AddRef,
    media_source_Release,
    media_source_GetEvent,
    media_source_BeginGetEvent,
    media_source_EndGetEvent,
    media_source_QueueEvent,
    media_source_GetCharacteristics,
    media_source_CreatePresentationDescriptor,
    media_source_Start,
    media_source_Stop,
    media_source_Pause,
    media_source_Shutdown,
};

HRESULT media_source_create(IMFByteStream *bytestream, const WCHAR *url, BYTE *data, UINT64 size, IMFMediaSource **out)
{
    BOOL video_selected = FALSE, audio_selected = FALSE;
    IMFStreamDescriptor **descriptors = NULL;
    unsigned int stream_count = UINT_MAX;
    struct media_source *object;
    UINT64 total_pres_time = 0;
    struct wg_parser *parser;
    DWORD bytestream_caps;
    uint64_t file_size;
    unsigned int i;
    HRESULT hr;

    if (FAILED(hr = IMFByteStream_GetCapabilities(bytestream, &bytestream_caps)))
        return hr;

    if (!(bytestream_caps & MFBYTESTREAM_IS_SEEKABLE))
    {
        FIXME("Non-seekable bytestreams not supported.\n");
        return MF_E_BYTESTREAM_NOT_SEEKABLE;
    }

    if (FAILED(hr = IMFByteStream_GetLength(bytestream, &file_size)))
    {
        FIXME("Failed to get byte stream length, hr %#lx.\n", hr);
        return hr;
    }

    if (!(object = calloc(1, sizeof(*object))))
        return E_OUTOFMEMORY;

    object->IMFMediaSource_iface.lpVtbl = &IMFMediaSource_vtbl;
    object->IMFGetService_iface.lpVtbl = &media_source_get_service_vtbl;
    object->IMFRateSupport_iface.lpVtbl = &media_source_rate_support_vtbl;
    object->IMFRateControl_iface.lpVtbl = &media_source_rate_control_vtbl;
    object->async_commands_callback.lpVtbl = &source_async_commands_callback_vtbl;
    object->ref = 1;
    object->byte_stream = bytestream;
    IMFByteStream_AddRef(bytestream);
    object->rate = 1.0f;
    InitializeCriticalSection(&object->cs);
    object->cs.DebugInfo->Spare[0] = (DWORD_PTR)(__FILE__ ": cs");

    if (FAILED(hr = MFCreateEventQueue(&object->event_queue)))
        goto fail;

    if (FAILED(hr = MFAllocateWorkQueue(&object->async_commands_queue)))
        goto fail;

    if (!(parser = wg_parser_create(WG_PARSER_DECODEBIN, false)))
    {
        hr = E_OUTOFMEMORY;
        goto fail;
    }
    object->wg_parser = parser;

    object->read_thread = CreateThread(NULL, 0, read_thread, object, 0, NULL);

    object->state = SOURCE_OPENING;

    if (FAILED(hr = wg_parser_connect(parser, file_size, NULL)))
        goto fail;

    stream_count = wg_parser_get_stream_count(parser);

    if (!(object->streams = calloc(stream_count, sizeof(*object->streams))))
    {
        hr = E_OUTOFMEMORY;
        goto fail;
    }

    for (i = 0; i < stream_count; ++i)
    {
        if (FAILED(hr = media_stream_create(&object->IMFMediaSource_iface, i, &object->streams[i])))
            goto fail;

        if (FAILED(hr = media_stream_init_desc(object->streams[i])))
        {
            ERR("Failed to finish initialization of media stream %p, hr %#lx.\n", object->streams[i], hr);
            IMFMediaSource_Release(object->streams[i]->media_source);
            IMFMediaEventQueue_Release(object->streams[i]->event_queue);
            free(object->streams[i]);
            goto fail;
        }

        object->stream_count++;
    }

    /* init presentation descriptor */

    descriptors = malloc(object->stream_count * sizeof(IMFStreamDescriptor *));
    for (i = 0; i < object->stream_count; i++)
    {
        static const struct
        {
            enum wg_parser_tag tag;
            const GUID *mf_attr;
        }
        tags[] =
        {
            {WG_PARSER_TAG_LANGUAGE, &MF_SD_LANGUAGE},
            {WG_PARSER_TAG_NAME, &MF_SD_STREAM_NAME},
        };
        IMFStreamDescriptor **descriptor = descriptors + object->stream_count - 1 - i;
        unsigned int j;
        WCHAR *strW;
        DWORD len;
        char *str;

        IMFMediaStream_GetStreamDescriptor(&object->streams[i]->IMFMediaStream_iface, descriptor);

        for (j = 0; j < ARRAY_SIZE(tags); ++j)
        {
            if (!(str = wg_parser_stream_get_tag(object->streams[i]->wg_stream, tags[j].tag)))
                continue;
            if (!(len = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0)))
            {
                free(str);
                continue;
            }
            strW = malloc(len * sizeof(*strW));
            if (MultiByteToWideChar(CP_UTF8, 0, str, -1, strW, len))
                IMFStreamDescriptor_SetString(*descriptor, tags[j].mf_attr, strW);
            free(strW);
            free(str);
        }
    }

    if (FAILED(hr = MFCreatePresentationDescriptor(object->stream_count, descriptors, &object->pres_desc)))
        goto fail;

    /* Select one of each major type. */
    for (i = 0; i < object->stream_count; i++)
    {
        IMFMediaTypeHandler *handler;
        GUID major_type;
        BOOL select_stream = FALSE;

        IMFStreamDescriptor_GetMediaTypeHandler(descriptors[i], &handler);
        IMFMediaTypeHandler_GetMajorType(handler, &major_type);
        if (IsEqualGUID(&major_type, &MFMediaType_Video) && !video_selected)
        {
            select_stream = TRUE;
            video_selected = TRUE;
        }
        if (IsEqualGUID(&major_type, &MFMediaType_Audio) && !audio_selected)
        {
            select_stream = TRUE;
            audio_selected = TRUE;
        }
        if (select_stream)
            IMFPresentationDescriptor_SelectStream(object->pres_desc, i);
        IMFMediaTypeHandler_Release(handler);
        IMFStreamDescriptor_Release(descriptors[i]);
    }
    free(descriptors);
    descriptors = NULL;

    for (i = 0; i < object->stream_count; i++)
        total_pres_time = max(total_pres_time,
                wg_parser_stream_get_duration(object->streams[i]->wg_stream));

    if (object->stream_count)
        IMFPresentationDescriptor_SetUINT64(object->pres_desc, &MF_PD_DURATION, total_pres_time);

    object->state = SOURCE_STOPPED;

    *out = &object->IMFMediaSource_iface;
    return S_OK;

    fail:
    WARN("Failed to construct MFMediaSource, hr %#lx.\n", hr);

    if (descriptors)
    {
        for (i = 0; i < object->stream_count; i++)
            IMFStreamDescriptor_Release(descriptors[i]);
        free(descriptors);
    }

    while (object->streams && object->stream_count--)
    {
        struct media_stream *stream = object->streams[object->stream_count];
        IMFMediaStream_Release(&stream->IMFMediaStream_iface);
    }
    free(object->streams);

    if (stream_count != UINT_MAX)
        wg_parser_disconnect(object->wg_parser);
    if (object->read_thread)
    {
        object->read_thread_shutdown = true;
        WaitForSingleObject(object->read_thread, INFINITE);
        CloseHandle(object->read_thread);
    }
    if (object->wg_parser)
        wg_parser_destroy(object->wg_parser);
    if (object->async_commands_queue)
        MFUnlockWorkQueue(object->async_commands_queue);
    if (object->event_queue)
        IMFMediaEventQueue_Release(object->event_queue);
    IMFByteStream_Release(object->byte_stream);
    free(object);
    return hr;
}
