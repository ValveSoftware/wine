#include <stdarg.h>

#define COBJMACROS
#include "windef.h"
#include "winbase.h"
#include "winstring.h"
#include "wine/debug.h"
#include "activation.h"
#include "objbase.h"
#include "initguid.h"

WINE_DEFAULT_DEBUG_CHANNEL(uwp);

static const char *debugstr_hstring(HSTRING hstr)
{
    const WCHAR *str;
    UINT32 len;
    if (hstr && !((ULONG_PTR)hstr >> 16)) return "(invalid)";
    str = WindowsGetStringRawBuffer(hstr, &len);
    return wine_dbgstr_wn(str, len);
}

DEFINE_GUID(IID_IGamepadStatics,0x8bbce529,0xd49c,0x39e9,0x95,0x60,0xe4,0x7d,0xde,0x96,0xb7,0xc8);
DEFINE_GUID(IID_IRawGameControllerStatics,0xeb8d0792,0xe95a,0x4b19,0xaf,0xc7,0x0a,0x59,0xf8,0xbf,0x75,0x9e);
DEFINE_GUID(IID_IAgileObject,0x94ea2b94,0xe9cc,0x49e0,0xc0,0xff,0xee,0x64,0xca,0x8f,0x5b,0x90);

typedef struct EventRegistrationToken
{
  __int64 value;
} EventRegistrationToken;

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

typedef struct IGamepadStatics IGamepadStatics;

typedef struct IGamepadStaticsVtbl
{
    BEGIN_INTERFACE

    /*** IUnknown methods ***/
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(
        IGamepadStatics *This,
        REFIID riid,
        void **ppvObject);

    ULONG (STDMETHODCALLTYPE *AddRef)(
        IGamepadStatics *This);

    ULONG (STDMETHODCALLTYPE *Release)(
        IGamepadStatics *This);

    /*** IInspectable methods ***/
    HRESULT (STDMETHODCALLTYPE *GetIids)(
        IGamepadStatics *This,
        ULONG *iidCount,
        IID **iids);

    HRESULT (STDMETHODCALLTYPE *GetRuntimeClassName)(
        IGamepadStatics *This,
        HSTRING *className);

    HRESULT (STDMETHODCALLTYPE *GetTrustLevel)(
        IGamepadStatics *This,
        TrustLevel *trustLevel);

    /*** IGamepadStatics methods ***/
    HRESULT (STDMETHODCALLTYPE *eventadd_GamepadAdded)(
        IGamepadStatics *This,
        /* Windows.Foundation.EventHandler<Windows.Gaming.Input.Gamepad*> */
        void *value,
        EventRegistrationToken* token);
    HRESULT (STDMETHODCALLTYPE *eventremove_GamepadAdded)(
        IGamepadStatics *This,
        EventRegistrationToken token);

    HRESULT (STDMETHODCALLTYPE *eventadd_GamepadRemoved)(
        IGamepadStatics *This,
        /* Windows.Foundation.EventHandler<Windows.Gaming.Input.Gamepad*> */
        void *value,
        EventRegistrationToken* token);
    HRESULT (STDMETHODCALLTYPE *eventremove_GamepadRemoved)(
        IGamepadStatics *This,
        EventRegistrationToken token);

    HRESULT (STDMETHODCALLTYPE *get_Gamepads)(
        IGamepadStatics *This,
        /* Windows.Foundation.Collections.IVectorView<Windows.Gaming.Input.Gamepad*>* */
        void **value);

    END_INTERFACE
} IGamepadStaticsVtbl;

struct IGamepadStatics
{
    CONST_VTBL IGamepadStaticsVtbl* lpVtbl;
};

typedef struct IRawGameControllerStatics IRawGameControllerStatics;

