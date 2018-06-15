/*
 * Copyright (c) 2015 Andrew Eikum for CodeWeavers
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

#include "config.h"

#include <stdarg.h>

#define NONAMELESSUNION
#define COBJMACROS

#include "initguid.h"
#include "xaudio_private.h"
#include "xapofx.h"

#include "wine/debug.h"
#include "wine/heap.h"

#include "FAudio/FAPO.h"
#include "FAudio/FAudioFX.h"

WINE_DEFAULT_DEBUG_CHANNEL(xaudio2);

#ifdef XAPOFX1_VER
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD reason, void *pReserved)
{
    TRACE("(%p, %d, %p)\n", hinstDLL, reason, pReserved);

    switch (reason)
    {
    case DLL_WINE_PREATTACH:
        return FALSE;  /* prefer native version */
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls( hinstDLL );
        break;
    }
    return TRUE;
}
#endif /* XAPOFX1_VER */

static XA2XAPOFXImpl *impl_from_IXAPO(IXAPO *iface)
{
    return CONTAINING_RECORD(iface, XA2XAPOFXImpl, IXAPO_iface);
}

static XA2XAPOFXImpl *impl_from_IXAPOParameters(IXAPOParameters *iface)
{
    return CONTAINING_RECORD(iface, XA2XAPOFXImpl, IXAPOParameters_iface);
}

static HRESULT WINAPI XAPOFX_QueryInterface(IXAPO *iface, REFIID riid, void **ppvObject)
{
    XA2XAPOFXImpl *This = impl_from_IXAPO(iface);

    TRACE("%p, %s, %p\n", This, wine_dbgstr_guid(riid), ppvObject);

    if(IsEqualGUID(riid, &IID_IUnknown) ||
            IsEqualGUID(riid, &IID_IXAPO) ||
            IsEqualGUID(riid, &IID_IXAPO27))
        *ppvObject = &This->IXAPO_iface;
    else if(IsEqualGUID(riid, &IID_IXAPOParameters) ||
            IsEqualGUID(riid, &IID_IXAPO27Parameters))
        *ppvObject = &This->IXAPOParameters_iface;
    else
        *ppvObject = NULL;

    if(*ppvObject){
        IUnknown_AddRef((IUnknown*)*ppvObject);
        return S_OK;
    }

    return E_NOINTERFACE;
}

static ULONG WINAPI XAPOFX_AddRef(IXAPO *iface)
{
    XA2XAPOFXImpl *This = impl_from_IXAPO(iface);
    ULONG ref = This->fapo->AddRef(This->fapo);
    TRACE("(%p)->(): Refcount now %u\n", This, ref);
    return ref;
}

static ULONG WINAPI XAPOFX_Release(IXAPO *iface)
{
    XA2XAPOFXImpl *This = impl_from_IXAPO(iface);
    ULONG ref = This->fapo->Release(This->fapo);

    TRACE("(%p)->(): Refcount now %u\n", This, ref);

    if(!ref)
        HeapFree(GetProcessHeap(), 0, This);

    return ref;
}

static HRESULT WINAPI XAPOFX_GetRegistrationProperties(IXAPO *iface,
    XAPO_REGISTRATION_PROPERTIES **props)
{
    XA2XAPOFXImpl *This = impl_from_IXAPO(iface);
    HRESULT hr;
    FAPORegistrationProperties *fprops;

    TRACE("%p, %p\n", This, props);

    /* TODO: check for version == 20 and use XAPO20_REGISTRATION_PROPERTIES */
    hr = This->fapo->GetRegistrationProperties(This->fapo, &fprops);
    if(FAILED(hr))
        return hr;

    *props = CoTaskMemAlloc(sizeof(XAPO_REGISTRATION_PROPERTIES));
    memcpy(*props, fprops, sizeof(XAPO_REGISTRATION_PROPERTIES));
    heap_free(fprops);
    return hr;
}

static HRESULT WINAPI XAPOFX_IsInputFormatSupported(IXAPO *iface,
        const WAVEFORMATEX *output_fmt, const WAVEFORMATEX *input_fmt,
        WAVEFORMATEX **supported_fmt)
{
    XA2XAPOFXImpl *This = impl_from_IXAPO(iface);
    TRACE("%p, %p, %p, %p\n", This, output_fmt, input_fmt, supported_fmt);
    return This->fapo->IsInputFormatSupported(This->fapo,
            (const FAudioWaveFormatEx *)output_fmt,
            (const FAudioWaveFormatEx *)input_fmt,
            (FAudioWaveFormatEx **)supported_fmt);
}

