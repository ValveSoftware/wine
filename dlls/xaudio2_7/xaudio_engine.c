/*
 * Copyright (c) 2015 Mark Harmstone
 * Copyright (c) 2015 Andrew Eikum for CodeWeavers
 * Copyright (c) 2018 Ethan Lee for CodeWeavers
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

#include "xaudio_private.h"

#include "ole2.h"
#include "rpcproxy.h"

#include "wine/debug.h"
#include "wine/heap.h"

#if XAUDIO2_VER == 0
#define COMPAT_E_INVALID_CALL E_INVALIDARG
#define COMPAT_E_DEVICE_INVALIDATED XAUDIO20_E_DEVICE_INVALIDATED
#else
#define COMPAT_E_INVALID_CALL XAUDIO2_E_INVALID_CALL
#define COMPAT_E_DEVICE_INVALIDATED XAUDIO2_E_DEVICE_INVALIDATED
#endif

WINE_DEFAULT_DEBUG_CHANNEL(xaudio2);

static inline IXAudio2Impl *impl_from_IXAudio2(IXAudio2 *iface)
{
    return CONTAINING_RECORD(iface, IXAudio2Impl, IXAudio2_iface);
}

static HRESULT WINAPI IXAudio2Impl_QueryInterface(IXAudio2 *iface, REFIID riid,
        void **ppvObject)
{
    IXAudio2Impl *This = impl_from_IXAudio2(iface);

    TRACE("(%p)->(%s, %p)\n", This, debugstr_guid(riid), ppvObject);

    if(IsEqualGUID(riid, &IID_IUnknown) ||
            IsEqualGUID(riid, &IID_IXAudio28) ||
            IsEqualGUID(riid, &IID_IXAudio2))
        *ppvObject = &This->IXAudio2_iface;
    else if(IsEqualGUID(riid, &IID_IXAudio27)){
        /* all xaudio versions before 28 share an IID */
#if XAUDIO2_VER == 0
        *ppvObject = &This->IXAudio20_iface;
#elif XAUDIO2_VER <= 2
        *ppvObject = &This->IXAudio22_iface;
#elif XAUDIO2_VER <= 7
        *ppvObject = &This->IXAudio27_iface;
#else
        *ppvObject = NULL;
#endif
    }else
        *ppvObject = NULL;

    if(*ppvObject){
        IUnknown_AddRef((IUnknown*)*ppvObject);
        return S_OK;
    }

    FIXME("(%p)->(%s,%p), not found\n", This,debugstr_guid(riid), ppvObject);

    return E_NOINTERFACE;
}

static ULONG WINAPI IXAudio2Impl_AddRef(IXAudio2 *iface)
{
    IXAudio2Impl *This = impl_from_IXAudio2(iface);
    ULONG ref = FAudio_AddRef(This->faudio);
    TRACE("(%p)->(): Refcount now %u\n", This, ref);
    return ref;
}

static ULONG WINAPI IXAudio2Impl_Release(IXAudio2 *iface)
{
    IXAudio2Impl *This = impl_from_IXAudio2(iface);
    ULONG ref = FAudio_Release(This->faudio);

    TRACE("(%p)->(): Refcount now %u\n", This, ref);

    if (!ref) {
        XA2VoiceImpl *v, *v2;

        LIST_FOR_EACH_ENTRY_SAFE(v, v2, &This->voices, XA2VoiceImpl, entry){
            v->lock.DebugInfo->Spare[0] = 0;
            DeleteCriticalSection(&v->lock);
            HeapFree(GetProcessHeap(), 0, v);
        }

        HeapFree(GetProcessHeap(), 0, This->cbs);

        This->lock.DebugInfo->Spare[0] = 0;
        DeleteCriticalSection(&This->lock);

        HeapFree(GetProcessHeap(), 0, This);
    }
    return ref;
}

static HRESULT WINAPI IXAudio2Impl_RegisterForCallbacks(IXAudio2 *iface,
        IXAudio2EngineCallback *pCallback)
{
    IXAudio2Impl *This = impl_from_IXAudio2(iface);
    int i;

    TRACE("(%p)->(%p)\n", This, pCallback);

    EnterCriticalSection(&This->lock);

    for(i = 0; i < This->ncbs; ++i){
        if(!This->cbs[i] || This->cbs[i] == pCallback){
            This->cbs[i] = pCallback;
            LeaveCriticalSection(&This->lock);
            return S_OK;
        }
    }

    This->ncbs++;
    This->cbs = heap_realloc(This->cbs, This->ncbs * sizeof(*This->cbs));

    This->cbs[i] = pCallback;

    LeaveCriticalSection(&This->lock);

    return S_OK;
}

