#include <stdarg.h>

#define COBJMACROS
#include "windef.h"
#include "winbase.h"
#include "winstring.h"
#include "wine/debug.h"
#include "activation.h"
#include "objbase.h"
#include "initguid.h"

WINE_DEFAULT_DEBUG_CHANNEL(speech);

DEFINE_GUID(IID_IInstalledVoicesStatic,0x7d526ecc,0x7533,0x4c3f,0x85,0xbe,0x88,0x8c,0x2b,0xae,0xeb,0xdc);
DEFINE_GUID(IID_IAgileObject,0x94ea2b94,0xe9cc,0x49e0,0xc0,0xff,0xee,0x64,0xca,0x8f,0x5b,0x90);

static const char *debugstr_hstring(HSTRING hstr)
{
    const WCHAR *str;
    UINT32 len;
    if (hstr && !((ULONG_PTR)hstr >> 16)) return "(invalid)";
    str = WindowsGetStringRawBuffer(hstr, &len);
    return wine_dbgstr_wn(str, len);
}

typedef struct IVectorView IVectorView;

typedef struct IVectorViewVtbl
{
    /*** IUnknown methods ***/
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(
        IVectorView *This,
        REFIID riid,
        void **ppvObject);

    ULONG (STDMETHODCALLTYPE *AddRef)(
        IVectorView *This);

    ULONG (STDMETHODCALLTYPE *Release)(
        IVectorView *This);

    /*** IInspectable methods ***/
    HRESULT (STDMETHODCALLTYPE *GetIids)(
        IVectorView *This,
        ULONG *iidCount,
        IID **iids);

    HRESULT (STDMETHODCALLTYPE *GetRuntimeClassName)(
        IVectorView *This,
        HSTRING *className);

    HRESULT (STDMETHODCALLTYPE *GetTrustLevel)(
        IVectorView *This,
        TrustLevel *trustLevel);

    /*** IVectorView<T> methods ***/
    HRESULT (STDMETHODCALLTYPE *GetAt)(
        IVectorView *This,
        ULONG index,
        /* T */ void *out_value);

    HRESULT (STDMETHODCALLTYPE *get_Size)(
        IVectorView *This,
        ULONG *out_value);

    HRESULT (STDMETHODCALLTYPE *IndexOf)(
        IVectorView *This,
        /* T */ void *value,
        ULONG *index,
        BOOLEAN *out_value);

    HRESULT (STDMETHODCALLTYPE *GetMany)(
        IVectorView *This,
        ULONG start_index,
        /* T[] */ void **items,
        UINT *out_value);
} IVectorViewVtbl;

struct IVectorView
{
    CONST_VTBL IVectorViewVtbl* lpVtbl;
};

typedef struct IInstalledVoicesStatic IInstalledVoicesStatic;

typedef struct IInstalledVoicesStaticVtbl
{
    BEGIN_INTERFACE

    /*** IUnknown methods ***/
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(
        IInstalledVoicesStatic *This,
        REFIID riid,
        void **ppvObject);

    ULONG (STDMETHODCALLTYPE *AddRef)(
        IInstalledVoicesStatic *This);

    ULONG (STDMETHODCALLTYPE *Release)(
        IInstalledVoicesStatic *This);

    /*** IInspectable methods ***/
    HRESULT (STDMETHODCALLTYPE *GetIids)(
        IInstalledVoicesStatic *This,
        ULONG *iidCount,
        IID **iids);

    HRESULT (STDMETHODCALLTYPE *GetRuntimeClassName)(
        IInstalledVoicesStatic *This,
        HSTRING *className);

    HRESULT (STDMETHODCALLTYPE *GetTrustLevel)(
        IInstalledVoicesStatic *This,
        TrustLevel *trustLevel);

    /*** IInstalledVoicesStatic methods ***/
    HRESULT (STDMETHODCALLTYPE *get_AllVoices)(
        IInstalledVoicesStatic *This,
        /* Windows.Foundation.Collections.IVectorView<Windows.Media.SpeechSynthesis.VoiceInformation*>** */
        void **value);
    HRESULT (STDMETHODCALLTYPE *get_DefaultVoice)(
        IInstalledVoicesStatic *This,
        /* Windows.Media.SpeechSynthesis.VoiceInformation** */
        void **value);

    END_INTERFACE
} IInstalledVoicesStaticVtbl;

struct IInstalledVoicesStatic
{
    CONST_VTBL IInstalledVoicesStaticVtbl* lpVtbl;
};

struct windows_media_speech
{
    IActivationFactory IActivationFactory_iface;
    IInstalledVoicesStatic IInstalledVoicesStatic_iface;
    IVectorView IVectorView_iface;
    LONG refcount;
};

