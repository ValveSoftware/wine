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

struct async_create_object
{
    IUnknown IUnknown_iface;
    LONG refcount;

    IPropertyStore *props;
    IMFByteStream *stream;
    WCHAR *url;
    DWORD flags;
};

static struct async_create_object *impl_from_IUnknown(IUnknown *iface)
{
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
        if (async->props)
            IPropertyStore_Release(async->props);
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

struct handler
{
    IMFByteStreamHandler IMFByteStreamHandler_iface;
    IMFAsyncCallback IMFAsyncCallback_iface;
    LONG refcount;
    struct list results;
    CRITICAL_SECTION cs;
};

static struct handler *impl_from_IMFAsyncCallback(IMFAsyncCallback *iface)
{
    return CONTAINING_RECORD(iface, struct handler, IMFAsyncCallback_iface);
}

static struct handler *impl_from_IMFByteStreamHandler(IMFByteStreamHandler *iface)
{
    return CONTAINING_RECORD(iface, struct handler, IMFByteStreamHandler_iface);
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
    return IMFByteStreamHandler_AddRef(&handler->IMFByteStreamHandler_iface);
}

static ULONG WINAPI async_callback_Release(IMFAsyncCallback *iface)
{
    struct handler *handler = impl_from_IMFAsyncCallback(iface);
    return IMFByteStreamHandler_Release(&handler->IMFByteStreamHandler_iface);
}

static HRESULT WINAPI async_callback_GetParameters(IMFAsyncCallback *iface, DWORD *flags, DWORD *queue)
{
    return E_NOTIMPL;
}

static HRESULT stream_handler_create_object(struct handler *handler, WCHAR *url,
        IMFByteStream *stream, DWORD flags, IPropertyStore *props, IUnknown **object, MF_OBJECT_TYPE *type)
{
    TRACE("%p, %s, %p, %#lx, %p, %p, %p.\n", handler, debugstr_w(url), stream, flags, props, object, type);

    if (flags & MF_RESOLUTION_MEDIASOURCE)
    {
        HRESULT hr;

        if (FAILED(hr = media_source_create(stream, NULL, (IMFMediaSource **)object)))
            return hr;

        *type = MF_OBJECT_MEDIASOURCE;
        return S_OK;
    }
    else
    {
        FIXME("Unhandled flags %#lx.\n", flags);
        return E_NOTIMPL;
    }
}

static HRESULT WINAPI async_callback_Invoke(IMFAsyncCallback *iface, IMFAsyncResult *result)
{
    struct handler *handler = impl_from_IMFAsyncCallback(iface);
    MF_OBJECT_TYPE type = MF_OBJECT_INVALID;
    IUnknown *object = NULL, *context_object;
    struct async_create_object *async;
    struct result_entry *entry;
    IMFAsyncResult *caller;
    HRESULT hr;

    caller = (IMFAsyncResult *)IMFAsyncResult_GetStateNoAddRef(result);

    if (FAILED(hr = IMFAsyncResult_GetObject(result, &context_object)))
    {
        WARN("Expected context set for callee result.\n");
        return hr;
    }

    async = impl_from_IUnknown(context_object);

    hr = stream_handler_create_object(handler, async->url, async->stream, async->flags,
            async->props, &object, &type);

    if ((entry = malloc(sizeof(*entry))))
    {
        entry->result = caller;
        IMFAsyncResult_AddRef(entry->result);
        entry->type = type;
        entry->object = object;

        EnterCriticalSection(&handler->cs);
        list_add_tail(&handler->results, &entry->entry);
        LeaveCriticalSection(&handler->cs);
    }
    else
    {
        if (object)
            IUnknown_Release(object);
        hr = E_OUTOFMEMORY;
    }

    IUnknown_Release(&async->IUnknown_iface);

    IMFAsyncResult_SetStatus(caller, hr);
    MFInvokeCallback(caller);

    return S_OK;
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
    ULONG refcount = InterlockedIncrement(&handler->refcount);
    TRACE("%p, refcount %lu.\n", handler, refcount);
    return refcount;
}

static ULONG WINAPI stream_handler_Release(IMFByteStreamHandler *iface)
{
    struct handler *handler = impl_from_IMFByteStreamHandler(iface);
    ULONG refcount = InterlockedDecrement(&handler->refcount);
    struct result_entry *entry, *next;

    TRACE("%p, refcount %lu.\n", iface, refcount);

    if (!refcount)
    {
        LIST_FOR_EACH_ENTRY_SAFE(entry, next, &handler->results, struct result_entry, entry)
        {
            list_remove(&entry->entry);
            IMFAsyncResult_Release(entry->result);
            if (entry->object)
                IUnknown_Release(entry->object);
            free(entry);
        }
        DeleteCriticalSection(&handler->cs);
        free(handler);
    }

    return refcount;
}

static HRESULT WINAPI stream_handler_BeginCreateObject(IMFByteStreamHandler *iface,
        IMFByteStream *stream, const WCHAR *url, DWORD flags, IPropertyStore *props,
        IUnknown **cookie, IMFAsyncCallback *callback, IUnknown *state)
{
    struct handler *handler = impl_from_IMFByteStreamHandler(iface);
    struct async_create_object *async;
    IMFAsyncResult *caller, *item;
    HRESULT hr;

    TRACE("%p, %s, %#lx, %p, %p, %p, %p.\n", iface, debugstr_w(url), flags, props, cookie, callback, state);

    if (cookie)
        *cookie = NULL;

    if (FAILED(hr = MFCreateAsyncResult(NULL, callback, state, &caller)))
        return hr;

    if (!(async = calloc(1, sizeof(*async))))
    {
        IMFAsyncResult_Release(caller);
        return E_OUTOFMEMORY;
    }

    async->IUnknown_iface.lpVtbl = &async_create_object_vtbl;
    async->refcount = 1;
    async->props = props;
    if (async->props)
        IPropertyStore_AddRef(async->props);
    async->flags = flags;
    async->stream = stream;
    if (async->stream)
        IMFByteStream_AddRef(async->stream);
    if (url)
        async->url = wcsdup(url);
    if (!async->stream)
    {
        IMFAsyncResult_Release(caller);
        IUnknown_Release(&async->IUnknown_iface);
        return E_OUTOFMEMORY;
    }

    hr = MFCreateAsyncResult(&async->IUnknown_iface, &handler->IMFAsyncCallback_iface,
            (IUnknown *)caller, &item);
    IUnknown_Release(&async->IUnknown_iface);
    if (SUCCEEDED(hr))
    {
        if (SUCCEEDED(hr = MFPutWorkItemEx(MFASYNC_CALLBACK_QUEUE_IO, item)))
        {
            if (cookie)
            {
                *cookie = (IUnknown *)caller;
                IUnknown_AddRef(*cookie);
            }
        }

        IMFAsyncResult_Release(item);
    }
    IMFAsyncResult_Release(caller);

    return hr;
}

static HRESULT WINAPI stream_handler_EndCreateObject(IMFByteStreamHandler *iface,
        IMFAsyncResult *result, MF_OBJECT_TYPE *type, IUnknown **object)
{
    struct handler *handler = impl_from_IMFByteStreamHandler(iface);
    struct result_entry *found = NULL, *entry;
    HRESULT hr;

    TRACE("%p, %p, %p, %p.\n", iface, result, type, object);

    EnterCriticalSection(&handler->cs);

    LIST_FOR_EACH_ENTRY(entry, &handler->results, struct result_entry, entry)
    {
        if (result == entry->result)
        {
            list_remove(&entry->entry);
            found = entry;
            break;
        }
    }

    LeaveCriticalSection(&handler->cs);

    if (found)
    {
        *type = found->type;
        *object = found->object;
        hr = IMFAsyncResult_GetStatus(found->result);
        IMFAsyncResult_Release(found->result);
        free(found);
    }
    else
    {
        *type = MF_OBJECT_INVALID;
        *object = NULL;
        hr = MF_E_UNEXPECTED;
    }

    return hr;
}

static HRESULT WINAPI stream_handler_CancelObjectCreation(IMFByteStreamHandler *iface, IUnknown *cookie)
{
    struct handler *handler = impl_from_IMFByteStreamHandler(iface);
    struct result_entry *found = NULL, *entry;

    TRACE("%p, %p.\n", iface, cookie);

    EnterCriticalSection(&handler->cs);

    LIST_FOR_EACH_ENTRY(entry, &handler->results, struct result_entry, entry)
    {
        if (cookie == (IUnknown *)entry->result)
        {
            list_remove(&entry->entry);
            found = entry;
            break;
        }
    }

    LeaveCriticalSection(&handler->cs);

    if (found)
    {
        IMFAsyncResult_Release(found->result);
        if (found->object)
            IUnknown_Release(found->object);
        free(found);
    }

    return found ? S_OK : MF_E_UNEXPECTED;
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

HRESULT winegstreamer_stream_handler_create(REFIID riid, void **obj)
{
    struct handler *handler;
    HRESULT hr;

    TRACE("%s, %p.\n", debugstr_guid(riid), obj);

    if (!(handler = calloc(1, sizeof(*handler))))
        return E_OUTOFMEMORY;

    list_init(&handler->results);
    InitializeCriticalSection(&handler->cs);

    handler->IMFByteStreamHandler_iface.lpVtbl = &stream_handler_vtbl;
    handler->IMFAsyncCallback_iface.lpVtbl = &async_callback_vtbl;
    handler->refcount = 1;

    hr = IMFByteStreamHandler_QueryInterface(&handler->IMFByteStreamHandler_iface, riid, obj);
    IMFByteStreamHandler_Release(&handler->IMFByteStreamHandler_iface);

    return hr;
}
