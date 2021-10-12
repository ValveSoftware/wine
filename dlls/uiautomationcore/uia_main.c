/*
 * Copyright 2017 Jacek Caban for CodeWeavers
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

#define COBJMACROS
#include <initguid.h>

#include "uia_private.h"
#include "ole2.h"
#include "rpcproxy.h"

#include "wine/debug.h"
#include "wine/heap.h"

WINE_DEFAULT_DEBUG_CHANNEL(uiautomation);

struct uia_object_wrapper
{
    IUnknown IUnknown_iface;
    LONG refcount;

    IUnknown *marshaler;
    IUnknown *marshal_object;
};

static struct uia_object_wrapper *impl_uia_object_wrapper_from_IUnknown(IUnknown *iface)
{
    return CONTAINING_RECORD(iface, struct uia_object_wrapper, IUnknown_iface);
}

static HRESULT WINAPI uia_object_wrapper_QueryInterface(IUnknown *iface,
        REFIID riid, void **ppv)
{
    struct uia_object_wrapper *wrapper = impl_uia_object_wrapper_from_IUnknown(iface);
    return IUnknown_QueryInterface(wrapper->marshal_object, riid, ppv);
}

static ULONG WINAPI uia_object_wrapper_AddRef(IUnknown *iface)
{
    struct uia_object_wrapper *wrapper = impl_uia_object_wrapper_from_IUnknown(iface);
    ULONG refcount = InterlockedIncrement(&wrapper->refcount);

    TRACE("%p, refcount %d\n", iface, refcount);

    return refcount;
}

static ULONG WINAPI uia_object_wrapper_Release(IUnknown *iface)
{
    struct uia_object_wrapper *wrapper = impl_uia_object_wrapper_from_IUnknown(iface);
    ULONG refcount = InterlockedDecrement(&wrapper->refcount);

    TRACE("%p, refcount %d\n", iface, refcount);
    if (!refcount)
    {
        IUnknown_Release(wrapper->marshaler);
        heap_free(wrapper);
    }

    return refcount;
}

static const IUnknownVtbl uia_object_wrapper_vtbl = {
    uia_object_wrapper_QueryInterface,
    uia_object_wrapper_AddRef,
    uia_object_wrapper_Release,
};

/*
 * When passing the ReservedNotSupportedValue/ReservedMixedAttributeValue
 * interface pointers across apartments within the same process, create a free
 * threaded marshaler so that the pointer value is preserved.
 */
static HRESULT create_uia_object_wrapper(IUnknown *reserved, void **ppv)
{
    struct uia_object_wrapper *wrapper;
    HRESULT hr;

    TRACE("%p, %p\n", reserved, ppv);

    wrapper = heap_alloc(sizeof(*wrapper));
    if (!wrapper)
        return E_OUTOFMEMORY;

    wrapper->IUnknown_iface.lpVtbl = &uia_object_wrapper_vtbl;
    wrapper->marshal_object = reserved;
    wrapper->refcount = 1;

    if (FAILED(hr = CoCreateFreeThreadedMarshaler(&wrapper->IUnknown_iface, &wrapper->marshaler)))
    {
        heap_free(wrapper);
        return hr;
    }

    hr = IUnknown_QueryInterface(wrapper->marshaler, &IID_IMarshal, ppv);
    IUnknown_Release(&wrapper->IUnknown_iface);

    return hr;
}

/*
 * UiaReservedNotSupportedValue/UiaReservedMixedAttributeValue object.
 */
static HRESULT WINAPI uia_reserved_obj_QueryInterface(IUnknown *iface,
        REFIID riid, void **ppv)
{
    *ppv = NULL;
    if (IsEqualIID(riid, &IID_IUnknown))
        *ppv = iface;
    else if (IsEqualIID(riid, &IID_IMarshal))
        return create_uia_object_wrapper(iface, ppv);
    else
        return E_NOINTERFACE;

    return S_OK;
}

static ULONG WINAPI uia_reserved_obj_AddRef(IUnknown *iface)
{
    return 1;
}

static ULONG WINAPI uia_reserved_obj_Release(IUnknown *iface)
{
    return 1;
}

