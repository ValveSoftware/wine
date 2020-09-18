/*
 * Copyright 2020 Andrew Eikum for CodeWeavers
 * Copyright 2020 Rémi Bernon for CodeWeavers
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
#include "activation.h"
#include "objbase.h"
#include "mmdeviceapi.h"

WINE_DEFAULT_DEBUG_CHANNEL(mediadevices);

typedef ERole Windows_Media_Devices_AudioDeviceRole;

DEFINE_GUID(IID_IMediaDeviceStatics,0xaa2d9a40,0x909f,0x4bba,0xBf,0x8b,0x0C,0x0D,0x29,0x6F,0x14,0xF0);

typedef struct EventRegistrationToken
{
  __int64 value;
} EventRegistrationToken;

static const char *debugstr_hstring(HSTRING hstr)
{
    const WCHAR *str;
    UINT32 len;
    if (hstr && !((ULONG_PTR)hstr >> 16)) return "(invalid)";
    str = WindowsGetStringRawBuffer(hstr, &len);
    return wine_dbgstr_wn(str, len);
}

typedef struct IMediaDeviceStatics IMediaDeviceStatics;

typedef struct IMediaDeviceStaticsVtbl
{
    BEGIN_INTERFACE

    /*** IUnknown methods ***/
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(
        IMediaDeviceStatics *This,
        REFIID riid,
        void **ppvObject);

    ULONG (STDMETHODCALLTYPE *AddRef)(
        IMediaDeviceStatics *This);

    ULONG (STDMETHODCALLTYPE *Release)(
        IMediaDeviceStatics *This);

    /*** IInspectable methods ***/
    HRESULT (STDMETHODCALLTYPE *GetIids)(
        IMediaDeviceStatics *This,
        ULONG *iidCount,
        IID **iids);

    HRESULT (STDMETHODCALLTYPE *GetRuntimeClassName)(
        IMediaDeviceStatics *This,
        HSTRING *className);

    HRESULT (STDMETHODCALLTYPE *GetTrustLevel)(
        IMediaDeviceStatics *This,
        TrustLevel *trustLevel);

    /*** IMediaDeviceStatics methods ***/

    HRESULT (STDMETHODCALLTYPE *GetAudioCaptureSelector)(
        IMediaDeviceStatics *This,
        HSTRING *selector);

    HRESULT (STDMETHODCALLTYPE *GetAudioRenderSelector)(
        IMediaDeviceStatics *This,
        HSTRING *selector);

    HRESULT (STDMETHODCALLTYPE *GetVideoCaptureSelector)(
        IMediaDeviceStatics *This,
        HSTRING *selector);

    HRESULT (STDMETHODCALLTYPE *GetDefaultAudioCaptureId)(
        IMediaDeviceStatics *This,
        Windows_Media_Devices_AudioDeviceRole role,
        HSTRING *device_id);

    HRESULT (STDMETHODCALLTYPE *GetDefaultAudioRenderId)(
        IMediaDeviceStatics *This,
        Windows_Media_Devices_AudioDeviceRole role,
        HSTRING *device_id);

    HRESULT (STDMETHODCALLTYPE *DefaultAudioCaptureDeviceChanged_add)(
        IMediaDeviceStatics *This,
        void *handler,
        EventRegistrationToken *token);

    HRESULT (STDMETHODCALLTYPE *DefaultAudioCaptureDeviceChanged_remove)(
        IMediaDeviceStatics *This,
        EventRegistrationToken token);

    HRESULT (STDMETHODCALLTYPE *DefaultAudioRenderDeviceChanged_add)(
        IMediaDeviceStatics *This,
        void *handler,
        EventRegistrationToken *token);

    HRESULT (STDMETHODCALLTYPE *DefaultAudioRenderDeviceChanged_remove)(
        IMediaDeviceStatics *This,
        EventRegistrationToken token);

    END_INTERFACE
} IMediaDeviceStaticsVtbl;

struct IMediaDeviceStatics
{
    CONST_VTBL IMediaDeviceStaticsVtbl* lpVtbl;
};

struct windows_media_devices
{
    IActivationFactory IActivationFactory_iface;
    IMediaDeviceStatics IMediaDeviceStatics_iface;
    LONG refcount;
};

static inline struct windows_media_devices *impl_from_IActivationFactory(IActivationFactory *iface)
{
    return CONTAINING_RECORD(iface, struct windows_media_devices, IActivationFactory_iface);
}

static inline struct windows_media_devices *impl_from_IMediaDeviceStatics(IMediaDeviceStatics *iface)
{
    return CONTAINING_RECORD(iface, struct windows_media_devices, IMediaDeviceStatics_iface);
}

static HRESULT STDMETHODCALLTYPE media_device_statics_QueryInterface(
        IMediaDeviceStatics *iface, REFIID iid, void **object)
{
    struct windows_media_devices *impl = impl_from_IMediaDeviceStatics(iface);
    TRACE("%p, iid %s, object %p\n", impl, debugstr_guid(iid), object);
    return IActivationFactory_QueryInterface(&impl->IActivationFactory_iface, iid, object);
}

