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

WINE_DEFAULT_DEBUG_CHANNEL(xaudio2);

static inline XA2XAPOImpl *impl_from_FAPO(FAPO *iface)
{
    return CONTAINING_RECORD(iface, XA2XAPOImpl, FAPO_vtbl);
}

static int32_t FAPOCALL XAPO_AddRef(void *iface)
{
    XA2XAPOImpl *This = impl_from_FAPO(iface);
    TRACE("%p\n", This);
    return InterlockedIncrement(&This->ref);
}

static int32_t FAPOCALL XAPO_Release(void *iface)
{
    int32_t r;
    XA2XAPOImpl *This = impl_from_FAPO(iface);
    TRACE("%p\n", This);
    r = InterlockedDecrement(&This->ref);
    if(r == 0){
        IXAPO_Release(This->xapo);
        if(This->xapo_params)
            IXAPOParameters_Release(This->xapo_params);
        heap_free(This);
    }
    return r;
}

static uint32_t FAPOCALL XAPO_GetRegistrationProperties(void *iface,
        FAPORegistrationProperties **ppRegistrationProperties)
{
    XA2XAPOImpl *This = impl_from_FAPO(iface);
    XAPO_REGISTRATION_PROPERTIES *xprops;
    HRESULT hr;

    TRACE("%p\n", This);

    hr = IXAPO_GetRegistrationProperties(This->xapo, &xprops);
    if(FAILED(hr))
        return hr;

    *ppRegistrationProperties = heap_alloc(sizeof(FAPORegistrationProperties));
    memcpy(*ppRegistrationProperties, xprops, sizeof(FAPORegistrationProperties));
    CoTaskMemFree(xprops);

    return 0;
}

static uint32_t FAPOCALL XAPO_IsInputFormatSupported(void *iface,
        const FAudioWaveFormatEx *pOutputFormat, const FAudioWaveFormatEx *pRequestedInputFormat,
        FAudioWaveFormatEx **ppSupportedInputFormat)
{
    XA2XAPOImpl *This = impl_from_FAPO(iface);
    TRACE("%p\n", This);
    return IXAPO_IsInputFormatSupported(This->xapo, (const WAVEFORMATEX*)pOutputFormat,
            (const WAVEFORMATEX*)pRequestedInputFormat, (WAVEFORMATEX**)ppSupportedInputFormat);
}

static uint32_t FAPOCALL XAPO_IsOutputFormatSupported(void *iface,
        const FAudioWaveFormatEx *pInputFormat, const FAudioWaveFormatEx *pRequestedOutputFormat,
        FAudioWaveFormatEx **ppSupportedOutputFormat)
{
    XA2XAPOImpl *This = impl_from_FAPO(iface);
    TRACE("%p\n", This);
    return IXAPO_IsOutputFormatSupported(This->xapo, (const WAVEFORMATEX *)pInputFormat,
            (const WAVEFORMATEX *)pRequestedOutputFormat, (WAVEFORMATEX**)ppSupportedOutputFormat);
}

static uint32_t FAPOCALL XAPO_Initialize(void *iface, const void *pData,
        uint32_t DataByteSize)
{
    XA2XAPOImpl *This = impl_from_FAPO(iface);
    TRACE("%p\n", This);
    return IXAPO_Initialize(This->xapo, pData, DataByteSize);
}

static void FAPOCALL XAPO_Reset(void *iface)
{
    XA2XAPOImpl *This = impl_from_FAPO(iface);
    TRACE("%p\n", This);
    IXAPO_Reset(This->xapo);
}

static uint32_t FAPOCALL XAPO_LockForProcess(void *iface,
        uint32_t InputLockedParameterCount,
        const FAPOLockForProcessBufferParameters *pInputLockedParameters,
        uint32_t OutputLockedParameterCount,
        const FAPOLockForProcessBufferParameters *pOutputLockedParameters)
{
    XA2XAPOImpl *This = impl_from_FAPO(iface);
    TRACE("%p\n", This);
    return IXAPO_LockForProcess(This->xapo,
            InputLockedParameterCount,
            (const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS *)pInputLockedParameters,
            OutputLockedParameterCount,
            (const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS *)pOutputLockedParameters);
}

static void FAPOCALL XAPO_UnlockForProcess(void *iface)
{
    XA2XAPOImpl *This = impl_from_FAPO(iface);
    TRACE("%p\n", This);
    IXAPO_UnlockForProcess(This->xapo);
}

static void FAPOCALL XAPO_Process(void *iface,
        uint32_t InputProcessParameterCount,
        const FAPOProcessBufferParameters* pInputProcessParameters,
        uint32_t OutputProcessParameterCount,
        FAPOProcessBufferParameters* pOutputProcessParameters,
        uint8_t IsEnabled)
{
    XA2XAPOImpl *This = impl_from_FAPO(iface);
    TRACE("%p\n", This);
    IXAPO_Process(This->xapo, InputProcessParameterCount,
            (const XAPO_PROCESS_BUFFER_PARAMETERS *)pInputProcessParameters,
            OutputProcessParameterCount,
            (XAPO_PROCESS_BUFFER_PARAMETERS *)pOutputProcessParameters,
            IsEnabled);
}