static HRESULT WINAPI XAPOFX_IsOutputFormatSupported(IXAPO *iface,
        const WAVEFORMATEX *input_fmt, const WAVEFORMATEX *output_fmt,
        WAVEFORMATEX **supported_fmt)
{
    XA2XAPOFXImpl *This = impl_from_IXAPO(iface);
    TRACE("%p, %p, %p, %p\n", This, input_fmt, output_fmt, supported_fmt);
    return This->fapo->IsOutputFormatSupported(This->fapo,
            (const FAudioWaveFormatEx *)input_fmt,
            (const FAudioWaveFormatEx *)output_fmt,
            (FAudioWaveFormatEx **)supported_fmt);
}

static HRESULT WINAPI XAPOFX_Initialize(IXAPO *iface, const void *data,
        UINT32 data_len)
{
    XA2XAPOFXImpl *This = impl_from_IXAPO(iface);
    TRACE("%p, %p, %u\n", This, data, data_len);
    return This->fapo->Initialize(This->fapo, data, data_len);
}

static void WINAPI XAPOFX_Reset(IXAPO *iface)
{
    XA2XAPOFXImpl *This = impl_from_IXAPO(iface);
    TRACE("%p\n", This);
    This->fapo->Reset(This->fapo);
}

static HRESULT WINAPI XAPOFX_LockForProcess(IXAPO *iface, UINT32 in_params_count,
        const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS *in_params,
        UINT32 out_params_count,
        const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS *out_params)
{
    XA2XAPOFXImpl *This = impl_from_IXAPO(iface);
    TRACE("%p, %u, %p, %u, %p\n", This, in_params_count, in_params,
            out_params_count, out_params);
    return This->fapo->LockForProcess(This->fapo,
            in_params_count,
            (const FAPOLockForProcessBufferParameters *)in_params,
            out_params_count,
            (const FAPOLockForProcessBufferParameters *)out_params);
}

static void WINAPI XAPOFX_UnlockForProcess(IXAPO *iface)
{
    XA2XAPOFXImpl *This = impl_from_IXAPO(iface);
    TRACE("%p\n", This);
    This->fapo->UnlockForProcess(This->fapo);
}

static void WINAPI XAPOFX_Process(IXAPO *iface, UINT32 in_params_count,
        const XAPO_PROCESS_BUFFER_PARAMETERS *in_params,
        UINT32 out_params_count,
        const XAPO_PROCESS_BUFFER_PARAMETERS *out_params, BOOL enabled)
{
    XA2XAPOFXImpl *This = impl_from_IXAPO(iface);
    TRACE("%p, %u, %p, %u, %p, %u\n", This, in_params_count, in_params,
            out_params_count, out_params, enabled);
    This->fapo->Process(This->fapo, in_params_count,
            (const FAPOProcessBufferParameters *)in_params, out_params_count,
            (FAPOProcessBufferParameters *)out_params, enabled);
}

static UINT32 WINAPI XAPOFX_CalcInputFrames(IXAPO *iface, UINT32 output_frames)
{
    XA2XAPOFXImpl *This = impl_from_IXAPO(iface);
    TRACE("%p, %u\n", This, output_frames);
    return 0;
}

static UINT32 WINAPI XAPOFX_CalcOutputFrames(IXAPO *iface, UINT32 input_frames)
{
    XA2XAPOFXImpl *This = impl_from_IXAPO(iface);
    TRACE("%p, %u\n", This, input_frames);
    return 0;
}

static const IXAPOVtbl XAPOFX_Vtbl = {
    XAPOFX_QueryInterface,
    XAPOFX_AddRef,
    XAPOFX_Release,
    XAPOFX_GetRegistrationProperties,
    XAPOFX_IsInputFormatSupported,
    XAPOFX_IsOutputFormatSupported,
    XAPOFX_Initialize,
    XAPOFX_Reset,
    XAPOFX_LockForProcess,
    XAPOFX_UnlockForProcess,
    XAPOFX_Process,
    XAPOFX_CalcInputFrames,
    XAPOFX_CalcOutputFrames
};

static HRESULT WINAPI XAPOFXParams_QueryInterface(IXAPOParameters *iface,
        REFIID riid, void **ppvObject)
{
    XA2XAPOFXImpl *This = impl_from_IXAPOParameters(iface);
    return XAPOFX_QueryInterface(&This->IXAPO_iface, riid, ppvObject);
}

static ULONG WINAPI XAPOFXParams_AddRef(IXAPOParameters *iface)
{
    XA2XAPOFXImpl *This = impl_from_IXAPOParameters(iface);
    return XAPOFX_AddRef(&This->IXAPO_iface);
}