static void WINAPI IXAudio2Impl_UnregisterForCallbacks(IXAudio2 *iface,
        IXAudio2EngineCallback *pCallback)
{
    IXAudio2Impl *This = impl_from_IXAudio2(iface);
    int i;

    TRACE("(%p)->(%p)\n", This, pCallback);

    EnterCriticalSection(&This->lock);

    if(This->ncbs == 0){
        LeaveCriticalSection(&This->lock);
        return;
    }

    for(i = 0; i < This->ncbs; ++i){
        if(This->cbs[i] == pCallback)
            break;
    }

    for(; i < This->ncbs - 1 && This->cbs[i + 1]; ++i)
        This->cbs[i] = This->cbs[i + 1];

    if(i < This->ncbs)
        This->cbs[i] = NULL;

    LeaveCriticalSection(&This->lock);
}

static XA2VoiceImpl *create_voice(IXAudio2Impl *This)
{
    XA2VoiceImpl *voice;

    voice = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*voice));
    if(!voice)
        return NULL;

    list_add_head(&This->voices, &voice->entry);

    voice->IXAudio2SourceVoice_iface.lpVtbl = &XAudio2SourceVoice_Vtbl;
#if XAUDIO2_VER == 0
    voice->IXAudio20SourceVoice_iface.lpVtbl = &XAudio20SourceVoice_Vtbl;
#elif XAUDIO2_VER <= 3
    voice->IXAudio23SourceVoice_iface.lpVtbl = &XAudio23SourceVoice_Vtbl;
#elif XAUDIO2_VER <= 7
    voice->IXAudio27SourceVoice_iface.lpVtbl = &XAudio27SourceVoice_Vtbl;
#endif

    voice->IXAudio2SubmixVoice_iface.lpVtbl = &XAudio2SubmixVoice_Vtbl;
#if XAUDIO2_VER == 0
    voice->IXAudio20SubmixVoice_iface.lpVtbl = &XAudio20SubmixVoice_Vtbl;
#elif XAUDIO2_VER <= 3
    voice->IXAudio23SubmixVoice_iface.lpVtbl = &XAudio23SubmixVoice_Vtbl;
#elif XAUDIO2_VER <= 7
    voice->IXAudio27SubmixVoice_iface.lpVtbl = &XAudio27SubmixVoice_Vtbl;
#endif

    voice->FAudioVoiceCallback_vtbl = FAudioVoiceCallback_Vtbl;

    InitializeCriticalSection(&voice->lock);
    voice->lock.DebugInfo->Spare[0] = (DWORD_PTR)(__FILE__ ": XA2VoiceImpl.lock");

    return voice;
}

static HRESULT WINAPI IXAudio2Impl_CreateSourceVoice(IXAudio2 *iface,
        IXAudio2SourceVoice **ppSourceVoice, const WAVEFORMATEX *pSourceFormat,
        UINT32 flags, float maxFrequencyRatio,
        IXAudio2VoiceCallback *pCallback, const XAUDIO2_VOICE_SENDS *pSendList,
        const XAUDIO2_EFFECT_CHAIN *pEffectChain)
{
    IXAudio2Impl *This = impl_from_IXAudio2(iface);
    XA2VoiceImpl *src;
    HRESULT hr;
    FAudioVoiceSends *faudio_sends;

    TRACE("(%p)->(%p, %p, 0x%x, %f, %p, %p, %p)\n", This, ppSourceVoice,
            pSourceFormat, flags, maxFrequencyRatio, pCallback, pSendList,
            pEffectChain);

    EnterCriticalSection(&This->lock);

    LIST_FOR_EACH_ENTRY(src, &This->voices, XA2VoiceImpl, entry){
        EnterCriticalSection(&src->lock);
        if(!src->in_use)
            break;
        LeaveCriticalSection(&src->lock);
    }

    if(&src->entry == &This->voices){
        src = create_voice(This);
        EnterCriticalSection(&src->lock);
    }

    LeaveCriticalSection(&This->lock);

    src->effect_chain = wrap_effect_chain(pEffectChain);
    faudio_sends = wrap_voice_sends(pSendList);

    hr = FAudio_CreateSourceVoice(This->faudio, &src->faudio_voice,
            (FAudioWaveFormatEx*)pSourceFormat, flags, maxFrequencyRatio,
            &src->FAudioVoiceCallback_vtbl, faudio_sends,
            src->effect_chain);
    free_voice_sends(faudio_sends);
    if(FAILED(hr)){
        LeaveCriticalSection(&This->lock);
        return hr;
    }
    src->in_use = TRUE;
    src->cb = pCallback;

    LeaveCriticalSection(&src->lock);

#if XAUDIO2_VER == 0
    *ppSourceVoice = (IXAudio2SourceVoice*)&src->IXAudio20SourceVoice_iface;
#elif XAUDIO2_VER <= 3
    *ppSourceVoice = (IXAudio2SourceVoice*)&src->IXAudio23SourceVoice_iface;
#elif XAUDIO2_VER <= 7
    *ppSourceVoice = (IXAudio2SourceVoice*)&src->IXAudio27SourceVoice_iface;
#else
    *ppSourceVoice = &src->IXAudio2SourceVoice_iface;
#endif

    TRACE("Created source voice: %p\n", src);

    return S_OK;
}

