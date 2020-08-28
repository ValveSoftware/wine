#include <stdarg.h>

#define COBJMACROS
#include "windef.h"
#include "winbase.h"
#include "winstring.h"
#include "wine/debug.h"
#include "activation.h"
#include "objbase.h"
#include "initguid.h"

WINE_DEFAULT_DEBUG_CHANNEL(locale);

#include "windows.foundation.h"
#include "windows.globalization.h"

static const char *debugstr_hstring(HSTRING hstr)
{
    const WCHAR *str;
    UINT32 len;
    if (hstr && !((ULONG_PTR)hstr >> 16)) return "(invalid)";
    str = WindowsGetStringRawBuffer(hstr, &len);
    return wine_dbgstr_wn(str, len);
}

DEFINE_GUID(IID_IGlobalizationPreferencesStatics,0x01bf4326,0xed37,0x4e96,0xb0,0xe9,0xc1,0x34,0x0d,0x1e,0xa1,0x58);
DEFINE_GUID(IID_IVectorView,0xbbe1fa4c,0xb0e3,0x4583,0xba,0xef,0x1f,0x1b,0x2e,0x48,0x3e,0x56);
DEFINE_GUID(IID_IAgileObject,0x94ea2b94,0xe9cc,0x49e0,0xc0,0xff,0xee,0x64,0xca,0x8f,0x5b,0x90);
DEFINE_GUID(IID_IInspectable,0xaf86e2e0,0xb12d,0x4c6a,0x9c,0x5a,0xd7,0xaa,0x65,0x10,0x1e,0x90);

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
        /* T */ void **out_value);

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

struct hstring_vector
{
    IVectorView IVectorView_iface;
    LONG ref;

    ULONG count;
    HSTRING values[0];
};

static inline struct hstring_vector *impl_from_IVectorView(IVectorView *iface)
{
    return CONTAINING_RECORD(iface, struct hstring_vector, IVectorView_iface);
}

