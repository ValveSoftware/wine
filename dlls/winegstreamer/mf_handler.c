/*
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

#include <stdarg.h>

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "mfidl.h"
#include "mferror.h"
#include "mfapi.h"
#include "gst_private.h"

#include "wine/debug.h"
#include "wine/list.h"

WINE_DEFAULT_DEBUG_CHANNEL(mfplat);

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

    IMFAsyncResult_AddRef((entry->result = result));
    entry->type = type;
    if ((entry->object = object))
        IUnknown_AddRef(entry->object);

    *out = entry;
    return S_OK;
}

static void result_entry_destroy(struct result_entry *entry)
{
    IMFAsyncResult_Release(entry->result);
    if (entry->object)
        IUnknown_Release(entry->object);
    free(entry);
}

struct async_create_object
{
    IUnknown IUnknown_iface;
    LONG refcount;

    IMFByteStream *stream;
    WCHAR *url;
    DWORD flags;
    IMFAsyncResult *result;

    UINT64 size;
    BYTE buffer[];
};

C_ASSERT(sizeof(struct async_create_object) == offsetof(struct async_create_object, buffer[0]));

static struct async_create_object *impl_from_IUnknown(IUnknown *iface)
{
    if (!iface) return NULL;
    return CONTAINING_RECORD(iface, struct async_create_object, IUnknown_iface);
}

static HRESULT WINAPI async_create_object_QueryInterface(IUnknown *iface, REFIID riid, void **obj)
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

static ULONG WINAPI async_create_object_AddRef(IUnknown *iface)
{
    struct async_create_object *async = impl_from_IUnknown(iface);
    ULONG refcount = InterlockedIncrement(&async->refcount);
    TRACE("%p, refcount %lu.\n", iface, refcount);
    return refcount;
}

static ULONG WINAPI async_create_object_Release(IUnknown *iface)
{
    struct async_create_object *async = impl_from_IUnknown(iface);
    ULONG refcount = InterlockedDecrement(&async->refcount);

    TRACE("%p, refcount %lu.\n", iface, refcount);

    if (!refcount)
    {
        IMFAsyncResult_Release(async->result);
        if (async->stream)
            IMFByteStream_Release(async->stream);
        free(async->url);
        free(async);
    }

    return refcount;
}

static const IUnknownVtbl async_create_object_vtbl =
{
    async_create_object_QueryInterface,
    async_create_object_AddRef,
    async_create_object_Release,
};

static HRESULT async_create_object_create(DWORD flags, IMFByteStream *stream, const WCHAR *url,
        IMFAsyncResult *result, UINT size, IUnknown **out, BYTE **buffer)
{
    WCHAR *tmp_url = url ? wcsdup(url) : NULL;
    struct async_create_object *impl;

    if (!stream && !tmp_url)
        return E_INVALIDARG;
    if (!(impl = calloc(1, offsetof(struct async_create_object, buffer[size]))))
    {
        free(tmp_url);
        return E_OUTOFMEMORY;
    }

    impl->IUnknown_iface.lpVtbl = &async_create_object_vtbl;
    impl->refcount = 1;
    impl->flags = flags;
    if ((impl->stream = stream))
        IMFByteStream_AddRef(impl->stream);
    impl->url = tmp_url;
    IMFAsyncResult_AddRef((impl->result = result));

    *buffer = impl->buffer;
    *out = &impl->IUnknown_iface;
    return S_OK;
}

static HRESULT async_create_object_complete(struct async_create_object *async,
        struct list *results, CRITICAL_SECTION *results_cs)
{
    IUnknown *object;
    HRESULT hr;

    if (async->flags & MF_RESOLUTION_MEDIASOURCE)
    {
        const char *sgi, *env = getenv("WINE_NEW_MEDIA_SOURCE");
        if (!env && (sgi = getenv("SteamGameId")) &&
            (!strcmp(sgi, "692850") || !strcmp(sgi, "559100"))) env = "1";

        if (!async->stream)
            hr = media_source_create_from_url(async->url, (IMFMediaSource **)&object);
        else if (!env || !atoi(env))
            hr = media_source_create_old(async->stream, NULL, (IMFMediaSource **)&object);
        else if (FAILED(hr = media_source_create(async->stream, async->url, async->buffer, async->size, (IMFMediaSource **)&object)))
        {
            FIXME("Failed to create new media source, falling back to old implementation, hr %#lx\n", hr);
            hr = media_source_create_old(async->stream, NULL, (IMFMediaSource **)&object);
        }
    }
    else
    {
        FIXME("Unhandled flags %#lx.\n", async->flags);
        hr = E_NOTIMPL;
    }

    if (FAILED(hr))
        WARN("Failed to create object, hr %#lx.\n", hr);
    else
    {
        struct result_entry *entry;

        if (FAILED(hr = result_entry_create(async->result, MF_OBJECT_MEDIASOURCE, object, &entry)))
            WARN("Failed to add handler result, hr %#lx\n", hr);
        else
        {
            EnterCriticalSection(results_cs);
            list_add_tail(results, &entry->entry);
            LeaveCriticalSection(results_cs);
        }

        IUnknown_Release(object);
    }

    IMFAsyncResult_SetStatus(async->result, hr);
    return MFInvokeCallback(async->result);
}

struct handler
{
    IMFAsyncCallback IMFAsyncCallback_iface;
    IMFByteStreamHandler IMFByteStreamHandler_iface;
    IMFSchemeHandler IMFSchemeHandler_iface;
    LONG refcount;
    struct list results;
    CRITICAL_SECTION cs;
};

static HRESULT handler_begin_create_object(struct handler *handler, DWORD flags,
        IMFByteStream *stream, const WCHAR *url, IMFAsyncResult *result)
{
    UINT size = 0x2000;
    IUnknown *async;
    HRESULT hr;
    BYTE *buffer;

    if (SUCCEEDED(hr = async_create_object_create(flags, stream, url, result, size, &async, &buffer)))
    {
        if (stream && FAILED(hr = IMFByteStream_BeginRead(stream, buffer, size, &handler->IMFAsyncCallback_iface, async)))
            WARN("Failed to begin reading from stream, hr %#lx\n", hr);
        if (!stream && FAILED(hr = MFPutWorkItem(MFASYNC_CALLBACK_QUEUE_IO, &handler->IMFAsyncCallback_iface, async)))
            WARN("Failed to queue async work item, hr %#lx\n", hr);
        IUnknown_Release(async);
    }

    return hr;
}

static struct result_entry *handler_find_result_entry(struct handler *handler, IMFAsyncResult *result)
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

static HRESULT handler_end_create_object(struct handler *handler,
        IMFAsyncResult *result, MF_OBJECT_TYPE *type, IUnknown **object)
{
    struct result_entry *entry;
    HRESULT hr;

    if (!(entry = handler_find_result_entry(handler, result)))
    {
        *type = MF_OBJECT_INVALID;
        *object = NULL;
        return MF_E_UNEXPECTED;
    }

    hr = IMFAsyncResult_GetStatus(entry->result);
    *type = entry->type;
    *object = entry->object;
    entry->object = NULL;

    result_entry_destroy(entry);
    return hr;
}

static HRESULT handler_cancel_object_creation(struct handler *handler, IUnknown *cookie)
{
    IMFAsyncResult *result = (IMFAsyncResult *)cookie;
    struct result_entry *entry;

    if (!(entry = handler_find_result_entry(handler, result)))
        return MF_E_UNEXPECTED;

    result_entry_destroy(entry);
    return S_OK;
}

static struct handler *impl_from_IMFAsyncCallback(IMFAsyncCallback *iface)
{
    return CONTAINING_RECORD(iface, struct handler, IMFAsyncCallback_iface);
}

static struct handler *impl_from_IMFByteStreamHandler(IMFByteStreamHandler *iface)
{
    return CONTAINING_RECORD(iface, struct handler, IMFByteStreamHandler_iface);
}

static struct handler *impl_from_IMFSchemeHandler(IMFSchemeHandler *iface)
{
    return CONTAINING_RECORD(iface, struct handler, IMFSchemeHandler_iface);
}

static HRESULT WINAPI async_callback_QueryInterface(IMFAsyncCallback *iface, REFIID riid, void **obj)
{
    if (IsEqualIID(riid, &IID_IMFAsyncCallback)
            || IsEqualIID(riid, &IID_IUnknown))
    {
        *obj = iface;
        IMFAsyncCallback_AddRef(iface);
        return S_OK;
    }

    WARN("Unsupported %s.\n", debugstr_guid(riid));
    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI async_callback_AddRef(IMFAsyncCallback *iface)
{
    struct handler *handler = impl_from_IMFAsyncCallback(iface);
    ULONG refcount = InterlockedIncrement(&handler->refcount);
    TRACE("%p, refcount %lu.\n", handler, refcount);
    return refcount;
}

static ULONG WINAPI async_callback_Release(IMFAsyncCallback *iface)
{
    struct handler *handler = impl_from_IMFAsyncCallback(iface);
    ULONG refcount = InterlockedDecrement(&handler->refcount);
    struct result_entry *entry, *next;

    TRACE("%p, refcount %lu.\n", iface, refcount);

    if (!refcount)
    {
        LIST_FOR_EACH_ENTRY_SAFE(entry, next, &handler->results, struct result_entry, entry)
            result_entry_destroy(entry);
        DeleteCriticalSection(&handler->cs);
        free(handler);
    }

    return refcount;
}

static HRESULT WINAPI async_callback_GetParameters(IMFAsyncCallback *iface, DWORD *flags, DWORD *queue)
{
    *flags = 0;
    *queue = MFASYNC_CALLBACK_QUEUE_IO;
    return S_OK;
}

static HRESULT WINAPI async_callback_Invoke(IMFAsyncCallback *iface, IMFAsyncResult *result)
{
    struct handler *handler = impl_from_IMFAsyncCallback(iface);
    struct async_create_object *async;
    ULONG size = 0;
    HRESULT hr;

    TRACE("iface %p, result %p\n", iface, result);

    if (!(async = impl_from_IUnknown(IMFAsyncResult_GetStateNoAddRef(result))))
    {
        WARN("Expected context set for callee result.\n");
        return E_FAIL;
    }

    if (async->stream && FAILED(hr = IMFByteStream_EndRead(async->stream, result, &size)))
        WARN("Failed to complete stream read, hr %#lx\n", hr);
    async->size = size;

    return async_create_object_complete(async, &handler->results, &handler->cs);
}

static const IMFAsyncCallbackVtbl async_callback_vtbl =
{
    async_callback_QueryInterface,
    async_callback_AddRef,
    async_callback_Release,
    async_callback_GetParameters,
    async_callback_Invoke,
};

static HRESULT WINAPI stream_handler_QueryInterface(IMFByteStreamHandler *iface, REFIID riid, void **obj)
{
    TRACE("%p, %s, %p.\n", iface, debugstr_guid(riid), obj);

    if (IsEqualIID(riid, &IID_IMFByteStreamHandler)
            || IsEqualIID(riid, &IID_IUnknown))
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
    struct handler *handler = impl_from_IMFByteStreamHandler(iface);
    return IMFAsyncCallback_AddRef(&handler->IMFAsyncCallback_iface);
}

static ULONG WINAPI stream_handler_Release(IMFByteStreamHandler *iface)
{
    struct handler *handler = impl_from_IMFByteStreamHandler(iface);
    return IMFAsyncCallback_Release(&handler->IMFAsyncCallback_iface);
}

static HRESULT WINAPI stream_handler_BeginCreateObject(IMFByteStreamHandler *iface,
        IMFByteStream *stream, const WCHAR *url, DWORD flags, IPropertyStore *props,
        IUnknown **cookie, IMFAsyncCallback *callback, IUnknown *state)
{
    struct handler *handler = impl_from_IMFByteStreamHandler(iface);
    IMFAsyncResult *result;
    HRESULT hr;

    TRACE("%p, %s, %#lx, %p, %p, %p, %p.\n", iface, debugstr_w(url), flags, props, cookie, callback, state);

    if (cookie)
        *cookie = NULL;

    if (FAILED(hr = MFCreateAsyncResult((IUnknown *)iface, callback, state, &result)))
        return hr;

    if (SUCCEEDED(hr = handler_begin_create_object(handler, flags, stream, url, result)) && cookie)
    {
        *cookie = (IUnknown *)result;
        IUnknown_AddRef(*cookie);
    }

    IMFAsyncResult_Release(result);

    return hr;
}

static HRESULT WINAPI stream_handler_EndCreateObject(IMFByteStreamHandler *iface,
        IMFAsyncResult *result, MF_OBJECT_TYPE *type, IUnknown **object)
{
    struct handler *handler = impl_from_IMFByteStreamHandler(iface);
    TRACE("%p, %p, %p, %p.\n", iface, result, type, object);
    return handler_end_create_object(handler, result, type, object);
}

static HRESULT WINAPI stream_handler_CancelObjectCreation(IMFByteStreamHandler *iface, IUnknown *cookie)
{
    struct handler *handler = impl_from_IMFByteStreamHandler(iface);
    TRACE("%p, %p.\n", iface, cookie);
    return handler_cancel_object_creation(handler, cookie);
}

static HRESULT WINAPI stream_handler_GetMaxNumberOfBytesRequiredForResolution(
        IMFByteStreamHandler *iface, QWORD *bytes)
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

static HRESULT WINAPI scheme_handler_QueryInterface(IMFSchemeHandler *iface, REFIID riid, void **obj)
{
    TRACE("%p, %s, %p.\n", iface, debugstr_guid(riid), obj);

    if (IsEqualIID(riid, &IID_IMFSchemeHandler)
            || IsEqualIID(riid, &IID_IUnknown))
    {
        *obj = iface;
        IMFSchemeHandler_AddRef(iface);
        return S_OK;
    }

    WARN("Unsupported %s.\n", debugstr_guid(riid));
    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI scheme_handler_AddRef(IMFSchemeHandler *iface)
{
    struct handler *handler = impl_from_IMFSchemeHandler(iface);
    return IMFAsyncCallback_AddRef(&handler->IMFAsyncCallback_iface);
}

static ULONG WINAPI scheme_handler_Release(IMFSchemeHandler *iface)
{
    struct handler *handler = impl_from_IMFSchemeHandler(iface);
    return IMFAsyncCallback_Release(&handler->IMFAsyncCallback_iface);
}

static HRESULT WINAPI scheme_handler_BeginCreateObject(IMFSchemeHandler *iface, const WCHAR *url,
        DWORD flags, IPropertyStore *props, IUnknown **cookie, IMFAsyncCallback *callback, IUnknown *state)
{
    struct handler *handler = impl_from_IMFSchemeHandler(iface);
    IMFAsyncResult *result;
    HRESULT hr;

    TRACE("%p, %s, %#lx, %p, %p, %p, %p.\n", iface, debugstr_w(url), flags, props, cookie, callback, state);

    if (cookie)
        *cookie = NULL;

    if (FAILED(hr = MFCreateAsyncResult((IUnknown *)iface, callback, state, &result)))
        return hr;

    if (SUCCEEDED(hr = handler_begin_create_object(handler, flags, NULL, url, result)) && cookie)
    {
        *cookie = (IUnknown *)result;
        IUnknown_AddRef(*cookie);
    }

    IMFAsyncResult_Release(result);

    return hr;
}

static HRESULT WINAPI scheme_handler_EndCreateObject(IMFSchemeHandler *iface,
        IMFAsyncResult *result, MF_OBJECT_TYPE *type, IUnknown **object)
{
    struct handler *handler = impl_from_IMFSchemeHandler(iface);
    TRACE("%p, %p, %p, %p.\n", iface, result, type, object);
    return handler_end_create_object(handler, result, type, object);
}

static HRESULT WINAPI scheme_handler_CancelObjectCreation(IMFSchemeHandler *iface, IUnknown *cookie)
{
    struct handler *handler = impl_from_IMFSchemeHandler(iface);
    TRACE("%p, %p.\n", iface, cookie);
    return handler_cancel_object_creation(handler, cookie);
}

static const IMFSchemeHandlerVtbl scheme_handler_vtbl =
{
    scheme_handler_QueryInterface,
    scheme_handler_AddRef,
    scheme_handler_Release,
    scheme_handler_BeginCreateObject,
    scheme_handler_EndCreateObject,
    scheme_handler_CancelObjectCreation,
};

HRESULT winegstreamer_stream_handler_create(REFIID riid, void **obj)
{
    struct handler *handler;
    HRESULT hr;

    TRACE("%s, %p.\n", debugstr_guid(riid), obj);

    if (!(handler = calloc(1, sizeof(*handler))))
        return E_OUTOFMEMORY;

    handler->IMFAsyncCallback_iface.lpVtbl = &async_callback_vtbl;
    handler->IMFByteStreamHandler_iface.lpVtbl = &stream_handler_vtbl;
    handler->refcount = 1;
    list_init(&handler->results);
    InitializeCriticalSection(&handler->cs);

    hr = IMFByteStreamHandler_QueryInterface(&handler->IMFByteStreamHandler_iface, riid, obj);
    IMFAsyncCallback_Release(&handler->IMFAsyncCallback_iface);

    return hr;
}

HRESULT winegstreamer_scheme_handler_create(REFIID riid, void **obj)
{
    struct handler *handler;
    HRESULT hr;

    TRACE("%s, %p.\n", debugstr_guid(riid), obj);

    if (!(handler = calloc(1, sizeof(*handler))))
        return E_OUTOFMEMORY;

    handler->IMFAsyncCallback_iface.lpVtbl = &async_callback_vtbl;
    handler->IMFSchemeHandler_iface.lpVtbl = &scheme_handler_vtbl;
    handler->refcount = 1;
    list_init(&handler->results);
    InitializeCriticalSection(&handler->cs);

    hr = IMFSchemeHandler_QueryInterface(&handler->IMFSchemeHandler_iface, riid, obj);
    IMFAsyncCallback_Release(&handler->IMFAsyncCallback_iface);

    return hr;
}