typedef struct IRawGameControllerStaticsVtbl
{
    BEGIN_INTERFACE

    /*** IUnknown methods ***/
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(
        IRawGameControllerStatics *This,
        REFIID riid,
        void **ppvObject);

    ULONG (STDMETHODCALLTYPE *AddRef)(
        IRawGameControllerStatics *This);

    ULONG (STDMETHODCALLTYPE *Release)(
        IRawGameControllerStatics *This);

    /*** IInspectable methods ***/
    HRESULT (STDMETHODCALLTYPE *GetIids)(
        IRawGameControllerStatics *This,
        ULONG *iidCount,
        IID **iids);

    HRESULT (STDMETHODCALLTYPE *GetRuntimeClassName)(
        IRawGameControllerStatics *This,
        HSTRING *className);

    HRESULT (STDMETHODCALLTYPE *GetTrustLevel)(
        IRawGameControllerStatics *This,
        TrustLevel *trustLevel);

    /*** IRawGameControllerStatics methods ***/
    HRESULT (STDMETHODCALLTYPE *eventadd_RawGameControllerAdded)(
        IRawGameControllerStatics *This,
        /* Windows.Foundation.EventHandler<Windows.Gaming.Input.RawGameController*> */
        void *value,
        EventRegistrationToken* token);
    HRESULT (STDMETHODCALLTYPE *eventremove_RawGameControllerAdded)(
        IRawGameControllerStatics *This,
        EventRegistrationToken token);

    HRESULT (STDMETHODCALLTYPE *eventadd_RawGameControllerRemoved)(
        IRawGameControllerStatics *This,
        /* Windows.Foundation.EventHandler<Windows.Gaming.Input.RawGameController*> */
        void *value,
        EventRegistrationToken* token);
    HRESULT (STDMETHODCALLTYPE *eventremove_RawGameControllerRemoved)(
        IRawGameControllerStatics *This,
        EventRegistrationToken token);

    HRESULT (STDMETHODCALLTYPE *get_RawGameControllers)(
        IRawGameControllerStatics *This,
        /* Windows.Foundation.Collections.IVectorView<Windows.Gaming.Input.RawGameController*>* */
        void **value);

    HRESULT (STDMETHODCALLTYPE *FromGameController)(
        IRawGameControllerStatics *This,
        /* Windows.Gaming.Input.IGameController* */
        void *game_controller,
        /* Windows.Gaming.Input.RawGameController** */
        void **value);

    END_INTERFACE
} IRawGameControllerStaticsVtbl;

struct IRawGameControllerStatics
{
    CONST_VTBL IRawGameControllerStaticsVtbl* lpVtbl;
};

struct windows_gaming_input
{
    IActivationFactory IActivationFactory_iface;
    IGamepadStatics IGamepadStatics_iface;
    IRawGameControllerStatics IRawGameControllerStatics_iface;
    IVectorView IVectorView_iface;
    LONG refcount;
};

static inline struct windows_gaming_input *impl_from_IActivationFactory(IActivationFactory *iface)
{
    return CONTAINING_RECORD(iface, struct windows_gaming_input, IActivationFactory_iface);
}

static inline struct windows_gaming_input *impl_from_IGamepadStatics(IGamepadStatics *iface)
{
    return CONTAINING_RECORD(iface, struct windows_gaming_input, IGamepadStatics_iface);
}

static inline struct windows_gaming_input *impl_from_IRawGameControllerStatics(IRawGameControllerStatics *iface)
{
    return CONTAINING_RECORD(iface, struct windows_gaming_input, IRawGameControllerStatics_iface);
}

static inline struct windows_gaming_input *impl_from_IVectorView(IVectorView *iface)
{
    return CONTAINING_RECORD(iface, struct windows_gaming_input, IVectorView_iface);
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
    struct windows_gaming_input *impl = impl_from_IVectorView(iface);
    ULONG rc = InterlockedIncrement(&impl->refcount);
    TRACE("%p increasing refcount to %u.\n", impl, rc);
    return rc;
}