static const IUnknownVtbl uia_reserved_obj_vtbl = {
    uia_reserved_obj_QueryInterface,
    uia_reserved_obj_AddRef,
    uia_reserved_obj_Release,
};

static IUnknown uia_reserved_ns_iface = {&uia_reserved_obj_vtbl};
static IUnknown uia_reserved_ma_iface = {&uia_reserved_obj_vtbl};

/*
 * UiaHostProviderFromHwnd IRawElementProviderSimple interface.
 */
struct hwnd_host_provider {
    IRawElementProviderSimple IRawElementProviderSimple_iface;
    LONG refcount;

    HWND hwnd;
};

static inline struct hwnd_host_provider *impl_from_hwnd_host_provider(IRawElementProviderSimple *iface)
{
    return CONTAINING_RECORD(iface, struct hwnd_host_provider, IRawElementProviderSimple_iface);
}

HRESULT WINAPI hwnd_host_provider_QueryInterface(IRawElementProviderSimple *iface, REFIID riid, void **ppv)
{
    *ppv = NULL;
    if (IsEqualIID(riid, &IID_IRawElementProviderSimple) || IsEqualIID(riid, &IID_IUnknown))
        *ppv = iface;
    else
        return E_NOINTERFACE;

    IRawElementProviderSimple_AddRef(iface);
    return S_OK;
}

ULONG WINAPI hwnd_host_provider_AddRef(IRawElementProviderSimple *iface)
{
    struct hwnd_host_provider *host_prov = impl_from_hwnd_host_provider(iface);
    ULONG refcount = InterlockedIncrement(&host_prov->refcount);

    TRACE("%p, refcount %d\n", iface, refcount);

    return refcount;
}

ULONG WINAPI hwnd_host_provider_Release(IRawElementProviderSimple *iface)
{
    struct hwnd_host_provider *host_prov = impl_from_hwnd_host_provider(iface);
    ULONG refcount = InterlockedDecrement(&host_prov->refcount);

    TRACE("%p, refcount %d\n", iface, refcount);

    if (!refcount)
        heap_free(host_prov);

    return refcount;
}

HRESULT WINAPI hwnd_host_provider_get_ProviderOptions(IRawElementProviderSimple *iface,
        enum ProviderOptions *ret_val)
{
    TRACE("%p, %p\n", iface, ret_val);
    *ret_val = ProviderOptions_ServerSideProvider;
    return S_OK;
}

HRESULT WINAPI hwnd_host_provider_GetPatternProvider(IRawElementProviderSimple *iface,
        PATTERNID pattern_id, IUnknown **ret_val)
{
    TRACE("%p, %d, %p\n", iface, pattern_id, ret_val);
    *ret_val = NULL;
    return S_OK;
}

HRESULT WINAPI hwnd_host_provider_GetPropertyValue(IRawElementProviderSimple *iface,
        PROPERTYID prop_id, VARIANT *ret_val)
{
    struct hwnd_host_provider *host_prov = impl_from_hwnd_host_provider(iface);

    TRACE("%p, %d, %p\n", iface, prop_id, ret_val);

    VariantInit(ret_val);
    switch (prop_id)
    {
    case UIA_NativeWindowHandlePropertyId:
        V_VT(ret_val) = VT_I4;
        V_I4(ret_val) = HandleToUlong(host_prov->hwnd);
        break;

    case UIA_ProviderDescriptionPropertyId:
        V_VT(ret_val) = VT_BSTR;
        V_BSTR(ret_val) = SysAllocString(L"Wine: HWND Provider Proxy");
        break;

    default:
        break;
    }

    return S_OK;
}

HRESULT WINAPI hwnd_host_provider_get_HostRawElementProvider(IRawElementProviderSimple *iface,
        IRawElementProviderSimple **ret_val)
{
    TRACE("%p, %p\n", iface, ret_val);
    *ret_val = NULL;
    return S_OK;
}

IRawElementProviderSimpleVtbl hwnd_host_provider_vtbl = {
    hwnd_host_provider_QueryInterface,
    hwnd_host_provider_AddRef,
    hwnd_host_provider_Release,
    hwnd_host_provider_get_ProviderOptions,
    hwnd_host_provider_GetPatternProvider,
    hwnd_host_provider_GetPropertyValue,
    hwnd_host_provider_get_HostRawElementProvider,
};

