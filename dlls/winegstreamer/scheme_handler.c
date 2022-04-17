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

struct gstreamer_scheme_handler_result
{
    struct list entry;
    IMFAsyncResult *result;
    IUnknown *object;
};

struct gstreamer_scheme_handler
{
    IMFSchemeHandler IMFSchemeHandler_iface;
    IMFAsyncCallback IMFAsyncCallback_iface;
    LONG refcount;
    struct list results;
    CRITICAL_SECTION cs;
};

static struct gstreamer_scheme_handler *impl_from_IMFSchemeHandler(IMFSchemeHandler *iface)
{
    return CONTAINING_RECORD(iface, struct gstreamer_scheme_handler, IMFSchemeHandler_iface);
}

static struct gstreamer_scheme_handler *impl_from_IMFAsyncCallback(IMFAsyncCallback *iface)
{
    return CONTAINING_RECORD(iface, struct gstreamer_scheme_handler, IMFAsyncCallback_iface);
}

static HRESULT WINAPI gstreamer_scheme_handler_QueryIntace(IMFSchemeHandler *iface, REFIID riid, void **obj)
{
    TRACE("%p, %s, %p.\n", iface, debugstr_guid(riid), obj);

    if (IsEqualIID(riid, &IID_IMFSchemeHandler) ||
            IsEqualIID(riid, &IID_IUnknown))
    {
        *obj = iface;
        IMFSchemeHandler_AddRef(iface);
        return S_OK;
    }

    WARN("Unsupported %s.\n", debugstr_guid(riid));
    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI gstreamer_scheme_handler_AddRef(IMFSchemeHandler *iface)
{
    struct gstreamer_scheme_handler *handler = impl_from_IMFSchemeHandler(iface);
    ULONG refcount = InterlockedIncrement(&handler->refcount);

    TRACE("%p, refcount %lu.\n", handler, refcount);

    return refcount;
}

static ULONG WINAPI gstreamer_scheme_handler_Release(IMFSchemeHandler *iface)
{
    struct gstreamer_scheme_handler *handler = impl_from_IMFSchemeHandler(iface);
    ULONG refcount = InterlockedDecrement(&handler->refcount);
    struct gstreamer_scheme_handler_result *result, *next;

    TRACE("%p, refcount %lu.\n", iface, refcount);

    if (!refcount)
    {
        LIST_FOR_EACH_ENTRY_SAFE(result, next, &handler->results, struct gstreamer_scheme_handler_result, entry)
        {
            list_remove(&result->entry);
            IMFAsyncResult_Release(result->result);
            if (result->object)
                IUnknown_Release(result->object);
            free(result);
        }
        DeleteCriticalSection(&handler->cs);
        free(handler);
    }

    return refcount;
}

struct create_object_context
{
    IUnknown IUnknown_iface;
    LONG refcount;

    IPropertyStore *props;
    WCHAR *url;
    DWORD flags;
};

static struct create_object_context *impl_from_IUnknown(IUnknown *iface)
{
    return CONTAINING_RECORD(iface, struct create_object_context, IUnknown_iface);
}

static HRESULT WINAPI create_object_context_QueryInterface(IUnknown *iface, REFIID riid, void **obj)
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

static ULONG WINAPI create_object_context_AddRef(IUnknown *iface)
{
    struct create_object_context *context = impl_from_IUnknown(iface);
    ULONG refcount = InterlockedIncrement(&context->refcount);

    TRACE("%p, refcount %lu.\n", iface, refcount);

    return refcount;
}

static ULONG WINAPI create_object_context_Release(IUnknown *iface)
{
    struct create_object_context *context = impl_from_IUnknown(iface);
    ULONG refcount = InterlockedDecrement(&context->refcount);

    TRACE("%p, refcount %lu.\n", iface, refcount);

    if (!refcount)
    {
        if (context->props)
            IPropertyStore_Release(context->props);
        free(context->url);
        free(context);
    }

    return refcount;
}

static const IUnknownVtbl create_object_context_vtbl =
{
    create_object_context_QueryInterface,
    create_object_context_AddRef,
    create_object_context_Release,
};

static HRESULT WINAPI gstreamer_scheme_handler_BeginCreateObject(IMFSchemeHandler *iface, const WCHAR *url, DWORD flags,
        IPropertyStore *props, IUnknown **cancel_cookie, IMFAsyncCallback *callback, IUnknown *state)
{
    struct gstreamer_scheme_handler *handler = impl_from_IMFSchemeHandler(iface);
    struct create_object_context *context;
    IMFAsyncResult *caller, *item;
    HRESULT hr;

    TRACE("%p, %s, %#lx, %p, %p, %p, %p.\n", iface, debugstr_w(url), flags, props, cancel_cookie, callback, state);

    if (cancel_cookie)
        *cancel_cookie = NULL;

    if (FAILED(hr = MFCreateAsyncResult(NULL, callback, state, &caller)))
        return hr;

    if (!(context = malloc(sizeof(*context))))
    {
        IMFAsyncResult_Release(caller);
        return E_OUTOFMEMORY;
    }

    context->IUnknown_iface.lpVtbl = &create_object_context_vtbl;
    context->refcount = 1;
    context->props = props;
    if (context->props)
        IPropertyStore_AddRef(context->props);
    context->flags = flags;
    context->url = wcsdup(url);
    if (!context->url)
    {
        IMFAsyncResult_Release(caller);
        IUnknown_Release(&context->IUnknown_iface);
        return E_OUTOFMEMORY;
    }

    hr = MFCreateAsyncResult(&context->IUnknown_iface, &handler->IMFAsyncCallback_iface, (IUnknown *)caller, &item);
    IUnknown_Release(&context->IUnknown_iface);
    if (SUCCEEDED(hr))
    {
        if (SUCCEEDED(hr = MFPutWorkItemEx(MFASYNC_CALLBACK_QUEUE_IO, item)))
        {
            if (cancel_cookie)
            {
                *cancel_cookie = (IUnknown *)caller;
                IUnknown_AddRef(*cancel_cookie);
            }
        }

        IMFAsyncResult_Release(item);
    }
    IMFAsyncResult_Release(caller);

    return hr;
}

static HRESULT WINAPI gstreamer_scheme_handler_EndCreateObject(IMFSchemeHandler *iface, IMFAsyncResult *result,
        MF_OBJECT_TYPE *obj_type, IUnknown **object)
{
    struct gstreamer_scheme_handler *handler = impl_from_IMFSchemeHandler(iface);
    struct gstreamer_scheme_handler_result *found = NULL, *cur;
    HRESULT hr;

    TRACE("%p, %p, %p, %p.\n", iface, result, obj_type, object);

    EnterCriticalSection(&handler->cs);

    LIST_FOR_EACH_ENTRY(cur, &handler->results, struct gstreamer_scheme_handler_result, entry)
    {
        if (result == cur->result)
        {
            list_remove(&cur->entry);
            found = cur;
            break;
        }
    }

    LeaveCriticalSection(&handler->cs);

    if (found)
    {
        *obj_type = MF_OBJECT_MEDIASOURCE;
        *object = found->object;
        hr = IMFAsyncResult_GetStatus(found->result);
        IMFAsyncResult_Release(found->result);
        free(found);
    }
    else
    {
        *obj_type = MF_OBJECT_INVALID;
        *object = NULL;
        hr = MF_E_UNEXPECTED;
    }

    return hr;
}

static HRESULT WINAPI gstreamer_scheme_handler_CancelObjectCreation(IMFSchemeHandler *iface, IUnknown *cancel_cookie)
{
    struct gstreamer_scheme_handler *handler = impl_from_IMFSchemeHandler(iface);
    struct gstreamer_scheme_handler_result *found = NULL, *cur;

    TRACE("%p, %p.\n", iface, cancel_cookie);

    EnterCriticalSection(&handler->cs);

    LIST_FOR_EACH_ENTRY(cur, &handler->results, struct gstreamer_scheme_handler_result, entry)
    {
        if (cancel_cookie == (IUnknown *)cur->result)
        {
            list_remove(&cur->entry);
            found = cur;
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

static const IMFSchemeHandlerVtbl gstreamer_scheme_handler_vtbl =
{
    gstreamer_scheme_handler_QueryIntace,
    gstreamer_scheme_handler_AddRef,
    gstreamer_scheme_handler_Release,
    gstreamer_scheme_handler_BeginCreateObject,
    gstreamer_scheme_handler_EndCreateObject,
    gstreamer_scheme_handler_CancelObjectCreation,
};

static HRESULT WINAPI gstreamer_scheme_handler_callback_QueryIntace(IMFAsyncCallback *iface, REFIID riid, void **obj)
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

static ULONG WINAPI gstreamer_scheme_handler_callback_AddRef(IMFAsyncCallback *iface)
{
    struct gstreamer_scheme_handler *handler = impl_from_IMFAsyncCallback(iface);
    return IMFSchemeHandler_AddRef(&handler->IMFSchemeHandler_iface);
}

static ULONG WINAPI gstreamer_scheme_handler_callback_Release(IMFAsyncCallback *iface)
{
    struct gstreamer_scheme_handler *handler = impl_from_IMFAsyncCallback(iface);
    return IMFSchemeHandler_Release(&handler->IMFSchemeHandler_iface);
}

static HRESULT WINAPI gstreamer_scheme_handler_callback_GetParameters(IMFAsyncCallback *iface, DWORD *flags, DWORD *queue)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI gstreamer_scheme_handler_callback_Invoke(IMFAsyncCallback *iface, IMFAsyncResult *result)
{
    IMFAsyncResult *caller;
    struct gstreamer_scheme_handler *handler = impl_from_IMFAsyncCallback(iface);
    struct gstreamer_scheme_handler_result *handler_result;
    IUnknown *object = NULL, *context_object;
    struct create_object_context *context;
    HRESULT hr;

    caller = (IMFAsyncResult *)IMFAsyncResult_GetStateNoAddRef(result);

    if (FAILED(hr = IMFAsyncResult_GetObject(result, &context_object)))
    {
        WARN("Expected context set for callee result.\n");
        return hr;
    }

    context = impl_from_IUnknown(context_object);

    hr = winegstreamer_create_media_source_from_uri(context->url, &object);

    handler_result = malloc(sizeof(*handler_result));
    if (handler_result)
    {
        handler_result->result = caller;
        IMFAsyncResult_AddRef(handler_result->result);

        handler_result->object = object;

        EnterCriticalSection(&handler->cs);
        list_add_tail(&handler->results, &handler_result->entry);
        LeaveCriticalSection(&handler->cs);
    }
    else
    {
        if (object)
            IUnknown_Release(object);
        hr = E_OUTOFMEMORY;
    }

    IMFAsyncResult_SetStatus(caller, hr);
    MFInvokeCallback(caller);

    return S_OK;
}

static const IMFAsyncCallbackVtbl gstreamer_scheme_handler_callback_vtbl =
{
    gstreamer_scheme_handler_callback_QueryIntace,
    gstreamer_scheme_handler_callback_AddRef,
    gstreamer_scheme_handler_callback_Release,
    gstreamer_scheme_handler_callback_GetParameters,
    gstreamer_scheme_handler_callback_Invoke,
};

HRESULT gstreamer_scheme_handler_construct(REFIID riid, void **obj)
{
    struct gstreamer_scheme_handler *handler;
    HRESULT hr;

    TRACE("%s, %p.\n", debugstr_guid(riid), obj);

    if (!(handler = calloc(1, sizeof(*handler))))
        return E_OUTOFMEMORY;

    handler->IMFSchemeHandler_iface.lpVtbl = &gstreamer_scheme_handler_vtbl;
    handler->IMFAsyncCallback_iface.lpVtbl = &gstreamer_scheme_handler_callback_vtbl;
    handler->refcount = 1;
    list_init(&handler->results);
    InitializeCriticalSection(&handler->cs);

    hr = IMFSchemeHandler_QueryInterface(&handler->IMFSchemeHandler_iface, riid, obj);
    IMFSchemeHandler_Release(&handler->IMFSchemeHandler_iface);

    return hr;
}