static ULONG STDMETHODCALLTYPE media_device_statics_AddRef(
        IMediaDeviceStatics *iface)
{
    struct windows_media_devices *impl = impl_from_IMediaDeviceStatics(iface);
    return IActivationFactory_AddRef(&impl->IActivationFactory_iface);
}

static ULONG STDMETHODCALLTYPE media_device_statics_Release(
        IMediaDeviceStatics *iface)
{
    struct windows_media_devices *impl = impl_from_IMediaDeviceStatics(iface);
    return IActivationFactory_Release(&impl->IActivationFactory_iface);
}

static HRESULT STDMETHODCALLTYPE media_device_statics_GetIids(
        IMediaDeviceStatics *iface, ULONG *iid_count, IID **iids)
{
    struct windows_media_devices *impl = impl_from_IMediaDeviceStatics(iface);
    return IActivationFactory_GetIids(&impl->IActivationFactory_iface, iid_count, iids);
}

static HRESULT STDMETHODCALLTYPE media_device_statics_GetRuntimeClassName(
        IMediaDeviceStatics *iface, HSTRING *class_name)
{
    struct windows_media_devices *impl = impl_from_IMediaDeviceStatics(iface);
    return IActivationFactory_GetRuntimeClassName(&impl->IActivationFactory_iface, class_name);
}

static HRESULT STDMETHODCALLTYPE media_device_statics_GetTrustLevel(
        IMediaDeviceStatics *iface, TrustLevel *trust_level)
{
    struct windows_media_devices *impl = impl_from_IMediaDeviceStatics(iface);
    return IActivationFactory_GetTrustLevel(&impl->IActivationFactory_iface, trust_level);
}