struct uia_provider_evlc
{
    struct list entry;

    IUIAEvlConnection *evlc_iface;
};

static struct list global_provider_evlc_list = LIST_INIT( global_provider_evlc_list );

/*
 * Check the current list of evlc's on the provider side to see if they are
 * still active. If not, remove them from the list.
 */
static void prune_listener_list(void)
{
    struct uia_provider_evlc *evlc;
    struct list *cursor, *cursor2;
    VARIANT var;
    HRESULT hr;

    LIST_FOR_EACH_SAFE(cursor, cursor2, &global_provider_evlc_list)
    {
        evlc = LIST_ENTRY(cursor, struct uia_provider_evlc, entry);
        hr = IUIAEvlConnection_CheckListenerStatus(evlc->evlc_iface, &var);
        if (hr == CO_E_OBJNOTCONNECTED)
        {
            list_remove(cursor);
            IUIAEvlConnection_Release(evlc->evlc_iface);
            heap_free(evlc);
        }
    }
}

/***********************************************************************
 *          UiaClientsAreListening (uiautomationcore.@)
 */
BOOL WINAPI UiaClientsAreListening(void)
{
    TRACE("()\n");

    prune_listener_list();
    if (list_empty(&global_provider_evlc_list))
        return FALSE;

    return TRUE;
}

/***********************************************************************
 *          UiaGetReservedMixedAttributeValue (uiautomationcore.@)
 */
HRESULT WINAPI UiaGetReservedMixedAttributeValue(IUnknown **value)
{
    TRACE("(%p)\n", value);

    if (!value)
        return E_INVALIDARG;

    *value = &uia_reserved_ma_iface;

    return S_OK;
}

/***********************************************************************
 *          UiaGetReservedNotSupportedValue (uiautomationcore.@)
 */
HRESULT WINAPI UiaGetReservedNotSupportedValue(IUnknown **value)
{
    TRACE("(%p)\n", value);

    if (!value)
        return E_INVALIDARG;

    *value = &uia_reserved_ns_iface;

    return S_OK;
}

/***********************************************************************
 *          UiaLookupId (uiautomationcore.@)
 */
int WINAPI UiaLookupId(enum AutomationIdentifierType type, const GUID *guid)
{
    FIXME("(%d, %s) stub!\n", type, debugstr_guid(guid));
    return 1;
}

/***********************************************************************
 *          UiaReturnRawElementProvider (uiautomationcore.@)
 */
LRESULT WINAPI UiaReturnRawElementProvider(HWND hwnd, WPARAM wParam,
        LPARAM lParam, IRawElementProviderSimple *elprov)
{
    TRACE("(%p, %lx, %lx, %p)\n", hwnd, wParam, lParam, elprov);

    if (lParam != UiaRootObjectId)
    {
        FIXME("Unsupported object id %ld\n", lParam);
        return 0;
    }

    /*
     * If a client send a WM_GETOBJECT message with a wParam value that isn't
     * 0, it's attempting to send an IUIAEvlConnection interface so that the
     * provider can signal events to the event listener.
     */
    if (wParam)
    {
        IUIAEvlConnection *evlc_iface;
        VARIANT var;
        HRESULT hr;

        TRACE("Client sent IUIAEvlConnection interface!\n");
        hr = ObjectFromLresult((LRESULT)wParam, &IID_IUIAEvlConnection, 0,
                (void **)&evlc_iface);
        hr = IUIAEvlConnection_CheckListenerStatus(evlc_iface, &var);
        if (SUCCEEDED(hr))
        {
            struct uia_provider_evlc *uia = heap_alloc_zero(sizeof(*uia));
            if (!uia)
                return 0;

            /* If success, add this to the providers listening clients list. */
            uia->evlc_iface = evlc_iface;
            list_add_tail(&global_provider_evlc_list, &uia->entry);
        }

        return 0;
    }

    return LresultFromObject(&IID_IRawElementProviderSimple, wParam, (IUnknown *)elprov);
}