static ULONG WINAPI XAPOFXParams_Release(IXAPOParameters *iface)
{
    XA2XAPOFXImpl *This = impl_from_IXAPOParameters(iface);
    return XAPOFX_Release(&This->IXAPO_iface);
}

static void WINAPI XAPOFXParams_SetParameters(IXAPOParameters *iface,
        const void *params, UINT32 params_len)
{
    XA2XAPOFXImpl *This = impl_from_IXAPOParameters(iface);
    TRACE("%p, %p, %u\n", This, params, params_len);
    This->fapo->SetParameters(This->fapo, params, params_len);
}

static void WINAPI XAPOFXParams_GetParameters(IXAPOParameters *iface, void *params,
        UINT32 params_len)
{
    XA2XAPOFXImpl *This = impl_from_IXAPOParameters(iface);
    TRACE("%p, %p, %u\n", This, params, params_len);
    This->fapo->GetParameters(This->fapo, params, params_len);
}

static const IXAPOParametersVtbl XAPOFXParameters_Vtbl = {
    XAPOFXParams_QueryInterface,
    XAPOFXParams_AddRef,
    XAPOFXParams_Release,
    XAPOFXParams_SetParameters,
    XAPOFXParams_GetParameters
};

struct xapo_cf {
    IClassFactory IClassFactory_iface;
    LONG ref;
    const CLSID *class;
};

static struct xapo_cf *xapo_impl_from_IClassFactory(IClassFactory *iface)
{
    return CONTAINING_RECORD(iface, struct xapo_cf, IClassFactory_iface);
}

static HRESULT WINAPI xapocf_QueryInterface(IClassFactory *iface, REFIID riid, void **ppobj)
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

static ULONG WINAPI xapocf_AddRef(IClassFactory *iface)
{
    struct xapo_cf *This = xapo_impl_from_IClassFactory(iface);
    ULONG ref = InterlockedIncrement(&This->ref);
    TRACE("(%p)->(): Refcount now %u\n", This, ref);
    return ref;
}

static ULONG WINAPI xapocf_Release(IClassFactory *iface)
{
    struct xapo_cf *This = xapo_impl_from_IClassFactory(iface);
    ULONG ref = InterlockedDecrement(&This->ref);
    TRACE("(%p)->(): Refcount now %u\n", This, ref);
    if (!ref)
        HeapFree(GetProcessHeap(), 0, This);
    return ref;
}

static HRESULT WINAPI xapocf_CreateInstance(IClassFactory *iface, IUnknown *pOuter,
        REFIID riid, void **ppobj)
{
    struct xapo_cf *This = xapo_impl_from_IClassFactory(iface);
    HRESULT hr;
    XA2XAPOFXImpl *object;

    TRACE("(%p)->(%p,%s,%p)\n", This, pOuter, debugstr_guid(riid), ppobj);

    *ppobj = NULL;

    if(pOuter)
        return CLASS_E_NOAGGREGATION;

    object = heap_alloc(sizeof(*object));
    object->IXAPO_iface.lpVtbl = &XAPOFX_Vtbl;
    object->IXAPOParameters_iface.lpVtbl = &XAPOFXParameters_Vtbl;

    if(IsEqualGUID(This->class, &CLSID_AudioVolumeMeter27)){
        hr = FAudioCreateVolumeMeter(&object->fapo, 0);
    }else if(IsEqualGUID(This->class, &CLSID_FXReverb)){
        hr = FAudioCreateReverb(&object->fapo, 0);
    }else{
        /* TODO FXECHO, FXMasteringLimiter, FXEQ */
        hr = E_INVALIDARG;
    }

    if(FAILED(hr)){
        HeapFree(GetProcessHeap(), 0, object);
        return hr;
    }

    hr = IXAPO_QueryInterface(&object->IXAPO_iface, riid, ppobj);
    if(FAILED(hr)){
        HeapFree(GetProcessHeap(), 0, object);
        return hr;
    }

    return S_OK;
}

static HRESULT WINAPI xapocf_LockServer(IClassFactory *iface, BOOL dolock)
{
    struct xapo_cf *This = xapo_impl_from_IClassFactory(iface);
    FIXME("(%p)->(%d): stub!\n", This, dolock);
    return S_OK;
}

static const IClassFactoryVtbl xapo_Vtbl =
{
    xapocf_QueryInterface,
    xapocf_AddRef,
    xapocf_Release,
    xapocf_CreateInstance,
    xapocf_LockServer
};