static HRESULT WINAPI IXAudio2Impl_CreateSubmixVoice(IXAudio2 *iface,
        IXAudio2SubmixVoice **ppSubmixVoice, UINT32 inputChannels,
        UINT32 inputSampleRate, UINT32 flags, UINT32 processingStage,
        const XAUDIO2_VOICE_SENDS *pSendList,
        const XAUDIO2_EFFECT_CHAIN *pEffectChain)
{
    HRESULT hr;
    IXAudio2Impl *This = impl_from_IXAudio2(iface);
    XA2VoiceImpl *sub;
    FAudioVoiceSends *faudio_sends;

    TRACE("(%p)->(%p, %u, %u, 0x%x, %u, %p, %p)\n", This, ppSubmixVoice,
            inputChannels, inputSampleRate, flags, processingStage, pSendList,
            pEffectChain);

    EnterCriticalSection(&This->lock);

    LIST_FOR_EACH_ENTRY(sub, &This->voices, XA2VoiceImpl, entry){
        EnterCriticalSection(&sub->lock);
        if(!sub->in_use)
            break;
        LeaveCriticalSection(&sub->lock);
    }

    if(&sub->entry == &This->voices){
        sub = create_voice(This);
        EnterCriticalSection(&sub->lock);
    }

    LeaveCriticalSection(&This->lock);

    sub->effect_chain = wrap_effect_chain(pEffectChain);
    faudio_sends = wrap_voice_sends(pSendList);

    hr = FAudio_CreateSubmixVoice(This->faudio, &sub->faudio_voice, inputChannels,
            inputSampleRate, flags, processingStage, faudio_sends,
            sub->effect_chain);
    free_voice_sends(faudio_sends);
    if(FAILED(hr)){
        LeaveCriticalSection(&sub->lock);
        return hr;
    }
    sub->in_use = TRUE;

    LeaveCriticalSection(&sub->lock);

#if XAUDIO2_VER == 0
    *ppSubmixVoice = (IXAudio2SubmixVoice*)&sub->IXAudio20SubmixVoice_iface;
#elif XAUDIO2_VER <= 3
    *ppSubmixVoice = (IXAudio2SubmixVoice*)&sub->IXAudio23SubmixVoice_iface;
#elif XAUDIO2_VER <= 7
    *ppSubmixVoice = (IXAudio2SubmixVoice*)&sub->IXAudio27SubmixVoice_iface;
#else
    *ppSubmixVoice = &sub->IXAudio2SubmixVoice_iface;
#endif

    TRACE("Created submix voice: %p\n", sub);

    return S_OK;
}

