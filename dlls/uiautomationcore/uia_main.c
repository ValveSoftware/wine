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

HINSTANCE uia_instance;

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
    FIXME("(%p) stub!\n", value);
    *value = NULL;
    return S_OK;
}

/***********************************************************************
 *          UiaGetReservedNotSupportedValue (uiautomationcore.@)
 */
HRESULT WINAPI UiaGetReservedNotSupportedValue(IUnknown **value)
{
    FIXME("(%p) stub!\n", value);
    *value = NULL;
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
 *          UiaRaiseStructureChangedEvent (uiautomationcore.@)
 */
HRESULT WINAPI UiaRaiseStructureChangedEvent(IRawElementProviderSimple *provider, enum StructureChangeType structureChangeType,
                                             int *pRuntimeId, int cRuntimeIdLen)
{
    FIXME("(%p, %d, %p, %d): stub\n", provider, structureChangeType, pRuntimeId, cRuntimeIdLen);
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

/***********************************************************************
 *          UiaRaiseNotificationEvent (uiautomationcore.@)
 */
HRESULT WINAPI UiaRaiseNotificationEvent(IRawElementProviderSimple *provider,
        enum NotificationKind notification_kind, enum NotificationProcessing notification_processing,
        BSTR display_str, BSTR activity_id)
{
    FIXME("(%p, %d, %d, %s, %s): stub\n", provider, notification_kind, notification_processing,
            debugstr_w(display_str), debugstr_w(activity_id));
    return S_OK;
}

void WINAPI UiaRegisterProviderCallback(UiaProviderCallback *callback)
{
    FIXME("(%p): stub\n", callback);
}

HRESULT WINAPI UiaHostProviderFromHwnd(HWND hwnd, IRawElementProviderSimple **provider)
{
    FIXME("(%p, %p): stub\n", hwnd, provider);
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
    return __wine_register_resources( uia_instance );
}

/***********************************************************************
 *          DllUnregisterServer (uiautomationcore.@)
 */
HRESULT WINAPI DllUnregisterServer(void)
{
    return __wine_unregister_resources( uia_instance );
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, void *reserved)
{
    TRACE("%p,%u,%p\n", hinst, reason, reserved);

    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        uia_instance = hinst;
        DisableThreadLibraryCalls(uia_instance);
        break;
    }

    return TRUE;
}