static inline struct windows_media_speech *impl_from_IActivationFactory(IActivationFactory *iface)
{
    return CONTAINING_RECORD(iface, struct windows_media_speech, IActivationFactory_iface);
}

static inline struct windows_media_speech *impl_from_IInstalledVoicesStatic(IInstalledVoicesStatic *iface)
{
    return CONTAINING_RECORD(iface, struct windows_media_speech, IInstalledVoicesStatic_iface);
}

static inline struct windows_media_speech *impl_from_IVectorView(IVectorView *iface)
{
    return CONTAINING_RECORD(iface, struct windows_media_speech, IVectorView_iface);
}

static HRESULT STDMETHODCALLTYPE vector_view_QueryInterface(
        IVectorView *iface, REFIID iid, void **object)
{
    TRACE("iface %p, iid %s, object %p stub!\n", iface, debugstr_guid(iid), object);
    WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
    *object = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE vector_view_AddRef(
        IVectorView *iface)
{
    struct windows_media_speech *impl = impl_from_IVectorView(iface);
    ULONG rc = InterlockedIncrement(&impl->refcount);
    TRACE("%p increasing refcount to %u.\n", impl, rc);
    return rc;
}

static ULONG STDMETHODCALLTYPE vector_view_Release(
        IVectorView *iface)
{
    struct windows_media_speech *impl = impl_from_IVectorView(iface);
    ULONG rc = InterlockedDecrement(&impl->refcount);
    TRACE("%p decreasing refcount to %u.\n", impl, rc);
    return rc;
}

static HRESULT STDMETHODCALLTYPE vector_view_GetIids(
        IVectorView *iface, ULONG *iid_count, IID **iids)
{
    FIXME("iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE vector_view_GetRuntimeClassName(
        IVectorView *iface, HSTRING *class_name)
{
    FIXME("iface %p, class_name %p stub!\n", iface, class_name);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE vector_view_GetTrustLevel(
        IVectorView *iface, TrustLevel *trust_level)
{
    FIXME("iface %p, trust_level %p stub!\n", iface, trust_level);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE vector_view_GetAt(
    IVectorView *iface, ULONG index, void *out_value)
{
    FIXME("iface %p, index %#x, out_value %p stub!\n", iface, index, out_value);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE vector_view_get_Size(
    IVectorView *iface, ULONG *out_value)
{
    FIXME("iface %p, out_value %p stub!\n", iface, out_value);
    *out_value = 0;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE vector_view_IndexOf(
    IVectorView *iface, void *value, ULONG *index, BOOLEAN *out_value)
{
    FIXME("iface %p, value %p, index %p, out_value %p stub!\n", iface, value, index, out_value);
    *out_value = FALSE;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE vector_view_GetMany(
    IVectorView *iface, ULONG start_index, void **items, UINT *out_value)
{
    FIXME("iface %p, start_index %#x, items %p, out_value %p stub!\n", iface, start_index, items, out_value);
    *out_value = 0;
    return S_OK;
}

static const struct IVectorViewVtbl vector_view_vtbl =
{
    vector_view_QueryInterface,
    vector_view_AddRef,
    vector_view_Release,
    /* IInspectable methods */
    vector_view_GetIids,
    vector_view_GetRuntimeClassName,
    vector_view_GetTrustLevel,
    /*** IVectorView<T> methods ***/
    vector_view_GetAt,
    vector_view_get_Size,
    vector_view_IndexOf,
    vector_view_GetMany,
};

static HRESULT STDMETHODCALLTYPE installed_voices_static_QueryInterface(
        IInstalledVoicesStatic *iface, REFIID iid, void **object)
{
    TRACE("iface %p, iid %s, object %p stub!\n", iface, debugstr_guid(iid), object);

    if (IsEqualGUID(iid, &IID_IAgileObject))
    {
        IUnknown_AddRef(iface);
        *object = iface;
        return S_OK;
    }

    WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
    *object = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE installed_voices_static_AddRef(
        IInstalledVoicesStatic *iface)
{
    struct windows_media_speech *impl = impl_from_IInstalledVoicesStatic(iface);
    ULONG rc = InterlockedIncrement(&impl->refcount);
    TRACE("%p increasing refcount to %u.\n", impl, rc);
    return rc;
}

static ULONG STDMETHODCALLTYPE installed_voices_static_Release(
        IInstalledVoicesStatic *iface)
{
    struct windows_media_speech *impl = impl_from_IInstalledVoicesStatic(iface);
    ULONG rc = InterlockedDecrement(&impl->refcount);
    TRACE("%p decreasing refcount to %u.\n", impl, rc);
    return rc;
}

static HRESULT STDMETHODCALLTYPE installed_voices_static_GetIids(
        IInstalledVoicesStatic *iface, ULONG *iid_count, IID **iids)
{
    FIXME("iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE installed_voices_static_GetRuntimeClassName(
        IInstalledVoicesStatic *iface, HSTRING *class_name)
{
    FIXME("iface %p, class_name %p stub!\n", iface, class_name);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE installed_voices_static_GetTrustLevel(
        IInstalledVoicesStatic *iface, TrustLevel *trust_level)
{
    FIXME("iface %p, trust_level %p stub!\n", iface, trust_level);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE installed_voices_static_get_AllVoices(
    IInstalledVoicesStatic *iface, void **value)
{
    struct windows_media_speech *impl = impl_from_IInstalledVoicesStatic(iface);
    FIXME("iface %p, value %p stub!\n", iface, value);
    *value = &impl->IVectorView_iface;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE installed_voices_static_get_DefaultVoice(
    IInstalledVoicesStatic *iface, void **value)
{
    struct windows_media_speech *impl = impl_from_IInstalledVoicesStatic(iface);
    FIXME("iface %p, value %p stub!\n", iface, value);
    *value = &impl->IVectorView_iface;
    return S_OK;
}

static const struct IInstalledVoicesStaticVtbl installed_voices_static_vtbl =
{
    installed_voices_static_QueryInterface,
    installed_voices_static_AddRef,
    installed_voices_static_Release,
    /* IInspectable methods */
    installed_voices_static_GetIids,
    installed_voices_static_GetRuntimeClassName,
    installed_voices_static_GetTrustLevel,
    /* IInstalledVoicesStatic methods */
    installed_voices_static_get_AllVoices,
    installed_voices_static_get_DefaultVoice,
};

static HRESULT STDMETHODCALLTYPE windows_media_speech_QueryInterface(
        IActivationFactory *iface, REFIID iid, void **object)
{
    struct windows_media_speech *impl = impl_from_IActivationFactory(iface);
    TRACE("iface %p, iid %s, object %p stub!\n", iface, debugstr_guid(iid), object);

    if (IsEqualGUID(iid, &IID_IInstalledVoicesStatic))
    {
        IUnknown_AddRef(iface);
        *object = &impl->IInstalledVoicesStatic_iface;
        return S_OK;
    }

    FIXME("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
    *object = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE windows_media_speech_AddRef(
        IActivationFactory *iface)
{
    struct windows_media_speech *impl = impl_from_IActivationFactory(iface);
    ULONG rc = InterlockedIncrement(&impl->refcount);
    TRACE("%p increasing refcount to %u.\n", impl, rc);
    return rc;
}

static ULONG STDMETHODCALLTYPE windows_media_speech_Release(
        IActivationFactory *iface)
{
    struct windows_media_speech *impl = impl_from_IActivationFactory(iface);
    ULONG rc = InterlockedDecrement(&impl->refcount);
    TRACE("%p decreasing refcount to %u.\n", impl, rc);
    return rc;
}

static HRESULT STDMETHODCALLTYPE windows_media_speech_GetIids(
        IActivationFactory *iface, ULONG *iid_count, IID **iids)
{
    FIXME("iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE windows_media_speech_GetRuntimeClassName(
        IActivationFactory *iface, HSTRING *class_name)
{
    FIXME("iface %p, class_name %p stub!\n", iface, class_name);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE windows_media_speech_GetTrustLevel(
        IActivationFactory *iface, TrustLevel *trust_level)
{
    FIXME("iface %p, trust_level %p stub!\n", iface, trust_level);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE windows_media_speech_ActivateInstance(
        IActivationFactory *iface, IInspectable **instance)
{
    FIXME("iface %p, instance %p stub!\n", iface, instance);
    return E_NOTIMPL;
}

static const struct IActivationFactoryVtbl activation_factory_vtbl =
{
    windows_media_speech_QueryInterface,
    windows_media_speech_AddRef,
    windows_media_speech_Release,
    /* IInspectable methods */
    windows_media_speech_GetIids,
    windows_media_speech_GetRuntimeClassName,
    windows_media_speech_GetTrustLevel,
    /* IActivationFactory methods */
    windows_media_speech_ActivateInstance,
};

static struct windows_media_speech windows_media_speech =
{
    {&activation_factory_vtbl},
    {&installed_voices_static_vtbl},
    {&vector_view_vtbl},
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
    *factory = &windows_media_speech.IActivationFactory_iface;
    IUnknown_AddRef(*factory);
    return S_OK;
}