static HRESULT WINAPI IXAudio2Impl_CreateMasteringVoice(IXAudio2 *iface,
        IXAudio2MasteringVoice **ppMasteringVoice, UINT32 inputChannels,
        UINT32 inputSampleRate, UINT32 flags, const WCHAR *deviceId,
        const XAUDIO2_EFFECT_CHAIN *pEffectChain,
        AUDIO_STREAM_CATEGORY streamCategory)
{
    IXAudio2Impl *This = impl_from_IXAudio2(iface);

    TRACE("(%p)->(%p, %u, %u, 0x%x, %s, %p, 0x%x)\n", This,
            ppMasteringVoice, inputChannels, inputSampleRate, flags,
            wine_dbgstr_w(deviceId), pEffectChain, streamCategory);

    EnterCriticalSection(&This->lock);

#if XAUDIO2_VER == 0
    *ppMasteringVoice = (IXAudio2MasteringVoice*)&This->mst.IXAudio20MasteringVoice_iface;
#elif XAUDIO2_VER <= 3
    *ppMasteringVoice = (IXAudio2MasteringVoice*)&This->mst.IXAudio23MasteringVoice_iface;
#elif XAUDIO2_VER <= 7
    *ppMasteringVoice = (IXAudio2MasteringVoice*)&This->mst.IXAudio27MasteringVoice_iface;
#else
    *ppMasteringVoice = &This->mst.IXAudio2MasteringVoice_iface;
#endif

    EnterCriticalSection(&This->mst.lock);

    if(This->mst.in_use){
        LeaveCriticalSection(&This->mst.lock);
        LeaveCriticalSection(&This->lock);
        return COMPAT_E_INVALID_CALL;
    }

    LeaveCriticalSection(&This->lock);

    This->mst.effect_chain = wrap_effect_chain(pEffectChain);

    FAudio_CreateMasteringVoice(This->faudio, &This->mst.faudio_voice, inputChannels,
            inputSampleRate, flags, 0 /* TODO */, This->mst.effect_chain);

    This->mst.in_use = TRUE;

    LeaveCriticalSection(&This->mst.lock);

    return S_OK;
}

static HRESULT WINAPI IXAudio2Impl_StartEngine(IXAudio2 *iface)
{
    IXAudio2Impl *This = impl_from_IXAudio2(iface);

    TRACE("(%p)->()\n", This);

    return FAudio_StartEngine(This->faudio);
}

static void WINAPI IXAudio2Impl_StopEngine(IXAudio2 *iface)
{
    IXAudio2Impl *This = impl_from_IXAudio2(iface);

    TRACE("(%p)->()\n", This);

    FAudio_StopEngine(This->faudio);
}

static HRESULT WINAPI IXAudio2Impl_CommitChanges(IXAudio2 *iface,
        UINT32 operationSet)
{
    IXAudio2Impl *This = impl_from_IXAudio2(iface);

    TRACE("(%p)->(0x%x): stub!\n", This, operationSet);

    return FAudio_CommitChanges(This->faudio);
}

static void WINAPI IXAudio2Impl_GetPerformanceData(IXAudio2 *iface,
        XAUDIO2_PERFORMANCE_DATA *pPerfData)
{
    IXAudio2Impl *This = impl_from_IXAudio2(iface);

    TRACE("(%p)->(%p): stub!\n", This, pPerfData);

    FAudio_GetPerformanceData(This->faudio, (FAudioPerformanceData *)pPerfData);
}

static void WINAPI IXAudio2Impl_SetDebugConfiguration(IXAudio2 *iface,
        const XAUDIO2_DEBUG_CONFIGURATION *pDebugConfiguration,
        void *pReserved)
{
    IXAudio2Impl *This = impl_from_IXAudio2(iface);

    TRACE("(%p)->(%p, %p): stub!\n", This, pDebugConfiguration, pReserved);

    FAudio_SetDebugConfiguration(This->faudio, (FAudioDebugConfiguration *)pDebugConfiguration, pReserved);
}

/* XAudio2 2.8 */
static const IXAudio2Vtbl XAudio2_Vtbl =
{
    IXAudio2Impl_QueryInterface,
    IXAudio2Impl_AddRef,
    IXAudio2Impl_Release,
    IXAudio2Impl_RegisterForCallbacks,
    IXAudio2Impl_UnregisterForCallbacks,
    IXAudio2Impl_CreateSourceVoice,
    IXAudio2Impl_CreateSubmixVoice,
    IXAudio2Impl_CreateMasteringVoice,
    IXAudio2Impl_StartEngine,
    IXAudio2Impl_StopEngine,
    IXAudio2Impl_CommitChanges,
    IXAudio2Impl_GetPerformanceData,
    IXAudio2Impl_SetDebugConfiguration
};

struct xaudio2_cf {
    IClassFactory IClassFactory_iface;
    LONG ref;
};

static struct xaudio2_cf *impl_from_IClassFactory(IClassFactory *iface)
{
    return CONTAINING_RECORD(iface, struct xaudio2_cf, IClassFactory_iface);
}

static HRESULT WINAPI XAudio2CF_QueryInterface(IClassFactory *iface, REFIID riid, void **ppobj)
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

static ULONG WINAPI XAudio2CF_AddRef(IClassFactory *iface)
{
    struct xaudio2_cf *This = impl_from_IClassFactory(iface);
    ULONG ref = InterlockedIncrement(&This->ref);
    TRACE("(%p)->(): Refcount now %u\n", This, ref);
    return ref;
}