static uint32_t FAPOCALL XAPO_CalcInputFrames(void *iface,
        uint32_t OutputFrameCount)
{
    XA2XAPOImpl *This = impl_from_FAPO(iface);
    TRACE("%p\n", This);
    return IXAPO_CalcInputFrames(This->xapo, OutputFrameCount);
}

static uint32_t FAPOCALL XAPO_CalcOutputFrames(void *iface,
        uint32_t InputFrameCount)
{
    XA2XAPOImpl *This = impl_from_FAPO(iface);
    TRACE("%p\n", This);
    return IXAPO_CalcOutputFrames(This->xapo, InputFrameCount);
}

static void FAPOCALL XAPO_SetParameters(void *iface,
        const void *pParameters, uint32_t ParametersByteSize)
{
    XA2XAPOImpl *This = impl_from_FAPO(iface);
    TRACE("%p\n", This);
    if(This->xapo_params)
        IXAPOParameters_SetParameters(This->xapo_params, pParameters, ParametersByteSize);
}

static void FAPOCALL XAPO_GetParameters(void *iface,
        void *pParameters, uint32_t ParametersByteSize)
{
    XA2XAPOImpl *This = impl_from_FAPO(iface);
    TRACE("%p\n", This);
    if(This->xapo_params)
        IXAPOParameters_GetParameters(This->xapo_params, pParameters, ParametersByteSize);
    else
        memset(pParameters, 0, ParametersByteSize);
}

static const FAPO FAPO_Vtbl = {
    XAPO_AddRef,
    XAPO_Release,
    XAPO_GetRegistrationProperties,
    XAPO_IsInputFormatSupported,
    XAPO_IsOutputFormatSupported,
    XAPO_Initialize,
    XAPO_Reset,
    XAPO_LockForProcess,
    XAPO_UnlockForProcess,
    XAPO_Process,
    XAPO_CalcInputFrames,
    XAPO_CalcOutputFrames,
    XAPO_SetParameters,
    XAPO_GetParameters,
};

static XA2XAPOImpl *wrap_xapo(IUnknown *unk)
{
    XA2XAPOImpl *ret;
    IXAPO *xapo;
    IXAPOParameters *xapo_params;
    HRESULT hr;

#if XAUDIO2_VER <= 7
    hr = IUnknown_QueryInterface(unk, &IID_IXAPO27, (void**)&xapo);
#else
    hr = IUnknown_QueryInterface(unk, &IID_IXAPO, (void**)&xapo);
#endif
    if(FAILED(hr)){
        WARN("XAPO doesn't support IXAPO? %p\n", unk);
        return NULL;
    }

#if XAUDIO2_VER <= 7
    hr = IUnknown_QueryInterface(unk, &IID_IXAPO27Parameters, (void**)&xapo_params);
#else
    hr = IUnknown_QueryInterface(unk, &IID_IXAPOParameters, (void**)&xapo_params);
#endif
    if(FAILED(hr)){
        TRACE("XAPO doesn't support IXAPOParameters %p\n", unk);
        xapo_params = NULL;
    }

    ret = heap_alloc(sizeof(*ret));

    ret->xapo = xapo;
    ret->xapo_params = xapo_params;
    ret->FAPO_vtbl = FAPO_Vtbl;
    ret->ref = 1;

    TRACE("wrapped IXAPO %p with %p\n", xapo, ret);

    return ret;
}

FAudioEffectChain *wrap_effect_chain(const XAUDIO2_EFFECT_CHAIN *pEffectChain)
{
    FAudioEffectChain *ret;
    int i;

    if(!pEffectChain)
        return NULL;

    ret = heap_alloc(sizeof(*ret) + sizeof(FAudioEffectDescriptor) * pEffectChain->EffectCount);

    ret->EffectCount = pEffectChain->EffectCount;
    ret->pEffectDescriptors = (void*)(ret + 1);

    for(i = 0; i < ret->EffectCount; ++i){
        ret->pEffectDescriptors[i].pEffect = &wrap_xapo(pEffectChain->pEffectDescriptors[i].pEffect)->FAPO_vtbl;
        ret->pEffectDescriptors[i].InitialState = pEffectChain->pEffectDescriptors[i].InitialState;
        ret->pEffectDescriptors[i].OutputChannels = pEffectChain->pEffectDescriptors[i].OutputChannels;
    }

    return ret;
}

void free_effect_chain(FAudioEffectChain *chain)
{
    int i;
    if(!chain)
        return;
    for(i = 0; i < chain->EffectCount; ++i)
        XAPO_Release(chain->pEffectDescriptors[i].pEffect);
    heap_free(chain);
}