HRESULT make_xapo_factory(REFCLSID clsid, REFIID riid, void **ppv)
{
    HRESULT hr;
    struct xapo_cf *ret = HeapAlloc(GetProcessHeap(), 0, sizeof(struct xapo_cf));
    ret->IClassFactory_iface.lpVtbl = &xapo_Vtbl;
    ret->class = clsid;
    ret->ref = 0;
    hr = IClassFactory_QueryInterface(&ret->IClassFactory_iface, riid, ppv);
    if(FAILED(hr))
        HeapFree(GetProcessHeap(), 0, ret);
    return hr;
}

#if XAUDIO2_VER >= 8
HRESULT WINAPI CreateAudioVolumeMeter(IUnknown **out)
{
    IClassFactory *cf;
    HRESULT hr;

    hr = make_xapo_factory(&CLSID_AudioVolumeMeter27, &IID_IClassFactory, (void**)&cf);
    if(FAILED(hr))
        return hr;

    hr = IClassFactory_CreateInstance(cf, NULL, &IID_IUnknown, (void**)out);

    IClassFactory_Release(cf);

    return hr;
}

HRESULT WINAPI CreateAudioReverb(IUnknown **out)
{
    IClassFactory *cf;
    HRESULT hr;

    hr = make_xapo_factory(&CLSID_FXReverb, &IID_IClassFactory, (void**)&cf);
    if(FAILED(hr))
        return hr;

    hr = IClassFactory_CreateInstance(cf, NULL, &IID_IUnknown, (void**)out);

    IClassFactory_Release(cf);

    return hr;
}

HRESULT CDECL CreateFX(REFCLSID clsid, IUnknown **out, void *initdata, UINT32 initdata_bytes)
{
    HRESULT hr;
    IUnknown *obj;
    const GUID *class = NULL;
    IClassFactory *cf;

    *out = NULL;

    if(IsEqualGUID(clsid, &CLSID_FXReverb27) ||
            IsEqualGUID(clsid, &CLSID_FXReverb))
        class = &CLSID_FXReverb;
    else if(IsEqualGUID(clsid, &CLSID_FXEQ27) ||
            IsEqualGUID(clsid, &CLSID_FXEQ))
        class = &CLSID_FXEQ;

    if(class){
        hr = make_xapo_factory(class, &IID_IClassFactory, (void**)&cf);
        if(FAILED(hr))
            return hr;

        hr = IClassFactory_CreateInstance(cf, NULL, &IID_IUnknown, (void**)&obj);
        IClassFactory_Release(cf);
        if(FAILED(hr))
            return hr;
    }else{
        hr = CoCreateInstance(clsid, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, (void**)&obj);
        if(FAILED(hr)){
            WARN("CoCreateInstance failed: %08x\n", hr);
            return hr;
        }
    }

    if(initdata && initdata_bytes > 0){
        IXAPO *xapo;

        hr = IUnknown_QueryInterface(obj, &IID_IXAPO, (void**)&xapo);
        if(SUCCEEDED(hr)){
            hr = IXAPO_Initialize(xapo, initdata, initdata_bytes);

            IXAPO_Release(xapo);

            if(FAILED(hr)){
                WARN("Initialize failed: %08x\n", hr);
                IUnknown_Release(obj);
                return hr;
            }
        }
    }

    *out = obj;

    return S_OK;
}
#endif /* XAUDIO2_VER >= 8 */

#ifdef XAPOFX1_VER
HRESULT CDECL CreateFX(REFCLSID clsid, IUnknown **out)
{
    HRESULT hr;
    IUnknown *obj;
    const GUID *class = NULL;
    IClassFactory *cf;

    TRACE("%s %p\n", debugstr_guid(clsid), out);

    *out = NULL;

    if(IsEqualGUID(clsid, &CLSID_FXReverb27) ||
            IsEqualGUID(clsid, &CLSID_FXReverb))
        class = &CLSID_FXReverb;
    else if(IsEqualGUID(clsid, &CLSID_FXEQ27) ||
            IsEqualGUID(clsid, &CLSID_FXEQ))
        class = &CLSID_FXEQ;
    /* TODO FXECHO, FXMasteringLimiter, */

    if(class){
        hr = make_xapo_factory(class, &IID_IClassFactory, (void**)&cf);
        if(FAILED(hr))
            return hr;

        hr = IClassFactory_CreateInstance(cf, NULL, &IID_IUnknown, (void**)&obj);
        IClassFactory_Release(cf);
        if(FAILED(hr))
            return hr;
    }else{
        hr = CoCreateInstance(clsid, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, (void**)&obj);
        if(FAILED(hr)){
            WARN("CoCreateInstance failed: %08x\n", hr);
            return hr;
        }
    }

    *out = obj;

    return S_OK;
}
#endif /* XAPOFX1_VER */