static HRESULT STDMETHODCALLTYPE hstring_vector_QueryInterface(
        IVectorView *iface, REFIID iid, void **out)
{
    struct hstring_vector *This = impl_from_IVectorView(iface);

    FIXME("iface %p, iid %s, out %p stub!\n", iface, debugstr_guid(iid), out);

    if (!out) return E_INVALIDARG;

    *out = NULL;
    if (!IsEqualIID(&IID_IUnknown, iid) && !IsEqualIID(&IID_IInspectable, iid) && !IsEqualIID(&IID_IVectorView, iid))
    {
        FIXME("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
        return E_NOINTERFACE;
    }

    *out = &This->IVectorView_iface;
    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}

static ULONG STDMETHODCALLTYPE hstring_vector_AddRef(
        IVectorView *iface)
{
    struct hstring_vector *This = impl_from_IVectorView(iface);
    ULONG ref = InterlockedIncrement(&This->ref);
    FIXME("iface %p -> ref %u.\n", iface, ref);
    return ref;
}

static ULONG STDMETHODCALLTYPE hstring_vector_Release(
        IVectorView *iface)
{
    struct hstring_vector *This = impl_from_IVectorView(iface);
    ULONG ref = InterlockedDecrement(&This->ref);
    FIXME("iface %p -> ref %u.\n", iface, ref);
    if (ref == 0)
    {
        while (This->count--) WindowsDeleteString(This->values[This->count]);
        HeapFree(GetProcessHeap(), 0, This);
    }
    return ref;
}

static HRESULT STDMETHODCALLTYPE hstring_vector_GetIids(
        IVectorView *iface, ULONG *iid_count, IID **iids)
{
    FIXME("iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE hstring_vector_GetRuntimeClassName(
        IVectorView *iface, HSTRING *class_name)
{
    FIXME("iface %p, class_name %p stub!\n", iface, class_name);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE hstring_vector_GetTrustLevel(
        IVectorView *iface, TrustLevel *trust_level)
{
    FIXME("iface %p, trust_level %p stub!\n", iface, trust_level);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE hstring_vector_GetAt(
    IVectorView *iface, ULONG index, void **value)
{
    struct hstring_vector *This = impl_from_IVectorView(iface);

    FIXME("iface %p, index %#x, value %p stub!\n", iface, index, value);

    if (index >= This->count) return E_BOUNDS;
    return WindowsDuplicateString(This->values[index], (HSTRING *)value);
}

static HRESULT STDMETHODCALLTYPE hstring_vector_get_Size(
    IVectorView *iface, ULONG *value)
{
    struct hstring_vector *This = impl_from_IVectorView(iface);

    FIXME("iface %p, value %p stub!\n", iface, value);

    *value = This->count;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE hstring_vector_IndexOf(
    IVectorView *iface, void *element, ULONG *index, BOOLEAN *value)
{
    FIXME("iface %p, element %p, index %p, value %p stub!\n", iface, element, index, value);
    *value = FALSE;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE hstring_vector_GetMany(
    IVectorView *iface, ULONG start_index, void **items, UINT *count)
{
    struct hstring_vector *This = impl_from_IVectorView(iface);
    HRESULT hr;
    ULONG i;

    FIXME("iface %p, start_index %#x, items %p, count %p stub!\n", iface, start_index, items, count);

    for (i = start_index; i < This->count; ++i)
        if (FAILED(hr = WindowsDuplicateString(This->values[i], (HSTRING *)(items + i - start_index))))
            return hr;
    *count = This->count - start_index;

    return S_OK;
}

static const struct IVectorViewVtbl hstring_vector_vtbl =
{
    hstring_vector_QueryInterface,
    hstring_vector_AddRef,
    hstring_vector_Release,
    /* IInspectable methods */
    hstring_vector_GetIids,
    hstring_vector_GetRuntimeClassName,
    hstring_vector_GetTrustLevel,
    /*** IVectorView<T> methods ***/
    hstring_vector_GetAt,
    hstring_vector_get_Size,
    hstring_vector_IndexOf,
    hstring_vector_GetMany,
};

static HRESULT hstring_vector_create(HSTRING *values, SIZE_T count, IVectorView **out)
{
    struct hstring_vector *This;

    if (!(This = HeapAlloc(GetProcessHeap(), 0, sizeof(*This) + count * sizeof(HSTRING)))) return E_OUTOFMEMORY;
    This->ref = 1;

    This->IVectorView_iface.lpVtbl = &hstring_vector_vtbl;
    This->count = count;
    memcpy(This->values, values, count * sizeof(HSTRING));

    *out = &This->IVectorView_iface;
    return S_OK;
}

typedef enum DayOfWeek
{
    DayOfWeek_Sunday = 0,
    DayOfWeek_Monday = 1,
    DayOfWeek_Tuesday = 2,
    DayOfWeek_Wednesday = 3,
    DayOfWeek_Thursday = 4,
    DayOfWeek_Friday = 5,
    DayOfWeek_Saturday = 6
} DayOfWeek;

typedef struct IGlobalizationPreferencesStatics IGlobalizationPreferencesStatics;

typedef struct IGlobalizationPreferencesStaticsVtbl
{
    BEGIN_INTERFACE

    /*** IUnknown methods ***/
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(
        IGlobalizationPreferencesStatics *This,
        REFIID riid,
        void **ppvObject);

    ULONG (STDMETHODCALLTYPE *AddRef)(
        IGlobalizationPreferencesStatics *This);

    ULONG (STDMETHODCALLTYPE *Release)(
        IGlobalizationPreferencesStatics *This);

    /*** IInspectable methods ***/
    HRESULT (STDMETHODCALLTYPE *GetIids)(
        IGlobalizationPreferencesStatics *This,
        ULONG *iidCount,
        IID **iids);

    HRESULT (STDMETHODCALLTYPE *GetRuntimeClassName)(
        IGlobalizationPreferencesStatics *This,
        HSTRING *className);

    HRESULT (STDMETHODCALLTYPE *GetTrustLevel)(
        IGlobalizationPreferencesStatics *This,
        TrustLevel *trustLevel);

    /*** IGlobalizationPreferencesStatics methods ***/

    HRESULT (STDMETHODCALLTYPE *get_Calendars)(
        IGlobalizationPreferencesStatics *This,
        /* Windows.Foundation.Collections.IVectorView<HSTRING>** */
        void **value);
    HRESULT (STDMETHODCALLTYPE *get_Clocks)(
        IGlobalizationPreferencesStatics *This,
        /* Windows.Foundation.Collections.IVectorView<HSTRING>** */
        void **value);
    HRESULT (STDMETHODCALLTYPE *get_Currencies)(
        IGlobalizationPreferencesStatics *This,
        /* Windows.Foundation.Collections.IVectorView<HSTRING>** */
        void **value);
    HRESULT (STDMETHODCALLTYPE *get_Languages)(
        IGlobalizationPreferencesStatics *This,
        /* Windows.Foundation.Collections.IVectorView<HSTRING>** */
        void **value);
    HRESULT (STDMETHODCALLTYPE *get_HomeGeographicRegion)(
        IGlobalizationPreferencesStatics *This,
        HSTRING* value);
    HRESULT (STDMETHODCALLTYPE *get_WeekStartsOn)(
        IGlobalizationPreferencesStatics *This,
        /* Windows.Globalization.DayOfWeek* */
        DayOfWeek *value);

    END_INTERFACE
} IGlobalizationPreferencesStaticsVtbl;

struct IGlobalizationPreferencesStatics
{
    CONST_VTBL IGlobalizationPreferencesStaticsVtbl* lpVtbl;
};

struct windows_globalization
{
    IActivationFactory IActivationFactory_iface;
    IGlobalizationPreferencesStatics IGlobalizationPreferencesStatics_iface;
    LONG ref;
};

static inline struct windows_globalization *impl_from_IActivationFactory(IActivationFactory *iface)
{
    return CONTAINING_RECORD(iface, struct windows_globalization, IActivationFactory_iface);
}

static inline struct windows_globalization *impl_from_IGlobalizationPreferencesStatics(IGlobalizationPreferencesStatics *iface)
{
    return CONTAINING_RECORD(iface, struct windows_globalization, IGlobalizationPreferencesStatics_iface);
}

static HRESULT STDMETHODCALLTYPE globalization_preferences_QueryInterface(
        IGlobalizationPreferencesStatics *iface, REFIID iid, void **object)
{
    FIXME("iface %p, iid %s, object %p stub!\n", iface, debugstr_guid(iid), object);

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

static ULONG STDMETHODCALLTYPE globalization_preferences_AddRef(
        IGlobalizationPreferencesStatics *iface)
{
    struct windows_globalization *impl = impl_from_IGlobalizationPreferencesStatics(iface);
    ULONG ref = InterlockedIncrement(&impl->ref);
    FIXME("iface %p -> ref %u.\n", iface, ref);
    return ref;
}

static ULONG STDMETHODCALLTYPE globalization_preferences_Release(
        IGlobalizationPreferencesStatics *iface)
{
    struct windows_globalization *impl = impl_from_IGlobalizationPreferencesStatics(iface);
    ULONG ref = InterlockedDecrement(&impl->ref);
    FIXME("iface %p -> ref %u.\n", iface, ref);
    return ref;
}

static HRESULT STDMETHODCALLTYPE globalization_preferences_GetIids(
        IGlobalizationPreferencesStatics *iface, ULONG *iid_count, IID **iids)
{
    FIXME("iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE globalization_preferences_GetRuntimeClassName(
        IGlobalizationPreferencesStatics *iface, HSTRING *class_name)
{
    FIXME("iface %p, class_name %p stub!\n", iface, class_name);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE globalization_preferences_GetTrustLevel(
        IGlobalizationPreferencesStatics *iface, TrustLevel *trust_level)
{
    FIXME("iface %p, trust_level %p stub!\n", iface, trust_level);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE globalization_preferences_get_Calendars(IGlobalizationPreferencesStatics *iface,
        void** value)
{
    FIXME("iface %p, value %p stub!\n", iface, value);
    return hstring_vector_create(NULL, 0, (IVectorView **)value);
}

static HRESULT STDMETHODCALLTYPE globalization_preferences_get_Clocks(IGlobalizationPreferencesStatics *iface,
        void** value)
{
    FIXME("iface %p, value %p stub!\n", iface, value);
    return hstring_vector_create(NULL, 0, (IVectorView **)value);
}

static HRESULT STDMETHODCALLTYPE globalization_preferences_get_Currencies(IGlobalizationPreferencesStatics *iface,
        void** value)
{
    FIXME("iface %p, value %p stub!\n", iface, value);
    return hstring_vector_create(NULL, 0, (IVectorView **)value);
}

static HRESULT STDMETHODCALLTYPE globalization_preferences_get_Languages(IGlobalizationPreferencesStatics *iface,
        void** value)
{
    HSTRING hstring;
    UINT32 length;
    WCHAR locale_w[LOCALE_NAME_MAX_LENGTH], *tmp;

    FIXME("iface %p, value %p stub!\n", iface, value);

    GetSystemDefaultLocaleName(locale_w, LOCALE_NAME_MAX_LENGTH);

    if ((tmp = wcsrchr(locale_w, '_'))) *tmp = 0;
    if ((tmp = wcschr(locale_w, '-')) && (wcslen(tmp) <= 3 || (tmp = wcschr(tmp + 1, '-')))) *tmp = 0;
    length = wcslen(locale_w);

    FIXME("returning language %s\n", debugstr_w(locale_w));

    WindowsCreateString(locale_w, length, &hstring);
    return hstring_vector_create(&hstring, 1, (IVectorView **)value);
}

static HRESULT STDMETHODCALLTYPE globalization_preferences_get_HomeGeographicRegion(IGlobalizationPreferencesStatics *iface,
        HSTRING* value)
{
    UINT32 length;
    WCHAR locale_w[LOCALE_NAME_MAX_LENGTH], *tmp;
    const WCHAR *country;

    FIXME("iface %p, value %p stub!\n", iface, value);

    GetSystemDefaultLocaleName(locale_w, LOCALE_NAME_MAX_LENGTH);

    if ((tmp = wcsrchr(locale_w, '_'))) *tmp = 0;
    if (!(tmp = wcschr(locale_w, '-')) || (wcslen(tmp) > 3 && !(tmp = wcschr(tmp + 1, '-')))) country = L"US";
    else country = tmp;
    length = wcslen(country);

    FIXME("returning country %s\n", debugstr_w(country));

    return WindowsCreateString(country, length, value);
}

static HRESULT STDMETHODCALLTYPE globalization_preferences_get_WeekStartsOn(IGlobalizationPreferencesStatics *iface,
        DayOfWeek* value)
{
    FIXME("iface %p, value %p stub!\n", iface, value);
    return E_NOTIMPL;
}

static const struct IGlobalizationPreferencesStaticsVtbl globalization_preferences_vtbl =
{
    globalization_preferences_QueryInterface,
    globalization_preferences_AddRef,
    globalization_preferences_Release,
    /* IInspectable methods */
    globalization_preferences_GetIids,
    globalization_preferences_GetRuntimeClassName,
    globalization_preferences_GetTrustLevel,
    /* IGlobalizationPreferencesStatics methods */
    globalization_preferences_get_Calendars,
    globalization_preferences_get_Clocks,
    globalization_preferences_get_Currencies,
    globalization_preferences_get_Languages,
    globalization_preferences_get_HomeGeographicRegion,
    globalization_preferences_get_WeekStartsOn,
};

static HRESULT STDMETHODCALLTYPE windows_globalization_QueryInterface(
        IActivationFactory *iface, REFIID iid, void **object)
{
    struct windows_globalization *impl = impl_from_IActivationFactory(iface);
    FIXME("iface %p, iid %s, object %p stub!\n", iface, debugstr_guid(iid), object);

    if (IsEqualGUID(iid, &IID_IGlobalizationPreferencesStatics))
    {
        IUnknown_AddRef(iface);
        *object = &impl->IGlobalizationPreferencesStatics_iface;
        return S_OK;
    }

    FIXME("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
    *object = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE windows_globalization_AddRef(
        IActivationFactory *iface)
{
    struct windows_globalization *impl = impl_from_IActivationFactory(iface);
    ULONG ref = InterlockedIncrement(&impl->ref);
    FIXME("iface %p -> ref %u.\n", iface, ref);
    return ref;
}

static ULONG STDMETHODCALLTYPE windows_globalization_Release(
        IActivationFactory *iface)
{
    struct windows_globalization *impl = impl_from_IActivationFactory(iface);
    ULONG ref = InterlockedDecrement(&impl->ref);
    FIXME("iface %p -> ref %u.\n", iface, ref);
    return ref;
}

static HRESULT STDMETHODCALLTYPE windows_globalization_GetIids(
        IActivationFactory *iface, ULONG *iid_count, IID **iids)
{
    FIXME("iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE windows_globalization_GetRuntimeClassName(
        IActivationFactory *iface, HSTRING *class_name)
{
    FIXME("iface %p, class_name %p stub!\n", iface, class_name);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE windows_globalization_GetTrustLevel(
        IActivationFactory *iface, TrustLevel *trust_level)
{
    FIXME("iface %p, trust_level %p stub!\n", iface, trust_level);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE windows_globalization_ActivateInstance(
        IActivationFactory *iface, IInspectable **instance)
{
    FIXME("iface %p, instance %p stub!\n", iface, instance);
    return E_NOTIMPL;
}

static const struct IActivationFactoryVtbl activation_factory_vtbl =
{
    windows_globalization_QueryInterface,
    windows_globalization_AddRef,
    windows_globalization_Release,
    /* IInspectable methods */
    windows_globalization_GetIids,
    windows_globalization_GetRuntimeClassName,
    windows_globalization_GetTrustLevel,
    /* IActivationFactory methods */
    windows_globalization_ActivateInstance,
};

static struct windows_globalization windows_globalization =
{
    {&activation_factory_vtbl},
    {&globalization_preferences_vtbl},
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
    FIXME("classid %s, factory %p.\n", debugstr_hstring(classid), factory);
    *factory = &windows_globalization.IActivationFactory_iface;
    IUnknown_AddRef(*factory);
    return S_OK;
}