static HRESULT STDMETHODCALLTYPE media_device_statics_GetAudioCaptureSelector(
    IMediaDeviceStatics *iface, HSTRING *selector)
{
    struct windows_media_devices *impl = impl_from_IMediaDeviceStatics(iface);
    FIXME("%p, selector %p stub!\n", impl, selector);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE media_device_statics_GetAudioRenderSelector(
    IMediaDeviceStatics *iface, HSTRING *selector)
{
    struct windows_media_devices *impl = impl_from_IMediaDeviceStatics(iface);
    FIXME("%p, selector %p stub!\n", impl, selector);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE media_device_statics_GetVideoCaptureSelector(
    IMediaDeviceStatics *iface, HSTRING *selector)
{
    struct windows_media_devices *impl = impl_from_IMediaDeviceStatics(iface);
    FIXME("%p, selector %p stub!\n", impl, selector);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE media_device_statics_GetDefaultAudioCaptureId(
    IMediaDeviceStatics *iface, Windows_Media_Devices_AudioDeviceRole role, HSTRING *device_id)
{
    struct windows_media_devices *impl = impl_from_IMediaDeviceStatics(iface);
    FIXME("%p, role %#x, device_id %p stub!\n", impl, role, device_id);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE media_device_statics_GetDefaultAudioRenderId(
    IMediaDeviceStatics *iface, Windows_Media_Devices_AudioDeviceRole role, HSTRING *device_id)
{
    struct windows_media_devices *impl = impl_from_IMediaDeviceStatics(iface);
    FIXME("%p, role %#x, device_id %p stub!\n", impl, role, device_id);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE media_device_statics_DefaultAudioCaptureDeviceChanged_add(
    IMediaDeviceStatics *iface, void *handler, EventRegistrationToken *token)
{
    struct windows_media_devices *impl = impl_from_IMediaDeviceStatics(iface);
    FIXME("%p, handler %p, token %p stub!\n", impl, handler, token);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE media_device_statics_DefaultAudioCaptureDeviceChanged_remove(
    IMediaDeviceStatics *iface, EventRegistrationToken token)
{
    struct windows_media_devices *impl = impl_from_IMediaDeviceStatics(iface);
    FIXME("%p, token %#I64x stub!\n", impl, token.value);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE media_device_statics_DefaultAudioRenderDeviceChanged_add(
    IMediaDeviceStatics *iface, void *handler, EventRegistrationToken *token)
{
    struct windows_media_devices *impl = impl_from_IMediaDeviceStatics(iface);
    FIXME("%p, handler %p, token %p stub!\n", impl, handler, token);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE media_device_statics_DefaultAudioRenderDeviceChanged_remove(
    IMediaDeviceStatics *iface, EventRegistrationToken token)
{
    struct windows_media_devices *impl = impl_from_IMediaDeviceStatics(iface);
    FIXME("%p, token %#I64x stub!\n", impl, token.value);
    return E_NOTIMPL;
}

static const struct IMediaDeviceStaticsVtbl media_device_statics_vtbl =
{
    media_device_statics_QueryInterface,
    media_device_statics_AddRef,
    media_device_statics_Release,
    /* IInspectable methods */
    media_device_statics_GetIids,
    media_device_statics_GetRuntimeClassName,
    media_device_statics_GetTrustLevel,
    /* IMediaDeviceStatics methods */
    media_device_statics_GetAudioCaptureSelector,
    media_device_statics_GetAudioRenderSelector,
    media_device_statics_GetVideoCaptureSelector,
    media_device_statics_GetDefaultAudioCaptureId,
    media_device_statics_GetDefaultAudioRenderId,
    media_device_statics_DefaultAudioCaptureDeviceChanged_add,
    media_device_statics_DefaultAudioCaptureDeviceChanged_remove,
    media_device_statics_DefaultAudioRenderDeviceChanged_add,
    media_device_statics_DefaultAudioRenderDeviceChanged_remove,
};

static HRESULT STDMETHODCALLTYPE windows_media_devices_QueryInterface(
        IActivationFactory *iface, REFIID iid, void **object)
{
    struct windows_media_devices *impl = impl_from_IActivationFactory(iface);
    TRACE("%p, iid %s, object %p\n", impl, debugstr_guid(iid), object);

    if (IsEqualGUID(iid, &IID_IActivationFactory) ||
            IsEqualGUID(iid, &IID_IInspectable) ||
            IsEqualGUID(iid, &IID_IUnknown))
    {
        *object = &impl->IActivationFactory_iface;
    }
    else if (IsEqualGUID(iid, &IID_IMediaDeviceStatics))
    {
        *object = &impl->IMediaDeviceStatics_iface;
    }
    else
    {
        FIXME("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
        *object = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*object);
    return S_OK;
}

static ULONG STDMETHODCALLTYPE windows_media_devices_AddRef(
        IActivationFactory *iface)
{
    struct windows_media_devices *impl = impl_from_IActivationFactory(iface);
    ULONG rc = InterlockedIncrement(&impl->refcount);
    TRACE("%p increasing refcount to %u.\n", impl, rc);
    return 2;
}

static ULONG STDMETHODCALLTYPE windows_media_devices_Release(
        IActivationFactory *iface)
{
    struct windows_media_devices *impl = impl_from_IActivationFactory(iface);
    ULONG rc = InterlockedDecrement(&impl->refcount);
    TRACE("%p decreasing refcount to %u.\n", impl, rc);
    return 1;
}

static HRESULT STDMETHODCALLTYPE windows_media_devices_GetIids(
        IActivationFactory *iface, ULONG *iid_count, IID **iids)
{
    struct windows_media_devices *impl = impl_from_IActivationFactory(iface);
    FIXME("%p, iid_count %p, iids %p stub!\n", impl, iid_count, iids);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE windows_media_devices_GetRuntimeClassName(
        IActivationFactory *iface, HSTRING *class_name)
{
    struct windows_media_devices *impl = impl_from_IActivationFactory(iface);
    FIXME("%p, class_name %p stub!\n", impl, class_name);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE windows_media_devices_GetTrustLevel(
        IActivationFactory *iface, TrustLevel *trust_level)
{
    struct windows_media_devices *impl = impl_from_IActivationFactory(iface);
    FIXME("%p, trust_level %p stub!\n", impl, trust_level);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE windows_media_devices_ActivateInstance(
        IActivationFactory *iface, IInspectable **instance)
{
    struct windows_media_devices *impl = impl_from_IActivationFactory(iface);
    FIXME("%p, instance %p stub!\n", impl, instance);
    return E_NOTIMPL;
}

static const struct IActivationFactoryVtbl activation_factory_vtbl =
{
    windows_media_devices_QueryInterface,
    windows_media_devices_AddRef,
    windows_media_devices_Release,
    /* IInspectable methods */
    windows_media_devices_GetIids,
    windows_media_devices_GetRuntimeClassName,
    windows_media_devices_GetTrustLevel,
    /* IActivationFactory methods */
    windows_media_devices_ActivateInstance,
};

static struct windows_media_devices windows_media_devices =
{
    {&activation_factory_vtbl},
    {&media_device_statics_vtbl},
    0
};

HRESULT WINAPI DllCanUnloadNow(void)
{
    return S_FALSE;
}

HRESULT WINAPI DllGetClassObject(REFCLSID clsid, REFIID riid, LPVOID *object)
{
    FIXME("clsid %s, riid %s, object %p stub!\n", debugstr_guid(clsid), debugstr_guid(riid), object);
    return CLASS_E_CLASSNOTAVAILABLE;
}

HRESULT WINAPI DllGetActivationFactory(HSTRING classid, IActivationFactory **factory)
{
    TRACE("classid %s, factory %p.\n", debugstr_hstring(classid), factory);
    *factory = &windows_media_devices.IActivationFactory_iface;
    IUnknown_AddRef(*factory);
    return S_OK;
}
