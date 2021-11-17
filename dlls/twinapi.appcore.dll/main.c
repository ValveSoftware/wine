/*
 * Copyright 2021 Paul Gofman for CodeWeavers
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
#include "initguid.h"
#include "windef.h"
#include "winbase.h"
#include "winstring.h"
#include "wine/debug.h"
#include "objbase.h"

#include "activation.h"

#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_Foundation_Collections
#include "windows.foundation.h"
#define WIDL_using_Windows_Security_ExchangeActiveSyncProvisioning
#include "windows.security.exchangeactivesyncprovisioning.h"

WINE_DEFAULT_DEBUG_CHANNEL(twinapi);

static const char *debugstr_hstring(HSTRING hstr)
{
    const WCHAR *str;
    UINT32 len;
    if (hstr && !((ULONG_PTR)hstr >> 16)) return "(invalid)";
    str = WindowsGetStringRawBuffer(hstr, &len);
    return wine_dbgstr_wn(str, len);
}

struct twinapi_appcore
{
    IActivationFactory IActivationFactory_iface;
    IEasClientDeviceInformation IEasClientDeviceInformation_iface;
    LONG ref;
};

static inline struct twinapi_appcore *impl_from_IEasClientDeviceInformation(IEasClientDeviceInformation *iface)
{
    return CONTAINING_RECORD(iface, struct twinapi_appcore, IEasClientDeviceInformation_iface);
}

static inline struct twinapi_appcore *impl_from_IActivationFactory(IActivationFactory *iface)
{
    return CONTAINING_RECORD(iface, struct twinapi_appcore, IActivationFactory_iface);
}

static HRESULT STDMETHODCALLTYPE twinapi_appcore_QueryInterface(
        IActivationFactory *iface, REFIID iid, void **out)
{
    struct twinapi_appcore *impl = impl_from_IActivationFactory(iface);

    TRACE("iface %p, iid %s, out %p stub!\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IUnknown) ||
            IsEqualGUID(iid, &IID_IInspectable) ||
            IsEqualGUID(iid, &IID_IAgileObject) ||
            IsEqualGUID(iid, &IID_IActivationFactory))
    {
        IUnknown_AddRef(iface);
        *out = iface;
        return S_OK;
    }

    if (IsEqualGUID(iid, &IID_IEasClientDeviceInformation))
    {
        IUnknown_AddRef(iface);
        *out = &impl->IEasClientDeviceInformation_iface;
        return S_OK;
    }

    FIXME("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE twinapi_appcore_AddRef(
        IActivationFactory *iface)
{
    struct twinapi_appcore *impl = impl_from_IActivationFactory(iface);
    ULONG ref = InterlockedIncrement(&impl->ref);
    TRACE("iface %p, ref %u.\n", iface, ref);
    return ref;
}

static ULONG STDMETHODCALLTYPE twinapi_appcore_Release(
        IActivationFactory *iface)
{
    struct twinapi_appcore *impl = impl_from_IActivationFactory(iface);
    ULONG ref = InterlockedDecrement(&impl->ref);
    TRACE("iface %p, ref %u.\n", iface, ref);
    return ref;
}

static HRESULT STDMETHODCALLTYPE twinapi_appcore_GetIids(
        IActivationFactory *iface, ULONG *iid_count, IID **iids)
{
    FIXME("iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE twinapi_appcore_GetRuntimeClassName(
        IActivationFactory *iface, HSTRING *class_name)
{
    FIXME("iface %p, class_name %p stub!\n", iface, class_name);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE twinapi_appcore_GetTrustLevel(
        IActivationFactory *iface, TrustLevel *trust_level)
{
    FIXME("iface %p, trust_level %p stub!\n", iface, trust_level);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE twinapi_appcore_ActivateInstance(
        IActivationFactory *iface, IInspectable **instance)
{
    FIXME("iface %p, instance %p semi-stub!\n", iface, instance);

    IActivationFactory_AddRef(iface);
    *instance = (IInspectable *)iface;

    return S_OK;
}

static const struct IActivationFactoryVtbl activation_factory_vtbl =
{
    twinapi_appcore_QueryInterface,
    twinapi_appcore_AddRef,
    twinapi_appcore_Release,
    /* IInspectable methods */
    twinapi_appcore_GetIids,
    twinapi_appcore_GetRuntimeClassName,
    twinapi_appcore_GetTrustLevel,
    /* IActivationFactory methods */
    twinapi_appcore_ActivateInstance,
};

static HRESULT WINAPI eas_client_devinfo_QueryInterface(IEasClientDeviceInformation *iface,
        REFIID riid, void **ppvObject)
{
    struct twinapi_appcore *This = impl_from_IEasClientDeviceInformation(iface);
    return twinapi_appcore_QueryInterface(&This->IActivationFactory_iface, riid, ppvObject);
}