/***********************************************************************
 *          UiaRaiseAutomationEvent (uiautomationcore.@)
 */
HRESULT WINAPI UiaRaiseAutomationEvent(IRawElementProviderSimple *provider, EVENTID id)
{
    struct uia_provider_evlc *evlc;
    struct list *cursor, *cursor2;
    HRESULT hr;

    TRACE("(%p, %d)\n", provider, id);

    LIST_FOR_EACH_SAFE(cursor, cursor2, &global_provider_evlc_list)
    {
        evlc = LIST_ENTRY(cursor, struct uia_provider_evlc, entry);
        hr = IUIAEvlConnection_ProviderRaiseEvent(evlc->evlc_iface, id, provider);
        TRACE("Event raised!\n");
        if (hr == CO_E_OBJNOTCONNECTED)
        {
            TRACE("Evlc no longer active, removing.\n");
            list_remove(cursor);
            IUIAEvlConnection_Release(evlc->evlc_iface);
            heap_free(evlc);
        }
    }

    return S_OK;
}

/***********************************************************************
 *          UiaRaiseAutomationPropertyChangedEvent (uiautomationcore.@)
 */
HRESULT WINAPI UiaRaiseAutomationPropertyChangedEvent(IRawElementProviderSimple *provider, PROPERTYID id, VARIANT old, VARIANT new)
{
    FIXME("(%p, %d, %s, %s): stub\n", provider, id, debugstr_variant(&old), debugstr_variant(&new));
    return S_OK;
}

/***********************************************************************
 *          UiaRaiseStructureChangedEvent (uiautomationcore.@)
 */
HRESULT WINAPI UiaRaiseStructureChangedEvent(IRawElementProviderSimple *provider, enum StructureChangeType struct_change_type,
                                             int *runtime_id, int runtime_id_len)
{
    FIXME("(%p, %d, %p, %d): stub\n", provider, struct_change_type, runtime_id, runtime_id_len);
    return S_OK;
}

/***********************************************************************
 *          UiaRaiseAsyncContentLoadedEvent (uiautomationcore.@)
 */
HRESULT WINAPI UiaRaiseAsyncContentLoadedEvent(IRawElementProviderSimple *provider,
                                                enum AsyncContentLoadedState async_content_loaded_state,
                                                double percent_complete)
{
    FIXME("(%p, %d, %f): stub\n", provider, async_content_loaded_state, percent_complete);
    return S_OK;
}

/***********************************************************************
 *          UiaRaiseTextEditTextChangedEvent (uiautomationcore.@)
 */
HRESULT WINAPI UiaRaiseTextEditTextChangedEvent(IRawElementProviderSimple *provider,
        enum TextEditChangeType text_edit_change_type, SAFEARRAY *changed_data)
{
    FIXME("(%p, %d, %p): stub\n", provider, text_edit_change_type, changed_data);
    return S_OK;
}

void WINAPI UiaRegisterProviderCallback(UiaProviderCallback *callback)
{
    FIXME("(%p): stub\n", callback);
}

HRESULT WINAPI UiaHostProviderFromHwnd(HWND hwnd, IRawElementProviderSimple **provider)
{
    struct hwnd_host_provider *host_prov;

    TRACE("(%p, %p)\n", hwnd, provider);

    if (provider)
        *provider = NULL;

    if (!IsWindow(hwnd) || !provider)
        return E_INVALIDARG;

    host_prov = heap_alloc(sizeof(*host_prov));
    if (!host_prov)
        return E_OUTOFMEMORY;

    host_prov->IRawElementProviderSimple_iface.lpVtbl = &hwnd_host_provider_vtbl;
    host_prov->refcount = 1;
    host_prov->hwnd = hwnd;
    *provider = &host_prov->IRawElementProviderSimple_iface;

    return S_OK;
}

HRESULT WINAPI UiaDisconnectProvider(IRawElementProviderSimple *provider)
{
    FIXME("(%p): stub\n", provider);
    return E_NOTIMPL;
}

/* UIAutomation ClassFactory */
struct uia_cf {
    IClassFactory IClassFactory_iface;
    LONG ref;
};

