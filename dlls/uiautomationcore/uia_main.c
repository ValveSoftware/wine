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

WINE_DEFAULT_DEBUG_CHANNEL(uiautomation);

HINSTANCE uia_instance;

/***********************************************************************
 *          UiaClientsAreListening (uiautomationcore.@)
 */
BOOL WINAPI UiaClientsAreListening(void)
{
    FIXME("()\n");
    return FALSE;
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
    FIXME("(%p, %lx, %lx, %p) stub!\n", hwnd, wParam, lParam, elprov);
    return 0;
}

/***********************************************************************
 *          UiaRaiseAutomationEvent (uiautomationcore.@)
 */
HRESULT WINAPI UiaRaiseAutomationEvent(IRawElementProviderSimple *provider, EVENTID id)
{
    FIXME("(%p, %d): stub\n", provider, id);
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