static ULONG WINAPI eas_client_devinfo_AddRef(IEasClientDeviceInformation *iface)
{
    struct twinapi_appcore *This = impl_from_IEasClientDeviceInformation(iface);
    return twinapi_appcore_AddRef(&This->IActivationFactory_iface);
}

static ULONG WINAPI eas_client_devinfo_Release(IEasClientDeviceInformation *iface)
{
    struct twinapi_appcore *This = impl_from_IEasClientDeviceInformation(iface);
    return twinapi_appcore_Release(&This->IActivationFactory_iface);
}

static HRESULT WINAPI eas_client_devinfo_GetIids(IEasClientDeviceInformation *iface,
        ULONG *iidCount, IID **iids)
{
    struct twinapi_appcore *This = impl_from_IEasClientDeviceInformation(iface);
    return twinapi_appcore_GetIids(&This->IActivationFactory_iface, iidCount, iids);
}

static HRESULT WINAPI eas_client_devinfo_GetRuntimeClassName(IEasClientDeviceInformation *iface,
        HSTRING *className)
{
    struct twinapi_appcore *This = impl_from_IEasClientDeviceInformation(iface);
    return twinapi_appcore_GetRuntimeClassName(&This->IActivationFactory_iface, className);
}

static HRESULT WINAPI eas_client_devinfo_GetTrustLevel(IEasClientDeviceInformation *iface,
        TrustLevel *trustLevel)
{
    struct twinapi_appcore *This = impl_from_IEasClientDeviceInformation(iface);
    return twinapi_appcore_GetTrustLevel(&This->IActivationFactory_iface, trustLevel);
}

static HRESULT WINAPI eas_client_devinfo_get_Id(IEasClientDeviceInformation *iface, GUID* value)
{
    FIXME("iface %p, value %p stub.\n", iface, value);

    return E_NOTIMPL;
}

static HRESULT WINAPI eas_client_devinfo_get_OperatingSystem(IEasClientDeviceInformation *iface,
        HSTRING* value)
{
    FIXME("iface %p, value %p stub.\n", iface, value);

    WindowsCreateString(NULL, 0, value);

    return E_NOTIMPL;
}

static HRESULT WINAPI eas_client_devinfo_get_FriendlyName(IEasClientDeviceInformation *iface,
        HSTRING* value)
{
    FIXME("iface %p, value %p stub.\n", iface, value);

    WindowsCreateString(NULL, 0, value);

    return E_NOTIMPL;
}

static HRESULT WINAPI eas_client_devinfo_get_SystemManufacturer(IEasClientDeviceInformation *iface,
        HSTRING* value)
{
    FIXME("iface %p, value %p stub.\n", iface, value);

    WindowsCreateString(NULL, 0, value);

    return E_NOTIMPL;
}

static HRESULT WINAPI eas_client_devinfo_get_SystemProductName(IEasClientDeviceInformation *iface,
        HSTRING* value)
{
    FIXME("iface %p, value %p stub.\n", iface, value);

    WindowsCreateString(NULL, 0, value);

    return E_NOTIMPL;
}

static HRESULT WINAPI eas_client_devinfo_get_SystemSku(IEasClientDeviceInformation *iface,
        HSTRING* value)
{
    FIXME("iface %p, value %p stub.\n", iface, value);

    WindowsCreateString(NULL, 0, value);

    return E_NOTIMPL;
}

static IEasClientDeviceInformationVtbl eas_client_devinfo_vtbl = {
    eas_client_devinfo_QueryInterface,
    eas_client_devinfo_AddRef,
    eas_client_devinfo_Release,
    /* IInspectable methods */
    eas_client_devinfo_GetIids,
    eas_client_devinfo_GetRuntimeClassName,
    eas_client_devinfo_GetTrustLevel,
    /* IEasClientDeviceInformation methods */
    eas_client_devinfo_get_Id,
    eas_client_devinfo_get_OperatingSystem,
    eas_client_devinfo_get_FriendlyName,
    eas_client_devinfo_get_SystemManufacturer,
    eas_client_devinfo_get_SystemProductName,
    eas_client_devinfo_get_SystemSku,
};

static struct twinapi_appcore twinapi_appcore =
{
    {&activation_factory_vtbl},
    {&eas_client_devinfo_vtbl},
    1
};

HRESULT WINAPI DllCanUnloadNow(void)
{
    return S_FALSE;
}

HRESULT WINAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void **out)
{
    FIXME("clsid %s, riid %s, out %p stub!\n", debugstr_guid(clsid), debugstr_guid(riid), out);
    return CLASS_E_CLASSNOTAVAILABLE;
}

HRESULT WINAPI DllGetActivationFactory(HSTRING classid, IActivationFactory **factory)
{
    TRACE("classid %s, factory %p.\n", debugstr_hstring(classid), factory);
    *factory = &twinapi_appcore.IActivationFactory_iface;
    IUnknown_AddRef(*factory);
    return S_OK;
}