static struct uia_cf *impl_from_IClassFactory(IClassFactory *iface)
{
    return CONTAINING_RECORD(iface, struct uia_cf, IClassFactory_iface);
}

static HRESULT WINAPI uia_cf_QueryInterface(IClassFactory *iface, REFIID riid, void **ppobj)
{
    if(IsEqualGUID(riid, &IID_IUnknown)
            || IsEqualGUID(riid, &IID_IClassFactory))
    {
        IClassFactory_AddRef(iface);
        *ppobj = iface;
        return S_OK;
    }

    *ppobj = NULL;
    WARN("(%p)->(%s, %p): interface not found\n", iface, debugstr_guid(riid), ppobj);
    return E_NOINTERFACE;
}

static ULONG WINAPI uia_cf_AddRef(IClassFactory *iface)
{
    struct uia_cf *This = impl_from_IClassFactory(iface);
    ULONG ref = InterlockedIncrement(&This->ref);
    TRACE("(%p)->(): Refcount now %u\n", This, ref);
    return ref;
}

static ULONG WINAPI uia_cf_Release(IClassFactory *iface)
{
    struct uia_cf *This = impl_from_IClassFactory(iface);
    ULONG ref = InterlockedDecrement(&This->ref);
    TRACE("(%p)->(): Refcount now %u\n", This, ref);
    if (!ref)
        HeapFree(GetProcessHeap(), 0, This);
    return ref;
}

static HRESULT WINAPI uia_cf_CreateInstance(IClassFactory *iface, IUnknown *pOuter,
                                               REFIID riid, void **ppobj)
{
    struct uia_cf *This = impl_from_IClassFactory(iface);

    TRACE("(%p)->(%p,%s,%p)\n", This, pOuter, debugstr_guid(riid), ppobj);

    *ppobj = NULL;

    if(pOuter)
        return CLASS_E_NOAGGREGATION;

    if (!IsEqualGUID(riid, &IID_IUIAutomation))
        return E_NOINTERFACE;

    return create_uia_iface((IUIAutomation **)ppobj);
}

static HRESULT WINAPI uia_cf_LockServer(IClassFactory *iface, BOOL dolock)
{
    struct uia_cf *This = impl_from_IClassFactory(iface);
    FIXME("(%p)->(%d): stub!\n", This, dolock);
    return S_OK;
}

static const IClassFactoryVtbl uia_cf_Vtbl =
{
    uia_cf_QueryInterface,
    uia_cf_AddRef,
    uia_cf_Release,
    uia_cf_CreateInstance,
    uia_cf_LockServer
};

static inline HRESULT make_uia_factory(REFIID riid, void **ppv)
{
    HRESULT hr;
    struct uia_cf *ret = HeapAlloc(GetProcessHeap(), 0, sizeof(*ret));
    ret->IClassFactory_iface.lpVtbl = &uia_cf_Vtbl;
    ret->ref = 0;

    hr = IClassFactory_QueryInterface(&ret->IClassFactory_iface, riid, ppv);
    if(FAILED(hr))
        HeapFree(GetProcessHeap(), 0, ret);

    return hr;
}

HRESULT WINAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void **ppv)
{
    TRACE("(%s, %s, %p)\n", debugstr_guid(rclsid), debugstr_guid(riid), ppv);

    if (IsEqualGUID(rclsid, &CLSID_CUIAutomation))
        return make_uia_factory(riid, ppv);

    return CLASS_E_CLASSNOTAVAILABLE;
}

/******************************************************************
 *              DllCanUnloadNow (uiautomationcore.@)
 */
HRESULT WINAPI DllCanUnloadNow(void)
{
    return S_FALSE;
}

/***********************************************************************
 *          DllRegisterServer (uiautomationcore.@)
 */
HRESULT WINAPI DllRegisterServer(void)
{
    return __wine_register_resources();
}

/***********************************************************************
 *          DllUnregisterServer (uiautomationcore.@)
 */
HRESULT WINAPI DllUnregisterServer(void)
{
    return __wine_unregister_resources();
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, void *reserved)
{
    TRACE("%p,%u,%p\n", hinst, reason, reserved);

    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hinst);
        break;
    }

    return TRUE;
}
