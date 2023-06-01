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

#define SOURCE_BUFFER_SIZE (32 * 1024)

struct object_context
{
    IUnknown IUnknown_iface;
    LONG refcount;

    IMFAsyncResult *result;
    IMFByteStream *stream;
    UINT64 file_size;
    WCHAR *url;

    BYTE *buffer;
    wg_source_t wg_source;
    WCHAR mime_type[256];
    UINT32 stream_count;
};

static struct object_context *impl_from_IUnknown(IUnknown *iface)
{
    return CONTAINING_RECORD(iface, struct object_context, IUnknown_iface);
}

static HRESULT WINAPI object_context_QueryInterface(IUnknown *iface, REFIID riid, void **obj)
{
    TRACE("%p, %s, %p.\n", iface, debugstr_guid(riid), obj);

    if (IsEqualIID(riid, &IID_IUnknown))
    {
        *obj = iface;
        IUnknown_AddRef(iface);
        return S_OK;
    }

    WARN("Unsupported %s.\n", debugstr_guid(riid));
    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI object_context_AddRef(IUnknown *iface)
{
    struct object_context *context = impl_from_IUnknown(iface);
    ULONG refcount = InterlockedIncrement(&context->refcount);

    TRACE("%p, refcount %lu.\n", iface, refcount);

    return refcount;
}

static ULONG WINAPI object_context_Release(IUnknown *iface)
{
    struct object_context *context = impl_from_IUnknown(iface);
    ULONG refcount = InterlockedDecrement(&context->refcount);

    TRACE("%p, refcount %lu.\n", iface, refcount);

    if (!refcount)
    {
        if (context->wg_source)
            wg_source_destroy(context->wg_source);
        IMFAsyncResult_Release(context->result);
        IMFByteStream_Release(context->stream);
        free(context->buffer);
        free(context->url);
        free(context);
    }

    return refcount;
}

static const IUnknownVtbl object_context_vtbl =
{
    object_context_QueryInterface,
    object_context_AddRef,
    object_context_Release,
};

static WCHAR *byte_stream_get_url(IMFByteStream *stream, const WCHAR *url)
{
    IMFAttributes *attributes;
    WCHAR buffer[MAX_PATH];
    UINT32 size;
    HRESULT hr;

    if (SUCCEEDED(hr = IMFByteStream_QueryInterface(stream, &IID_IMFAttributes, (void **)&attributes)))
    {
        if (FAILED(hr = IMFAttributes_GetString(attributes, &MF_BYTESTREAM_ORIGIN_NAME,
                buffer, ARRAY_SIZE(buffer), &size)))
            WARN("Failed to get MF_BYTESTREAM_ORIGIN_NAME got size %#x, hr %#lx\n", size, hr);
        else
            url = buffer;
        IMFAttributes_Release(attributes);
    }

    return url ? wcsdup(url) : NULL;
}

static HRESULT object_context_create(DWORD flags, IMFByteStream *stream, const WCHAR *url,
        QWORD file_size, IMFAsyncResult *result, IUnknown **out, BYTE **out_buf)
{
    WCHAR *tmp_url = byte_stream_get_url(stream, url);
    struct object_context *context;

    if (!(context = calloc(1, sizeof(*context)))
            || !(context->buffer = malloc(SOURCE_BUFFER_SIZE)))
    {
        free(tmp_url);
        free(context);
        return E_OUTOFMEMORY;
    }

    context->IUnknown_iface.lpVtbl = &object_context_vtbl;
    context->refcount = 1;
    context->stream = stream;
    IMFByteStream_AddRef(context->stream);
    context->file_size = file_size;
    context->url = tmp_url;
    context->result = result;
    IMFAsyncResult_AddRef(context->result);

    *out = &context->IUnknown_iface;
    *out_buf = context->buffer;
    return S_OK;
}

struct media_source_fallback_callback
{
    IMFAsyncCallback IMFAsyncCallback_iface;
    IMFAsyncResult *result;
    HANDLE event;
};

static HRESULT WINAPI media_source_fallback_callback_QueryInterface(IMFAsyncCallback *iface, REFIID riid, void **obj)
{
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

static ULONG WINAPI media_source_fallback_callback_AddRef(IMFAsyncCallback *iface)
{
    return 2;
}

static ULONG WINAPI media_source_fallback_callback_Release(IMFAsyncCallback *iface)
{
    return 1;
}

static HRESULT WINAPI media_source_fallback_callback_GetParameters(IMFAsyncCallback *iface, DWORD *flags, DWORD *queue)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI media_source_fallback_callback_Invoke(IMFAsyncCallback *iface, IMFAsyncResult *result)
{
    struct media_source_fallback_callback *impl = CONTAINING_RECORD(iface, struct media_source_fallback_callback, IMFAsyncCallback_iface);

    IMFAsyncResult_AddRef((impl->result = result));
    SetEvent(impl->event);

    return S_OK;
}

static const IMFAsyncCallbackVtbl media_source_fallback_callback_vtbl =
{
    media_source_fallback_callback_QueryInterface,
    media_source_fallback_callback_AddRef,
    media_source_fallback_callback_Release,
    media_source_fallback_callback_GetParameters,
    media_source_fallback_callback_Invoke,
};

static HRESULT create_media_source_fallback(struct object_context *context, IUnknown **object)
{
    static const GUID CLSID_GStreamerByteStreamHandler = {0x317df618, 0x5e5a, 0x468a, {0x9f, 0x15, 0xd8, 0x27, 0xa9, 0xa0, 0x81, 0x62}};
    struct media_source_fallback_callback callback = {{&media_source_fallback_callback_vtbl}};
    IMFByteStreamHandler *handler;
    MF_OBJECT_TYPE type;
    HRESULT hr;

    if (!(callback.event = CreateEventW(NULL, FALSE, FALSE, NULL)))
        return HRESULT_FROM_WIN32(GetLastError());

    if (FAILED(hr = CoCreateInstance(&CLSID_GStreamerByteStreamHandler, NULL, CLSCTX_INPROC_SERVER,
            &IID_IMFByteStreamHandler, (void **)&handler)))
    {
        CloseHandle(callback.event);
        return hr;
    }

    if (SUCCEEDED(hr = IMFByteStreamHandler_BeginCreateObject(handler, context->stream, NULL, MF_RESOLUTION_MEDIASOURCE,
            NULL, NULL, &callback.IMFAsyncCallback_iface, NULL)))
    {
        WaitForSingleObject(callback.event, INFINITE);
        hr = IMFByteStreamHandler_EndCreateObject(handler, callback.result, &type, object);
        IMFAsyncResult_Release(callback.result);
    }

    IMFByteStreamHandler_Release(handler);
    CloseHandle(callback.event);
    return hr;
}

struct media_stream
{
    IMFMediaStream IMFMediaStream_iface;
    LONG ref;

    IMFMediaSource *media_source;
    IMFMediaEventQueue *event_queue;
    IMFStreamDescriptor *descriptor;

    IUnknown **token_queue;
    LONG token_queue_count;
    LONG token_queue_cap;

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

    wg_source_t wg_source;
    WCHAR mime_type[256];
    UINT64 file_size;
    UINT64 duration;
    BYTE *read_buffer;

    IMFStreamDescriptor **descriptors;
    struct media_stream **streams;
    ULONG stream_count;
    UINT *stream_map;

    enum
    {
        SOURCE_OPENING,
        SOURCE_STOPPED,
        SOURCE_PAUSED,
        SOURCE_RUNNING,
        SOURCE_SHUTDOWN,
    } state;
    float rate;
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
        {
            IMFPresentationDescriptor_Release(command->u.start.descriptor);
            PropVariantClear(&command->u.start.position);
        }
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

static HRESULT source_create_async_op(enum source_async_op op, IUnknown **out)
{
    struct source_async_command *command;

    if (!(command = calloc(1, sizeof(*command))))
        return E_OUTOFMEMORY;

    command->IUnknown_iface.lpVtbl = &source_async_command_vtbl;
    command->refcount = 1;
    command->op = op;

    *out = &command->IUnknown_iface;
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

static HRESULT stream_descriptor_get_media_type(IMFStreamDescriptor *descriptor, IMFMediaType **media_type)
{
    IMFMediaTypeHandler *handler;
    HRESULT hr;

    if (FAILED(hr = IMFStreamDescriptor_GetMediaTypeHandler(descriptor, &handler)))
        return hr;
    hr = IMFMediaTypeHandler_GetCurrentMediaType(handler, media_type);
    IMFMediaTypeHandler_Release(handler);

    return hr;
}

static HRESULT stream_descriptor_get_major_type(IMFStreamDescriptor *descriptor, GUID *major)
{
    IMFMediaTypeHandler *handler;
    HRESULT hr;

    if (FAILED(hr = IMFStreamDescriptor_GetMediaTypeHandler(descriptor, &handler)))
        return hr;
    hr = IMFMediaTypeHandler_GetMajorType(handler, major);
    IMFMediaTypeHandler_Release(handler);

    return hr;
}

static HRESULT wg_format_from_stream_descriptor(IMFStreamDescriptor *descriptor, struct wg_format *format)
{
    IMFMediaType *media_type;
    HRESULT hr;

    if (FAILED(hr = stream_descriptor_get_media_type(descriptor, &media_type)))
        return hr;
    mf_media_type_to_wg_format(media_type, format);
    IMFMediaType_Release(media_type);

    return hr;
}

static HRESULT stream_descriptor_set_tag(IMFStreamDescriptor *descriptor,
    wg_source_t source, UINT index, const GUID *attr, enum wg_parser_tag tag)
{
    WCHAR *strW;
    HRESULT hr;
    DWORD len;
    char *str;

    if (!(str = wg_source_get_stream_tag(source, index, tag))
            || !(len = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0)))
        hr = S_OK;
    else if (!(strW = malloc(len * sizeof(*strW))))
        hr = E_OUTOFMEMORY;
    else
    {
        if (MultiByteToWideChar(CP_UTF8, 0, str, -1, strW, len))
            hr = IMFStreamDescriptor_SetString(descriptor, attr, strW);
        else
            hr = E_FAIL;
        free(strW);
    }

    free(str);
    return hr;
}

static HRESULT stream_descriptor_create(UINT32 id, struct wg_format *format, IMFStreamDescriptor **out)
{
    IMFStreamDescriptor *descriptor;
    IMFMediaTypeHandler *handler;
    IMFMediaType *type;
    HRESULT hr;

    if (!(type = mf_media_type_from_wg_format(format)))
        return MF_E_INVALIDMEDIATYPE;
    if (FAILED(hr = MFCreateStreamDescriptor(id, 1, &type, &descriptor)))
        goto done;

    if (FAILED(hr = IMFStreamDescriptor_GetMediaTypeHandler(descriptor, &handler)))
        IMFStreamDescriptor_Release(descriptor);
    else
    {
        hr = IMFMediaTypeHandler_SetCurrentMediaType(handler, type);
        IMFMediaTypeHandler_Release(handler);
    }

done:
    IMFMediaType_Release(type);
    *out = SUCCEEDED(hr) ? descriptor : NULL;
    return hr;
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
            IUnknown *op;
            HRESULT hr;

            if (SUCCEEDED(hr = source_create_async_op(SOURCE_ASYNC_REQUEST_SAMPLE, &op)))
            {
                struct source_async_command *command = impl_from_async_command_IUnknown(op);
                command->u.request_sample.stream = stream;
                command->u.request_sample.token = stream->token_queue[i];

                hr = MFPutWorkItem(source->async_commands_queue, &source->async_commands_callback, op);
                IUnknown_Release(op);
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

static HRESULT media_stream_start(struct media_stream *stream, BOOL active, BOOL seeking, const PROPVARIANT *position)
{
    struct media_source *source = impl_from_IMFMediaSource(stream->media_source);
    struct wg_format format;
    HRESULT hr;

    TRACE("source %p, stream %p\n", source, stream);

    if (FAILED(hr = wg_format_from_stream_descriptor(stream->descriptor, &format)))
        WARN("Failed to get wg_format from stream descriptor, hr %#lx\n", hr);

    if (FAILED(hr = IMFMediaEventQueue_QueueEventParamUnk(source->event_queue, active ? MEUpdatedStream : MENewStream,
            &GUID_NULL, S_OK, (IUnknown *)&stream->IMFMediaStream_iface)))
        WARN("Failed to send source stream event, hr %#lx\n", hr);
    return IMFMediaEventQueue_QueueEventParamVar(stream->event_queue, seeking ? MEStreamSeeked : MEStreamStarted,
            &GUID_NULL, S_OK, position);
}

static HRESULT media_source_start(struct media_source *source, IMFPresentationDescriptor *descriptor,
        GUID *format, PROPVARIANT *position)
{
    BOOL starting = source->state == SOURCE_STOPPED, seek_message = !starting && position->vt != VT_EMPTY;
    IMFStreamDescriptor **descriptors;
    DWORD i, count;
    HRESULT hr;

    TRACE("source %p, descriptor %p, format %s, position %s\n", source, descriptor,
            debugstr_guid(format), wine_dbgstr_variant((VARIANT *)position));

    if (source->state == SOURCE_SHUTDOWN)
        return MF_E_SHUTDOWN;

    /* seek to beginning on stop->play */
    if (source->state == SOURCE_STOPPED && position->vt == VT_EMPTY)
    {
        position->vt = VT_I8;
        position->hVal.QuadPart = 0;
    }

    if (!(descriptors = calloc(source->stream_count, sizeof(*descriptors))))
        return E_OUTOFMEMORY;

    if (FAILED(hr = IMFPresentationDescriptor_GetStreamDescriptorCount(descriptor, &count)))
        WARN("Failed to get presentation descriptor stream count, hr %#lx\n", hr);

    for (i = 0; i < count; i++)
    {
        IMFStreamDescriptor *stream_descriptor;
        BOOL selected;
        DWORD id;

        if (FAILED(hr = IMFPresentationDescriptor_GetStreamDescriptorByIndex(descriptor, i,
                &selected, &stream_descriptor)))
            WARN("Failed to get presentation stream descriptor, hr %#lx\n", hr);
        else
        {
            if (FAILED(hr = IMFStreamDescriptor_GetStreamIdentifier(stream_descriptor, &id)))
                WARN("Failed to get stream descriptor id, hr %#lx\n", hr);
            else if (id > source->stream_count)
                WARN("Invalid stream descriptor id %lu, hr %#lx\n", id, hr);
            else if (selected)
                IMFStreamDescriptor_AddRef((descriptors[id - 1] = stream_descriptor));

            IMFStreamDescriptor_Release(stream_descriptor);
        }
    }

    for (i = 0; i < source->stream_count; i++)
    {
        struct media_stream *stream = source->streams[i];
        BOOL was_active = !starting && stream->active;

        if (position->vt != VT_EMPTY)
            stream->eos = FALSE;

        if (!(stream->active = !!descriptors[i]))
            wg_source_set_stream_flags(source->wg_source, source->stream_map[i], FALSE);
        else
        {
            if (FAILED(hr = media_stream_start(stream, was_active, seek_message, position)))
                WARN("Failed to start media stream, hr %#lx\n", hr);
            else
                wg_source_set_stream_flags(source->wg_source, source->stream_map[i], TRUE);
            IMFStreamDescriptor_Release(descriptors[i]);
        }
    }

    free(descriptors);

    source->state = SOURCE_RUNNING;
    if (position->vt == VT_I8)
        wg_source_set_position(source->wg_source, position->hVal.QuadPart);

    for (i = 0; i < source->stream_count; i++)
        flush_token_queue(source->streams[i], position->vt == VT_EMPTY);

    return IMFMediaEventQueue_QueueEventParamVar(source->event_queue,
            seek_message ? MESourceSeeked : MESourceStarted, &GUID_NULL, S_OK, position);
}

static HRESULT media_source_pause(struct media_source *source)
{
    unsigned int i;
    HRESULT hr;

    TRACE("source %p\n", source);

    if (source->state == SOURCE_SHUTDOWN)
        return MF_E_SHUTDOWN;

    for (i = 0; i < source->stream_count; i++)
    {
        struct media_stream *stream = source->streams[i];
        if (stream->active && FAILED(hr = IMFMediaEventQueue_QueueEventParamVar(stream->event_queue, MEStreamPaused,
                    &GUID_NULL, S_OK, NULL)))
            WARN("Failed to queue MEStreamPaused event, hr %#lx\n", hr);
    }

    source->state = SOURCE_PAUSED;
    return IMFMediaEventQueue_QueueEventParamVar(source->event_queue, MESourcePaused, &GUID_NULL, S_OK, NULL);
}

static HRESULT media_source_stop(struct media_source *source)
{
    unsigned int i;
    HRESULT hr;

    TRACE("source %p\n", source);

    if (source->state == SOURCE_SHUTDOWN)
        return MF_E_SHUTDOWN;

    for (i = 0; i < source->stream_count; i++)
    {
        struct media_stream *stream = source->streams[i];
        if (stream->active && FAILED(hr = IMFMediaEventQueue_QueueEventParamVar(stream->event_queue, MEStreamStopped,
                    &GUID_NULL, S_OK, NULL)))
            WARN("Failed to queue MEStreamStopped event, hr %#lx\n", hr);
    }

    source->state = SOURCE_STOPPED;

    for (i = 0; i < source->stream_count; i++)
        flush_token_queue(source->streams[i], FALSE);

    return IMFMediaEventQueue_QueueEventParamVar(source->event_queue, MESourceStopped, &GUID_NULL, S_OK, NULL);
}

static HRESULT media_stream_send_eos(struct media_source *source, struct media_stream *stream)
{
    PROPVARIANT empty = {.vt = VT_EMPTY};
    HRESULT hr;
    UINT i;

    TRACE("source %p, stream %p\n", source, stream);

    stream->eos = TRUE;
    if (FAILED(hr = IMFMediaEventQueue_QueueEventParamVar(stream->event_queue, MEEndOfStream, &GUID_NULL, S_OK, &empty)))
        WARN("Failed to queue MEEndOfStream event, hr %#lx\n", hr);

    for (i = 0; i < source->stream_count; i++)
    {
        struct media_stream *stream = source->streams[i];
        if (stream->active && !stream->eos)
            return S_OK;
    }

    if (FAILED(hr = IMFMediaEventQueue_QueueEventParamVar(source->event_queue, MEEndOfPresentation, &GUID_NULL, S_OK, &empty)))
        WARN("Failed to queue MEEndOfPresentation event, hr %#lx\n", hr);
    return S_OK;
}

static HRESULT wait_on_sample(struct media_stream *stream, IUnknown *token)
{
    struct media_source *source = impl_from_IMFMediaSource(stream->media_source);
    DWORD id, read_size;
    UINT64 read_offset;
    IMFSample *sample;
    HRESULT hr;

    TRACE("%p, %p\n", stream, token);

    if (FAILED(hr = IMFStreamDescriptor_GetStreamIdentifier(stream->descriptor, &id)))
        return hr;
    id = source->stream_map[id - 1];

    while (SUCCEEDED(hr) && (hr = wg_source_read_data(source->wg_source, id, &sample)) == E_PENDING)
    {
        if (FAILED(hr = wg_source_get_position(source->wg_source, &read_offset)))
            break;
        if (FAILED(hr = IMFByteStream_SetCurrentPosition(source->byte_stream, read_offset)))
            WARN("Failed to seek stream to %#I64x, hr %#lx\n", read_offset, hr);
        else if (FAILED(hr = IMFByteStream_Read(source->byte_stream, source->read_buffer, SOURCE_BUFFER_SIZE, &read_size)))
            WARN("Failed to read %#lx bytes from stream, hr %#lx\n", read_size, hr);
        else if (FAILED(hr = wg_source_push_data(source->wg_source, source->read_buffer, read_size)))
            WARN("Failed to push %#lx bytes to source, hr %#lx\n", read_size, hr);
    }

    if (FAILED(hr))
        WARN("Failed to read stream %lu data, hr %#lx\n", id, hr);
    else
    {
        if (!token || SUCCEEDED(hr = IMFSample_SetUnknown(sample, &MFSampleExtension_Token, token)))
            hr = IMFMediaEventQueue_QueueEventParamUnk(stream->event_queue, MEMediaSample,
                    &GUID_NULL, S_OK, (IUnknown *)sample);
        IMFSample_Release(sample);
        return hr;
    }

    return media_stream_send_eos(source, stream);
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
        {
            IMFPresentationDescriptor *descriptor = command->u.start.descriptor;
            GUID format = command->u.start.format;
            PROPVARIANT position = command->u.start.position;

            if (FAILED(hr = media_source_start(source, descriptor, &format, &position)))
                WARN("Failed to start source %p, hr %#lx\n", source, hr);
            break;
        }
        case SOURCE_ASYNC_PAUSE:
            if (FAILED(hr = media_source_pause(source)))
                WARN("Failed to pause source %p, hr %#lx\n", source, hr);
            break;
        case SOURCE_ASYNC_STOP:
            if (FAILED(hr = media_source_stop(source)))
                WARN("Failed to stop source %p, hr %#lx\n", source, hr);
            break;
        case SOURCE_ASYNC_REQUEST_SAMPLE:
            if (source->state == SOURCE_PAUSED)
                enqueue_token(command->u.request_sample.stream, command->u.request_sample.token);
            else if (source->state == SOURCE_RUNNING)
            {
                if (FAILED(hr = wait_on_sample(command->u.request_sample.stream, command->u.request_sample.token)))
                    WARN("Failed to request sample, hr %#lx\n", hr);
            }
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
    IUnknown *op;
    HRESULT hr;

    TRACE("%p, %p.\n", iface, token);

    EnterCriticalSection(&source->cs);

    if (source->state == SOURCE_SHUTDOWN)
        hr = MF_E_SHUTDOWN;
    else if (!stream->active)
        hr = MF_E_MEDIA_SOURCE_WRONGSTATE;
    else if (stream->eos)
        hr = MF_E_END_OF_STREAM;
    else if (SUCCEEDED(hr = source_create_async_op(SOURCE_ASYNC_REQUEST_SAMPLE, &op)))
    {
        struct source_async_command *command = impl_from_async_command_IUnknown(op);
        command->u.request_sample.stream = stream;
        if (token)
            IUnknown_AddRef(token);
        command->u.request_sample.token = token;

        hr = MFPutWorkItem(source->async_commands_queue, &source->async_commands_callback, op);
        IUnknown_Release(op);
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

static HRESULT media_stream_create(IMFMediaSource *source, IMFStreamDescriptor *descriptor, struct media_stream **out)
{
    struct media_stream *object;
    HRESULT hr;

    TRACE("source %p, descriptor %p.\n", source, descriptor);

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
    IMFStreamDescriptor_AddRef(descriptor);
    object->descriptor = descriptor;

    TRACE("Created stream object %p.\n", object);

    *out = object;
    return S_OK;
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
        IMFMediaEventQueue_Release(source->event_queue);
        IMFByteStream_Release(source->byte_stream);
        wg_source_destroy(source->wg_source);
        free(source->read_buffer);
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
    UINT i;

    TRACE("%p, %p.\n", iface, descriptor);

    EnterCriticalSection(&source->cs);

    if (source->state == SOURCE_SHUTDOWN)
        hr = MF_E_SHUTDOWN;
    else if (SUCCEEDED(hr = MFCreatePresentationDescriptor(source->stream_count, source->descriptors, descriptor)))
    {
        if (FAILED(hr = IMFPresentationDescriptor_SetString(*descriptor, &MF_PD_MIME_TYPE, source->mime_type)))
            WARN("Failed to set presentation descriptor MF_PD_MIME_TYPE, hr %#lx\n", hr);
        if (FAILED(hr = IMFPresentationDescriptor_SetUINT64(*descriptor, &MF_PD_TOTAL_FILE_SIZE, source->file_size)))
            WARN("Failed to set presentation descriptor MF_PD_TOTAL_FILE_SIZE, hr %#lx\n", hr);
        if (FAILED(hr = IMFPresentationDescriptor_SetUINT64(*descriptor, &MF_PD_DURATION, source->duration)))
            WARN("Failed to set presentation descriptor MF_PD_DURATION, hr %#lx\n", hr);

        for (i = 0; i < source->stream_count; ++i)
        {
            if (!source->streams[i]->active)
                continue;
            if (FAILED(hr = IMFPresentationDescriptor_SelectStream(*descriptor, i)))
                WARN("Failed to select stream %u, hr %#lx\n", i, hr);
        }

        hr = S_OK;
    }

    LeaveCriticalSection(&source->cs);

    return hr;
}

static HRESULT WINAPI media_source_Start(IMFMediaSource *iface, IMFPresentationDescriptor *descriptor,
                                     const GUID *time_format, const PROPVARIANT *position)
{
    struct media_source *source = impl_from_IMFMediaSource(iface);
    IUnknown *op;
    HRESULT hr;

    TRACE("%p, %p, %p, %p.\n", iface, descriptor, time_format, position);

    EnterCriticalSection(&source->cs);

    if (source->state == SOURCE_SHUTDOWN)
        hr = MF_E_SHUTDOWN;
    else if (!(IsEqualIID(time_format, &GUID_NULL)))
        hr = MF_E_UNSUPPORTED_TIME_FORMAT;
    else if (SUCCEEDED(hr = source_create_async_op(SOURCE_ASYNC_START, &op)))
    {
        struct source_async_command *command = impl_from_async_command_IUnknown(op);
        command->u.start.descriptor = descriptor;
        IMFPresentationDescriptor_AddRef(descriptor);
        command->u.start.format = *time_format;
        PropVariantCopy(&command->u.start.position, position);

        hr = MFPutWorkItem(source->async_commands_queue, &source->async_commands_callback, op);
        IUnknown_Release(op);
    }

    LeaveCriticalSection(&source->cs);

    return hr;
}

static HRESULT WINAPI media_source_Stop(IMFMediaSource *iface)
{
    struct media_source *source = impl_from_IMFMediaSource(iface);
    IUnknown *op;
    HRESULT hr;

    TRACE("%p.\n", iface);

    EnterCriticalSection(&source->cs);

    if (source->state == SOURCE_SHUTDOWN)
        hr = MF_E_SHUTDOWN;
    else if (SUCCEEDED(hr = source_create_async_op(SOURCE_ASYNC_STOP, &op)))
    {
        hr = MFPutWorkItem(source->async_commands_queue, &source->async_commands_callback, op);
        IUnknown_Release(op);
    }

    LeaveCriticalSection(&source->cs);

    return hr;
}

static HRESULT WINAPI media_source_Pause(IMFMediaSource *iface)
{
    struct media_source *source = impl_from_IMFMediaSource(iface);
    IUnknown *op;
    HRESULT hr;

    TRACE("%p.\n", iface);

    EnterCriticalSection(&source->cs);

    if (source->state == SOURCE_SHUTDOWN)
        hr = MF_E_SHUTDOWN;
    else if (source->state != SOURCE_RUNNING)
        hr = MF_E_INVALID_STATE_TRANSITION;
    else if (SUCCEEDED(hr = source_create_async_op(SOURCE_ASYNC_PAUSE, &op)))
    {
        hr = MFPutWorkItem(source->async_commands_queue, &source->async_commands_callback, op);
        IUnknown_Release(op);
    }

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

    IMFMediaEventQueue_Shutdown(source->event_queue);
    IMFByteStream_Close(source->byte_stream);

    while (source->stream_count--)
    {
        struct media_stream *stream = source->streams[source->stream_count];
        IMFStreamDescriptor_Release(source->descriptors[source->stream_count]);
        IMFMediaEventQueue_Shutdown(stream->event_queue);
        IMFMediaStream_Release(&stream->IMFMediaStream_iface);
    }
    free(source->stream_map);
    free(source->descriptors);
    free(source->streams);

    LeaveCriticalSection(&source->cs);

    MFUnlockWorkQueue(source->async_commands_queue);

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

static void media_source_init_stream_map(struct media_source *source, UINT stream_count)
{
    struct wg_format format;
    int i, n = 0;

    if (wcscmp(source->mime_type, L"video/mp4"))
    {
        for (i = stream_count - 1; i >= 0; i--)
        {
            TRACE("mapping stream %u to wg_source stream %u\n", i, i);
            source->stream_map[i] = i;
        }
        return;
    }

    for (i = stream_count - 1; i >= 0; i--)
    {
        wg_source_get_stream_format(source->wg_source, i, &format);
        if (format.major_type == WG_MAJOR_TYPE_UNKNOWN) continue;
        if (format.major_type >= WG_MAJOR_TYPE_VIDEO) continue;
        TRACE("mapping stream %u to wg_source stream %u\n", n, i);
        source->stream_map[n++] = i;
    }
    for (i = stream_count - 1; i >= 0; i--)
    {
        wg_source_get_stream_format(source->wg_source, i, &format);
        if (format.major_type == WG_MAJOR_TYPE_UNKNOWN) continue;
        if (format.major_type < WG_MAJOR_TYPE_VIDEO) continue;
        TRACE("mapping stream %u to wg_source stream %u\n", n, i);
        source->stream_map[n++] = i;
    }
    for (i = stream_count - 1; i >= 0; i--)
    {
        wg_source_get_stream_format(source->wg_source, i, &format);
        if (format.major_type != WG_MAJOR_TYPE_UNKNOWN) continue;
        TRACE("mapping stream %u to wg_source stream %u\n", n, i);
        source->stream_map[n++] = i;
    }
}

static void media_source_init_descriptors(struct media_source *source)
{
    UINT i, last_audio = -1, last_video = -1, first_audio = -1, first_video = -1;
    HRESULT hr;

    for (i = 0; i < source->stream_count; i++)
    {
        IMFStreamDescriptor *descriptor = source->descriptors[i];
        GUID major = GUID_NULL;
        UINT exclude = -1;

        if (FAILED(hr = stream_descriptor_get_major_type(descriptor, &major)))
            WARN("Failed to get major type from stream descriptor, hr %#lx\n", hr);

        if (IsEqualGUID(&major, &MFMediaType_Audio))
        {
            if (first_audio == -1)
                first_audio = i;
            exclude = last_audio;
            last_audio = i;
        }
        else if (IsEqualGUID(&major, &MFMediaType_Video))
        {
            if (first_video == -1)
                first_video = i;
            exclude = last_video;
            last_video = i;
        }

        if (exclude != -1)
        {
            if (FAILED(IMFStreamDescriptor_SetUINT32(source->descriptors[exclude], &MF_SD_MUTUALLY_EXCLUSIVE, 1)))
                WARN("Failed to set stream %u MF_SD_MUTUALLY_EXCLUSIVE\n", exclude);
            else if (FAILED(IMFStreamDescriptor_SetUINT32(descriptor, &MF_SD_MUTUALLY_EXCLUSIVE, 1)))
                WARN("Failed to set stream %u MF_SD_MUTUALLY_EXCLUSIVE\n", i);
        }

        if (FAILED(hr = stream_descriptor_set_tag(descriptor, source->wg_source, source->stream_map[i],
                &MF_SD_LANGUAGE, WG_PARSER_TAG_LANGUAGE)))
            WARN("Failed to set stream descriptor language, hr %#lx\n", hr);
        if (FAILED(hr = stream_descriptor_set_tag(descriptor, source->wg_source, source->stream_map[i],
                &MF_SD_STREAM_NAME, WG_PARSER_TAG_NAME)))
            WARN("Failed to set stream descriptor name, hr %#lx\n", hr);
    }

    if (!wcscmp(source->mime_type, L"video/mp4"))
    {
        if (last_audio != -1)
            source->streams[last_audio]->active = TRUE;
        if (last_video != -1)
            source->streams[last_video]->active = TRUE;
    }
    else
    {
        if (first_audio != -1)
            source->streams[first_audio]->active = TRUE;
        if (first_video != -1)
            source->streams[first_video]->active = TRUE;
    }
}

static HRESULT media_source_create(struct object_context *context, IMFMediaSource **out)
{
    UINT32 stream_count = context->stream_count;
    struct media_source *object;
    unsigned int i;
    HRESULT hr;

    if (!(object = calloc(1, sizeof(*object))))
        return E_OUTOFMEMORY;

    object->IMFMediaSource_iface.lpVtbl = &IMFMediaSource_vtbl;
    object->IMFGetService_iface.lpVtbl = &media_source_get_service_vtbl;
    object->IMFRateSupport_iface.lpVtbl = &media_source_rate_support_vtbl;
    object->IMFRateControl_iface.lpVtbl = &media_source_rate_control_vtbl;
    object->async_commands_callback.lpVtbl = &source_async_commands_callback_vtbl;
    object->ref = 1;
    object->byte_stream = context->stream;
    IMFByteStream_AddRef(context->stream);
    wcscpy(object->mime_type, context->mime_type);
    object->file_size = context->file_size;
    object->rate = 1.0f;
    InitializeCriticalSection(&object->cs);
    object->cs.DebugInfo->Spare[0] = (DWORD_PTR)(__FILE__ ": cs");

    object->read_buffer = context->buffer;
    context->buffer = NULL;
    object->wg_source = context->wg_source;
    context->wg_source = 0;

    if (FAILED(hr = MFCreateEventQueue(&object->event_queue)))
        goto fail;
    if (FAILED(hr = MFAllocateWorkQueue(&object->async_commands_queue)))
        goto fail;
    if (FAILED(hr = wg_source_get_duration(object->wg_source, &object->duration)))
        goto fail;
    object->state = SOURCE_OPENING;

    if (!(object->descriptors = calloc(stream_count, sizeof(*object->descriptors)))
            || !(object->stream_map = calloc(stream_count, sizeof(*object->stream_map)))
            || !(object->streams = calloc(stream_count, sizeof(*object->streams))))
    {
        hr = E_OUTOFMEMORY;
        goto fail;
    }

    media_source_init_stream_map(object, stream_count);

    for (i = 0; i < stream_count; ++i)
    {
        IMFStreamDescriptor *descriptor;
        struct media_stream *stream;
        struct wg_format format;

        if (FAILED(hr = wg_source_get_stream_format(object->wg_source, object->stream_map[i], &format)))
            goto fail;
        if (FAILED(hr = stream_descriptor_create(i + 1, &format, &descriptor)))
            goto fail;
        if (FAILED(hr = media_stream_create(&object->IMFMediaSource_iface, descriptor, &stream)))
        {
            IMFStreamDescriptor_Release(descriptor);
            goto fail;
        }

        IMFStreamDescriptor_AddRef(descriptor);
        object->descriptors[i] = descriptor;
        object->streams[i] = stream;
        object->stream_count++;
    }

    media_source_init_descriptors(object);
    object->state = SOURCE_STOPPED;

    *out = &object->IMFMediaSource_iface;
    TRACE("Created IMFMediaSource %p\n", *out);
    return S_OK;

fail:
    WARN("Failed to construct MFMediaSource, hr %#lx.\n", hr);

    while (object->streams && object->stream_count--)
    {
        struct media_stream *stream = object->streams[object->stream_count];
        IMFStreamDescriptor_Release(object->descriptors[object->stream_count]);
        IMFMediaStream_Release(&stream->IMFMediaStream_iface);
    }
    free(object->stream_map);
    free(object->descriptors);
    free(object->streams);

    if (object->wg_source)
        wg_source_destroy(object->wg_source);
    if (object->async_commands_queue)
        MFUnlockWorkQueue(object->async_commands_queue);
    if (object->event_queue)
        IMFMediaEventQueue_Release(object->event_queue);
    IMFByteStream_Release(object->byte_stream);
    free(object->read_buffer);
    free(object);

    return hr;
}

struct result_entry
{
    struct list entry;
    IMFAsyncResult *result;
    MF_OBJECT_TYPE type;
    IUnknown *object;
};

static HRESULT result_entry_create(IMFAsyncResult *result, MF_OBJECT_TYPE type,
        IUnknown *object, struct result_entry **out)
{
    struct result_entry *entry;

    if (!(entry = malloc(sizeof(*entry))))
        return E_OUTOFMEMORY;

    entry->result = result;
    IMFAsyncResult_AddRef(entry->result);
    entry->object = object;
    IUnknown_AddRef(entry->object);
    entry->type = type;

    *out = entry;
    return S_OK;
}

static void result_entry_destroy(struct result_entry *entry)
{
    IMFAsyncResult_Release(entry->result);
    IUnknown_Release(entry->object);
    free(entry);
}

struct stream_handler
{
    IMFByteStreamHandler IMFByteStreamHandler_iface;
    IMFAsyncCallback IMFAsyncCallback_iface;
    LONG refcount;
    struct list results;
    CRITICAL_SECTION cs;
};

static struct result_entry *handler_find_result_entry(struct stream_handler *handler, IMFAsyncResult *result)
{
    struct result_entry *entry;

    EnterCriticalSection(&handler->cs);
    LIST_FOR_EACH_ENTRY(entry, &handler->results, struct result_entry, entry)
    {
        if (result == entry->result)
        {
            list_remove(&entry->entry);
            LeaveCriticalSection(&handler->cs);
            return entry;
        }
    }
    LeaveCriticalSection(&handler->cs);

    return NULL;
}

static struct stream_handler *impl_from_IMFByteStreamHandler(IMFByteStreamHandler *iface)
{
    return CONTAINING_RECORD(iface, struct stream_handler, IMFByteStreamHandler_iface);
}

static struct stream_handler *impl_from_IMFAsyncCallback(IMFAsyncCallback *iface)
{
    return CONTAINING_RECORD(iface, struct stream_handler, IMFAsyncCallback_iface);
}

static HRESULT WINAPI stream_handler_QueryInterface(IMFByteStreamHandler *iface, REFIID riid, void **obj)
{
    TRACE("%p, %s, %p.\n", iface, debugstr_guid(riid), obj);

    if (IsEqualIID(riid, &IID_IMFByteStreamHandler) ||
            IsEqualIID(riid, &IID_IUnknown))
    {
        *obj = iface;
        IMFByteStreamHandler_AddRef(iface);
        return S_OK;
    }

    WARN("Unsupported %s.\n", debugstr_guid(riid));
    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI stream_handler_AddRef(IMFByteStreamHandler *iface)
{
    struct stream_handler *handler = impl_from_IMFByteStreamHandler(iface);
    ULONG refcount = InterlockedIncrement(&handler->refcount);

    TRACE("%p, refcount %lu.\n", handler, refcount);

    return refcount;
}

static ULONG WINAPI stream_handler_Release(IMFByteStreamHandler *iface)
{
    struct stream_handler *handler = impl_from_IMFByteStreamHandler(iface);
    ULONG refcount = InterlockedDecrement(&handler->refcount);
    struct result_entry *result, *next;

    TRACE("%p, refcount %lu.\n", iface, refcount);

    if (!refcount)
    {
        LIST_FOR_EACH_ENTRY_SAFE(result, next, &handler->results, struct result_entry, entry)
            result_entry_destroy(result);
        DeleteCriticalSection(&handler->cs);
        free(handler);
    }

    return refcount;
}

static HRESULT WINAPI stream_handler_BeginCreateObject(IMFByteStreamHandler *iface, IMFByteStream *stream, const WCHAR *url, DWORD flags,
        IPropertyStore *props, IUnknown **cancel_cookie, IMFAsyncCallback *callback, IUnknown *state)
{
    struct stream_handler *handler = impl_from_IMFByteStreamHandler(iface);
    IMFAsyncResult *result;
    IUnknown *context;
    QWORD file_size, position;
    BYTE *buffer;
    HRESULT hr;
    DWORD caps;

    TRACE("%p, %s, %#lx, %p, %p, %p, %p.\n", iface, debugstr_w(url), flags, props, cancel_cookie, callback, state);

    if (cancel_cookie)
        *cancel_cookie = NULL;

    if (!stream)
        return E_INVALIDARG;
    if (flags != MF_RESOLUTION_MEDIASOURCE)
        FIXME("Unimplemented flags %#lx\n", flags);

    if (FAILED(hr = IMFByteStream_GetCapabilities(stream, &caps)))
        return hr;
    if (!(caps & MFBYTESTREAM_IS_SEEKABLE))
    {
        FIXME("Non-seekable bytestreams not supported.\n");
        return MF_E_BYTESTREAM_NOT_SEEKABLE;
    }
    if (FAILED(hr = IMFByteStream_GetLength(stream, &file_size)))
    {
        FIXME("Failed to get byte stream length, hr %#lx.\n", hr);
        return hr;
    }

    if (FAILED(hr = MFCreateAsyncResult(NULL, callback, state, &result)))
        return hr;
    if (FAILED(hr = object_context_create(flags, stream, url, file_size, result, &context, &buffer)))
        goto done;

    if (FAILED(hr = IMFByteStream_GetCurrentPosition(stream, &position)))
        WARN("Failed to get byte stream position, hr %#lx\n", hr);
    else if (position != 0 && FAILED(hr = IMFByteStream_SetCurrentPosition(stream, 0)))
        WARN("Failed to set byte stream position, hr %#lx\n", hr);
    else if (FAILED(hr = IMFByteStream_BeginRead(stream, buffer, min(SOURCE_BUFFER_SIZE, file_size),
            &handler->IMFAsyncCallback_iface, context)))
        WARN("Failed to queue byte stream async read, hr %#lx\n", hr);
    IUnknown_Release(context);

    if (SUCCEEDED(hr) && cancel_cookie)
    {
        *cancel_cookie = (IUnknown *)result;
        IUnknown_AddRef(*cancel_cookie);
    }

done:
    IMFAsyncResult_Release(result);

    return hr;
}

static HRESULT WINAPI stream_handler_EndCreateObject(IMFByteStreamHandler *iface, IMFAsyncResult *result,
        MF_OBJECT_TYPE *type, IUnknown **object)
{
    struct stream_handler *handler = impl_from_IMFByteStreamHandler(iface);
    struct result_entry *entry;
    HRESULT hr;

    TRACE("%p, %p, %p, %p.\n", iface, result, type, object);

    if (!(entry = handler_find_result_entry(handler, result)))
    {
        *type = MF_OBJECT_INVALID;
        *object = NULL;
        return MF_E_UNEXPECTED;
    }

    hr = IMFAsyncResult_GetStatus(entry->result);
    *type = entry->type;
    *object = entry->object;
    IUnknown_AddRef(*object);
    result_entry_destroy(entry);
    return hr;
}

static HRESULT WINAPI stream_handler_CancelObjectCreation(IMFByteStreamHandler *iface, IUnknown *cookie)
{
    struct stream_handler *handler = impl_from_IMFByteStreamHandler(iface);
    IMFAsyncResult *result = (IMFAsyncResult *)cookie;
    struct result_entry *entry;

    TRACE("%p, %p.\n", iface, cookie);

    if (!(entry = handler_find_result_entry(handler, result)))
        return MF_E_UNEXPECTED;

    result_entry_destroy(entry);
    return S_OK;
}

static HRESULT WINAPI stream_handler_GetMaxNumberOfBytesRequiredForResolution(IMFByteStreamHandler *iface, QWORD *bytes)
{
    FIXME("stub (%p %p)\n", iface, bytes);
    return E_NOTIMPL;
}

static const IMFByteStreamHandlerVtbl stream_handler_vtbl =
{
    stream_handler_QueryInterface,
    stream_handler_AddRef,
    stream_handler_Release,
    stream_handler_BeginCreateObject,
    stream_handler_EndCreateObject,
    stream_handler_CancelObjectCreation,
    stream_handler_GetMaxNumberOfBytesRequiredForResolution,
};

static HRESULT WINAPI stream_handler_callback_QueryInterface(IMFAsyncCallback *iface, REFIID riid, void **obj)
{
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

static ULONG WINAPI stream_handler_callback_AddRef(IMFAsyncCallback *iface)
{
    struct stream_handler *handler = impl_from_IMFAsyncCallback(iface);
    return IMFByteStreamHandler_AddRef(&handler->IMFByteStreamHandler_iface);
}

static ULONG WINAPI stream_handler_callback_Release(IMFAsyncCallback *iface)
{
    struct stream_handler *handler = impl_from_IMFAsyncCallback(iface);
    return IMFByteStreamHandler_Release(&handler->IMFByteStreamHandler_iface);
}

static HRESULT WINAPI stream_handler_callback_GetParameters(IMFAsyncCallback *iface, DWORD *flags, DWORD *queue)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI stream_handler_callback_Invoke(IMFAsyncCallback *iface, IMFAsyncResult *result)
{
    struct stream_handler *handler = impl_from_IMFAsyncCallback(iface);
    IUnknown *object, *state = IMFAsyncResult_GetStateNoAddRef(result);
    struct object_context *context;
    struct result_entry *entry;
    UINT64 read_offset;
    DWORD size = 0;
    HRESULT hr;

    if (!state || !(context = impl_from_IUnknown(state)))
        return E_INVALIDARG;

    if (FAILED(hr = IMFByteStream_EndRead(context->stream, result, &size)))
        WARN("Failed to complete stream read, hr %#lx\n", hr);
    else if (!context->wg_source && FAILED(hr = wg_source_create(context->url, context->file_size,
            context->buffer, size, context->mime_type, &context->wg_source)))
        WARN("Failed to create wg_source, hr %#lx\n", hr);
    else if (FAILED(hr = wg_source_push_data(context->wg_source, context->buffer, size)))
        WARN("Failed to push wg_source data, hr %#lx\n", hr);
    else if (FAILED(hr = wg_source_get_stream_count(context->wg_source, &context->stream_count)))
        WARN("Failed to get wg_source status, hr %#lx\n", hr);
    else if (!context->stream_count)
    {
        QWORD position, offset;
        if (FAILED(hr = wg_source_get_position(context->wg_source, &read_offset)))
            WARN("Failed to get wg_source position, hr %#lx\n", hr);
        else if (FAILED(hr = IMFByteStream_GetCurrentPosition(context->stream, &position)))
            WARN("Failed to get current byte stream position, hr %#lx\n", hr);
        else if (position != (offset = min(read_offset, context->file_size))
                && FAILED(hr = IMFByteStream_SetCurrentPosition(context->stream, offset)))
            WARN("Failed to set current byte stream position, hr %#lx\n", hr);
        else
        {
            UINT32 read_size = min(SOURCE_BUFFER_SIZE, context->file_size - offset);
            return IMFByteStream_BeginRead(context->stream, context->buffer, read_size,
                    &handler->IMFAsyncCallback_iface, state);
        }
    }
    else if (FAILED(hr = media_source_create(context, (IMFMediaSource **)&object)))
        WARN("Failed to create media source, hr %#lx\n", hr);

    if (FAILED(hr))
    {
        FIXME("Falling back to old media source, hr %#lx\n", hr);
        hr = create_media_source_fallback(context, (IUnknown **)&object);
    }

    if (SUCCEEDED(hr))
    {
        if (FAILED(hr = result_entry_create(context->result, MF_OBJECT_MEDIASOURCE, object, &entry)))
            WARN("Failed to create handler result, hr %#lx\n", hr);
        else
        {
            EnterCriticalSection(&handler->cs);
            list_add_tail(&handler->results, &entry->entry);
            LeaveCriticalSection(&handler->cs);
        }

        IUnknown_Release(object);
    }

    IMFAsyncResult_SetStatus(context->result, hr);
    MFInvokeCallback(context->result);

    return S_OK;
}

static const IMFAsyncCallbackVtbl stream_handler_callback_vtbl =
{
    stream_handler_callback_QueryInterface,
    stream_handler_callback_AddRef,
    stream_handler_callback_Release,
    stream_handler_callback_GetParameters,
    stream_handler_callback_Invoke,
};

HRESULT gstreamer_byte_stream_handler_2_create(REFIID riid, void **obj)
{
    struct stream_handler *handler;
    HRESULT hr;

    TRACE("%s, %p.\n", debugstr_guid(riid), obj);

    if (!(handler = calloc(1, sizeof(*handler))))
        return E_OUTOFMEMORY;

    list_init(&handler->results);
    InitializeCriticalSection(&handler->cs);

    handler->IMFByteStreamHandler_iface.lpVtbl = &stream_handler_vtbl;
    handler->IMFAsyncCallback_iface.lpVtbl = &stream_handler_callback_vtbl;
    handler->refcount = 1;

    hr = IMFByteStreamHandler_QueryInterface(&handler->IMFByteStreamHandler_iface, riid, obj);
    IMFByteStreamHandler_Release(&handler->IMFByteStreamHandler_iface);

    return hr;
}