static ULONG WINAPI XAudio2CF_Release(IClassFactory *iface)
{
    struct xaudio2_cf *This = impl_from_IClassFactory(iface);
    ULONG ref = InterlockedDecrement(&This->ref);
    TRACE("(%p)->(): Refcount now %u\n", This, ref);
    if (!ref)
        HeapFree(GetProcessHeap(), 0, This);
    return ref;
}

static HRESULT WINAPI XAudio2CF_CreateInstance(IClassFactory *iface, IUnknown *pOuter,
                                               REFIID riid, void **ppobj)
{
    struct xaudio2_cf *This = impl_from_IClassFactory(iface);
    HRESULT hr;
    IXAudio2Impl *object;

    TRACE("(%p)->(%p,%s,%p)\n", This, pOuter, debugstr_guid(riid), ppobj);

    *ppobj = NULL;

    if(pOuter)
        return CLASS_E_NOAGGREGATION;

    object = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*object));
    if(!object)
        return E_OUTOFMEMORY;

    object->IXAudio2_iface.lpVtbl = &XAudio2_Vtbl;

#if XAUDIO2_VER == 0
    object->IXAudio20_iface.lpVtbl = &XAudio20_Vtbl;
#elif XAUDIO2_VER <= 2
    object->IXAudio22_iface.lpVtbl = &XAudio22_Vtbl;
#elif XAUDIO2_VER <= 7
    object->IXAudio27_iface.lpVtbl = &XAudio27_Vtbl;
#endif

    object->mst.IXAudio2MasteringVoice_iface.lpVtbl = &XAudio2MasteringVoice_Vtbl;

#if XAUDIO2_VER == 0
    object->mst.IXAudio20MasteringVoice_iface.lpVtbl = &XAudio20MasteringVoice_Vtbl;
#elif XAUDIO2_VER <= 3
    object->mst.IXAudio23MasteringVoice_iface.lpVtbl = &XAudio23MasteringVoice_Vtbl;
#elif XAUDIO2_VER <= 7
    object->mst.IXAudio27MasteringVoice_iface.lpVtbl = &XAudio27MasteringVoice_Vtbl;
#endif

    object->FAudioEngineCallback_vtbl = FAudioEngineCallback_Vtbl;

    list_init(&object->voices);

    InitializeCriticalSection(&object->lock);
    object->lock.DebugInfo->Spare[0] = (DWORD_PTR)(__FILE__ ": IXAudio2Impl.lock");

    InitializeCriticalSection(&object->mst.lock);
    object->mst.lock.DebugInfo->Spare[0] = (DWORD_PTR)(__FILE__ ": XA2MasteringVoice.lock");

    FAudioCOMConstructEXT(&object->faudio, XAUDIO2_VER);

    FAudio_RegisterForCallbacks(object->faudio, &object->FAudioEngineCallback_vtbl);

    hr = IXAudio2_QueryInterface(&object->IXAudio2_iface, riid, ppobj);
    if(FAILED(hr)){
        object->lock.DebugInfo->Spare[0] = 0;
        DeleteCriticalSection(&object->lock);
        HeapFree(GetProcessHeap(), 0, object);
        return hr;
    }

    TRACE("Created XAudio version %u: %p\n", 20 + XAUDIO2_VER, object);

    return hr;
}

static HRESULT WINAPI XAudio2CF_LockServer(IClassFactory *iface, BOOL dolock)
{
    struct xaudio2_cf *This = impl_from_IClassFactory(iface);
    FIXME("(%p)->(%d): stub!\n", This, dolock);
    return S_OK;
}

static const IClassFactoryVtbl XAudio2CF_Vtbl =
{
    XAudio2CF_QueryInterface,
    XAudio2CF_AddRef,
    XAudio2CF_Release,
    XAudio2CF_CreateInstance,
    XAudio2CF_LockServer
};

HRESULT make_xaudio2_factory(REFIID riid, void **ppv)
{
    HRESULT hr;
    struct xaudio2_cf *ret = HeapAlloc(GetProcessHeap(), 0, sizeof(struct xaudio2_cf));
    ret->IClassFactory_iface.lpVtbl = &XAudio2CF_Vtbl;
    ret->ref = 0;
    hr = IClassFactory_QueryInterface(&ret->IClassFactory_iface, riid, ppv);
    if(FAILED(hr))
        HeapFree(GetProcessHeap(), 0, ret);
    return hr;
}

HRESULT xaudio2_initialize(IXAudio2Impl *This, UINT32 flags, XAUDIO2_PROCESSOR proc)
{
    return FAudio_Initialize(This->faudio, flags, proc);
}