static ULONG STDMETHODCALLTYPE vector_view_Release(
        IVectorView *iface)
{
    struct windows_gaming_input *impl = impl_from_IVectorView(iface);
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

static HRESULT STDMETHODCALLTYPE gamepad_statics_QueryInterface(
        IGamepadStatics *iface, REFIID iid, void **object)
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

static ULONG STDMETHODCALLTYPE gamepad_statics_AddRef(
        IGamepadStatics *iface)
{
    struct windows_gaming_input *impl = impl_from_IGamepadStatics(iface);
    ULONG rc = InterlockedIncrement(&impl->refcount);
    TRACE("%p increasing refcount to %u.\n", impl, rc);
    return rc;
}

static ULONG STDMETHODCALLTYPE gamepad_statics_Release(
        IGamepadStatics *iface)
{
    struct windows_gaming_input *impl = impl_from_IGamepadStatics(iface);
    ULONG rc = InterlockedDecrement(&impl->refcount);
    TRACE("%p decreasing refcount to %u.\n", impl, rc);
    return rc;
}

static HRESULT STDMETHODCALLTYPE gamepad_statics_GetIids(
        IGamepadStatics *iface, ULONG *iid_count, IID **iids)
{
    FIXME("iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE gamepad_statics_GetRuntimeClassName(
        IGamepadStatics *iface, HSTRING *class_name)
{
    FIXME("iface %p, class_name %p stub!\n", iface, class_name);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE gamepad_statics_GetTrustLevel(
        IGamepadStatics *iface, TrustLevel *trust_level)
{
    FIXME("iface %p, trust_level %p stub!\n", iface, trust_level);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE gamepad_statics_eventadd_GamepadAdded(
    IGamepadStatics *iface, void *value, EventRegistrationToken* token)
{
    FIXME("iface %p, value %p, token %p stub!\n", iface, value, token);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE gamepad_statics_eventremove_GamepadAdded(
    IGamepadStatics *iface, EventRegistrationToken token)
{
    FIXME("iface %p, token %#I64x stub!\n", iface, token.value);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE gamepad_statics_eventadd_GamepadRemoved(
    IGamepadStatics *iface, void *value, EventRegistrationToken* token)
{
    FIXME("iface %p, value %p, token %p stub!\n", iface, value, token);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE gamepad_statics_eventremove_GamepadRemoved(
    IGamepadStatics *iface, EventRegistrationToken token)
{
    FIXME("iface %p, token %#I64x stub!\n", iface, token.value);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE gamepad_statics_get_Gamepads(
    IGamepadStatics *iface, void **value)
{
    struct windows_gaming_input *impl = impl_from_IGamepadStatics(iface);
    FIXME("iface %p, value %p stub!\n", iface, value);
    *value = &impl->IVectorView_iface;
    return S_OK;
}

static const struct IGamepadStaticsVtbl gamepad_statics_vtbl =
{
    gamepad_statics_QueryInterface,
    gamepad_statics_AddRef,
    gamepad_statics_Release,
    /* IInspectable methods */
    gamepad_statics_GetIids,
    gamepad_statics_GetRuntimeClassName,
    gamepad_statics_GetTrustLevel,
    /* IGamepadStatics methods */
    gamepad_statics_eventadd_GamepadAdded,
    gamepad_statics_eventremove_GamepadAdded,
    gamepad_statics_eventadd_GamepadRemoved,
    gamepad_statics_eventremove_GamepadRemoved,
    gamepad_statics_get_Gamepads,
};

static HRESULT STDMETHODCALLTYPE raw_game_controller_statics_QueryInterface(
        IRawGameControllerStatics *iface, REFIID iid, void **object)
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

static ULONG STDMETHODCALLTYPE raw_game_controller_statics_AddRef(
        IRawGameControllerStatics *iface)
{
    struct windows_gaming_input *impl = impl_from_IRawGameControllerStatics(iface);
    ULONG rc = InterlockedIncrement(&impl->refcount);
    TRACE("%p increasing refcount to %u.\n", impl, rc);
    return rc;
}

static ULONG STDMETHODCALLTYPE raw_game_controller_statics_Release(
        IRawGameControllerStatics *iface)
{
    struct windows_gaming_input *impl = impl_from_IRawGameControllerStatics(iface);
    ULONG rc = InterlockedDecrement(&impl->refcount);
    TRACE("%p decreasing refcount to %u.\n", impl, rc);
    return rc;
}

static HRESULT STDMETHODCALLTYPE raw_game_controller_statics_GetIids(
        IRawGameControllerStatics *iface, ULONG *iid_count, IID **iids)
{
    FIXME("iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE raw_game_controller_statics_GetRuntimeClassName(
        IRawGameControllerStatics *iface, HSTRING *class_name)
{
    FIXME("iface %p, class_name %p stub!\n", iface, class_name);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE raw_game_controller_statics_GetTrustLevel(
        IRawGameControllerStatics *iface, TrustLevel *trust_level)
{
    FIXME("iface %p, trust_level %p stub!\n", iface, trust_level);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE raw_game_controller_statics_eventadd_RawGameControllerAdded(
    IRawGameControllerStatics *iface, void *value, EventRegistrationToken* token)
{
    FIXME("iface %p, value %p, token %p stub!\n", iface, value, token);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE raw_game_controller_statics_eventremove_RawGameControllerAdded(
    IRawGameControllerStatics *iface, EventRegistrationToken token)
{
    FIXME("iface %p, token %#I64x stub!\n", iface, token.value);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE raw_game_controller_statics_eventadd_RawGameControllerRemoved(
    IRawGameControllerStatics *iface, void *value, EventRegistrationToken* token)
{
    FIXME("iface %p, value %p, token %p stub!\n", iface, value, token);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE raw_game_controller_statics_eventremove_RawGameControllerRemoved(
    IRawGameControllerStatics *iface, EventRegistrationToken token)
{
    FIXME("iface %p, token %#I64x stub!\n", iface, token.value);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE raw_game_controller_statics_get_RawGameControllers(
    IRawGameControllerStatics *iface, void **value)
{
    struct windows_gaming_input *impl = impl_from_IRawGameControllerStatics(iface);
    FIXME("iface %p, value %p stub!\n", iface, value);
    *value = &impl->IVectorView_iface;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE raw_game_controller_statics_FromGameController(
    IRawGameControllerStatics *iface, void *game_controller, void **value)
{
    FIXME("iface %p, game_controller %p, value %p stub!\n", iface, game_controller, value);
    return E_NOTIMPL;
}

static const struct IRawGameControllerStaticsVtbl raw_game_controller_statics_vtbl =
{
    raw_game_controller_statics_QueryInterface,
    raw_game_controller_statics_AddRef,
    raw_game_controller_statics_Release,
    /* IInspectable methods */
    raw_game_controller_statics_GetIids,
    raw_game_controller_statics_GetRuntimeClassName,
    raw_game_controller_statics_GetTrustLevel,
    /* IRawGameControllerStatics methods */
    raw_game_controller_statics_eventadd_RawGameControllerAdded,
    raw_game_controller_statics_eventremove_RawGameControllerAdded,
    raw_game_controller_statics_eventadd_RawGameControllerRemoved,
    raw_game_controller_statics_eventremove_RawGameControllerRemoved,
    raw_game_controller_statics_get_RawGameControllers,
    raw_game_controller_statics_FromGameController,
};

static HRESULT STDMETHODCALLTYPE windows_gaming_input_QueryInterface(
        IActivationFactory *iface, REFIID iid, void **object)
{
    struct windows_gaming_input *impl = impl_from_IActivationFactory(iface);
    TRACE("iface %p, iid %s, object %p stub!\n", iface, debugstr_guid(iid), object);

    if (IsEqualGUID(iid, &IID_IGamepadStatics))
    {
        IUnknown_AddRef(iface);
        *object = &impl->IGamepadStatics_iface;
        return S_OK;
    }

    if (IsEqualGUID(iid, &IID_IRawGameControllerStatics))
    {
        IUnknown_AddRef(iface);
        *object = &impl->IRawGameControllerStatics_iface;
        return S_OK;
    }

    WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
    *object = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE windows_gaming_input_AddRef(
        IActivationFactory *iface)
{
    struct windows_gaming_input *impl = impl_from_IActivationFactory(iface);
    ULONG rc = InterlockedIncrement(&impl->refcount);
    TRACE("%p increasing refcount to %u.\n", impl, rc);
    return rc;
}

static ULONG STDMETHODCALLTYPE windows_gaming_input_Release(
        IActivationFactory *iface)
{
    struct windows_gaming_input *impl = impl_from_IActivationFactory(iface);
    ULONG rc = InterlockedDecrement(&impl->refcount);
    TRACE("%p decreasing refcount to %u.\n", impl, rc);
    return rc;
}

static HRESULT STDMETHODCALLTYPE windows_gaming_input_GetIids(
        IActivationFactory *iface, ULONG *iid_count, IID **iids)
{
    FIXME("iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE windows_gaming_input_GetRuntimeClassName(
        IActivationFactory *iface, HSTRING *class_name)
{
    FIXME("iface %p, class_name %p stub!\n", iface, class_name);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE windows_gaming_input_GetTrustLevel(
        IActivationFactory *iface, TrustLevel *trust_level)
{
    FIXME("iface %p, trust_level %p stub!\n", iface, trust_level);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE windows_gaming_input_ActivateInstance(
        IActivationFactory *iface, IInspectable **instance)
{
    FIXME("iface %p, instance %p stub!\n", iface, instance);
    return E_NOTIMPL;
}

static const struct IActivationFactoryVtbl activation_factory_vtbl =
{
    windows_gaming_input_QueryInterface,
    windows_gaming_input_AddRef,
    windows_gaming_input_Release,
    /* IInspectable methods */
    windows_gaming_input_GetIids,
    windows_gaming_input_GetRuntimeClassName,
    windows_gaming_input_GetTrustLevel,
    /* IActivationFactory methods */
    windows_gaming_input_ActivateInstance,
};

static struct windows_gaming_input windows_gaming_input =
{
    {&activation_factory_vtbl},
    {&gamepad_statics_vtbl},
    {&raw_game_controller_statics_vtbl},
    {&vector_view_vtbl},
    0
};

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void *reserved)
{
    TRACE("(%p, %u, %p)\n", instance, reason, reserved);

    switch (reason)
    {
    case DLL_WINE_PREATTACH:
        return FALSE;    /* prefer native version */
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(instance);
        break;
    }

    return TRUE;
}

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
    *factory = &windows_gaming_input.IActivationFactory_iface;
    IUnknown_AddRef(*factory);
    return S_OK;
}
