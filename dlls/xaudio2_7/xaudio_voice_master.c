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

XA2VoiceImpl *impl_from_IXAudio2MasteringVoice(IXAudio2MasteringVoice *iface)
{
    return CONTAINING_RECORD(iface, XA2VoiceImpl, IXAudio2MasteringVoice_iface);
}

static void WINAPI XA2M_GetVoiceDetails(IXAudio2MasteringVoice *iface,
        XAUDIO2_VOICE_DETAILS *pVoiceDetails)
{
    XA2VoiceImpl *This = impl_from_IXAudio2MasteringVoice(iface);
    TRACE("%p, %p\n", This, pVoiceDetails);
    FAudioVoice_GetVoiceDetails(This->faudio_voice, (FAudioVoiceDetails *)pVoiceDetails);
}

static HRESULT WINAPI XA2M_SetOutputVoices(IXAudio2MasteringVoice *iface,
        const XAUDIO2_VOICE_SENDS *pSendList)
{
    XA2VoiceImpl *This = impl_from_IXAudio2MasteringVoice(iface);
    FAudioVoiceSends *faudio_sends;
    HRESULT hr;

    TRACE("%p, %p\n", This, pSendList);

    faudio_sends = wrap_voice_sends(pSendList);

    hr = FAudioVoice_SetOutputVoices(This->faudio_voice, faudio_sends);

    free_voice_sends(faudio_sends);

    return hr;
}

static HRESULT WINAPI XA2M_SetEffectChain(IXAudio2MasteringVoice *iface,
        const XAUDIO2_EFFECT_CHAIN *pEffectChain)
{
    XA2VoiceImpl *This = impl_from_IXAudio2MasteringVoice(iface);
    HRESULT hr;

    TRACE("%p, %p\n", This, pEffectChain);

    free_effect_chain(This->effect_chain);
    This->effect_chain = wrap_effect_chain(pEffectChain);

    hr = FAudioVoice_SetEffectChain(This->faudio_voice, This->effect_chain);

    return hr;
}

static HRESULT WINAPI XA2M_EnableEffect(IXAudio2MasteringVoice *iface, UINT32 EffectIndex,
        UINT32 OperationSet)
{
    XA2VoiceImpl *This = impl_from_IXAudio2MasteringVoice(iface);
    TRACE("%p, %u, 0x%x\n", This, EffectIndex, OperationSet);
    return FAudioVoice_EnableEffect(This->faudio_voice, EffectIndex, OperationSet);
}

static HRESULT WINAPI XA2M_DisableEffect(IXAudio2MasteringVoice *iface, UINT32 EffectIndex,
        UINT32 OperationSet)
{
    XA2VoiceImpl *This = impl_from_IXAudio2MasteringVoice(iface);
    TRACE("%p, %u, 0x%x\n", This, EffectIndex, OperationSet);
    return FAudioVoice_DisableEffect(This->faudio_voice, EffectIndex, OperationSet);
}

static void WINAPI XA2M_GetEffectState(IXAudio2MasteringVoice *iface, UINT32 EffectIndex,
        BOOL *pEnabled)
{
    XA2VoiceImpl *This = impl_from_IXAudio2MasteringVoice(iface);
    uint8_t result;
    TRACE("%p, %u, %p\n", This, EffectIndex, pEnabled);
    FAudioVoice_GetEffectState(This->faudio_voice, EffectIndex, &result);
    *pEnabled = result;
}

static HRESULT WINAPI XA2M_SetEffectParameters(IXAudio2MasteringVoice *iface,
        UINT32 EffectIndex, const void *pParameters, UINT32 ParametersByteSize,
        UINT32 OperationSet)
{
    XA2VoiceImpl *This = impl_from_IXAudio2MasteringVoice(iface);
    TRACE("%p, %u, %p, 0x%x, 0x%x\n", This, EffectIndex, pParameters,
            ParametersByteSize, OperationSet);
    return FAudioVoice_SetEffectParameters(This->faudio_voice, EffectIndex,
            pParameters, ParametersByteSize, OperationSet);
}

static HRESULT WINAPI XA2M_GetEffectParameters(IXAudio2MasteringVoice *iface,
        UINT32 EffectIndex, void *pParameters, UINT32 ParametersByteSize)
{
    XA2VoiceImpl *This = impl_from_IXAudio2MasteringVoice(iface);
    TRACE("%p, %u, %p, 0x%x\n", This, EffectIndex, pParameters,
            ParametersByteSize);
    return FAudioVoice_GetEffectParameters(This->faudio_voice, EffectIndex,
            pParameters, ParametersByteSize);
}

static HRESULT WINAPI XA2M_SetFilterParameters(IXAudio2MasteringVoice *iface,
        const XAUDIO2_FILTER_PARAMETERS *pParameters, UINT32 OperationSet)
{
    XA2VoiceImpl *This = impl_from_IXAudio2MasteringVoice(iface);
    TRACE("%p, %p, 0x%x\n", This, pParameters, OperationSet);
    return FAudioVoice_SetFilterParameters(This->faudio_voice, (const FAudioFilterParameters *)pParameters,
            OperationSet);
}

static void WINAPI XA2M_GetFilterParameters(IXAudio2MasteringVoice *iface,
        XAUDIO2_FILTER_PARAMETERS *pParameters)
{
    XA2VoiceImpl *This = impl_from_IXAudio2MasteringVoice(iface);
    TRACE("%p, %p\n", This, pParameters);
    FAudioVoice_GetFilterParameters(This->faudio_voice, (FAudioFilterParameters *)pParameters);
}

static HRESULT WINAPI XA2M_SetOutputFilterParameters(IXAudio2MasteringVoice *iface,
        IXAudio2Voice *pDestinationVoice,
        const XAUDIO2_FILTER_PARAMETERS *pParameters, UINT32 OperationSet)
{
    XA2VoiceImpl *This = impl_from_IXAudio2MasteringVoice(iface);
    XA2VoiceImpl *dst = pDestinationVoice ? impl_from_IXAudio2Voice(pDestinationVoice) : NULL;

    TRACE("%p, %p, %p, 0x%x\n", This, pDestinationVoice, pParameters, OperationSet);

    return FAudioVoice_SetOutputFilterParameters(This->faudio_voice,
            dst ? dst->faudio_voice : NULL, (const FAudioFilterParameters *)pParameters, OperationSet);
}

static void WINAPI XA2M_GetOutputFilterParameters(IXAudio2MasteringVoice *iface,
        IXAudio2Voice *pDestinationVoice,
        XAUDIO2_FILTER_PARAMETERS *pParameters)
{
    XA2VoiceImpl *This = impl_from_IXAudio2MasteringVoice(iface);
    XA2VoiceImpl *dst = pDestinationVoice ? impl_from_IXAudio2Voice(pDestinationVoice) : NULL;

    TRACE("%p, %p, %p\n", This, pDestinationVoice, pParameters);

    FAudioVoice_GetOutputFilterParameters(This->faudio_voice,
            dst ? dst->faudio_voice : NULL, (FAudioFilterParameters *)pParameters);
}

static HRESULT WINAPI XA2M_SetVolume(IXAudio2MasteringVoice *iface, float Volume,
        UINT32 OperationSet)
{
    XA2VoiceImpl *This = impl_from_IXAudio2MasteringVoice(iface);
    TRACE("%p, %f, 0x%x\n", This, Volume, OperationSet);
    return FAudioVoice_SetVolume(This->faudio_voice, Volume, OperationSet);
}

static void WINAPI XA2M_GetVolume(IXAudio2MasteringVoice *iface, float *pVolume)
{
    XA2VoiceImpl *This = impl_from_IXAudio2MasteringVoice(iface);
    TRACE("%p, %p\n", This, pVolume);
    return FAudioVoice_GetVolume(This->faudio_voice, pVolume);
}

static HRESULT WINAPI XA2M_SetChannelVolumes(IXAudio2MasteringVoice *iface, UINT32 Channels,
        const float *pVolumes, UINT32 OperationSet)
{
    XA2VoiceImpl *This = impl_from_IXAudio2MasteringVoice(iface);
    TRACE("%p, %u, %p, 0x%x\n", This, Channels, pVolumes, OperationSet);
    return FAudioVoice_SetChannelVolumes(This->faudio_voice, Channels,
            pVolumes, OperationSet);
}

static void WINAPI XA2M_GetChannelVolumes(IXAudio2MasteringVoice *iface, UINT32 Channels,
        float *pVolumes)
{
    XA2VoiceImpl *This = impl_from_IXAudio2MasteringVoice(iface);
    TRACE("%p, %u, %p\n", This, Channels, pVolumes);
    return FAudioVoice_GetChannelVolumes(This->faudio_voice, Channels,
            pVolumes);
}

static HRESULT WINAPI XA2M_SetOutputMatrix(IXAudio2MasteringVoice *iface,
        IXAudio2Voice *pDestinationVoice, UINT32 SourceChannels,
        UINT32 DestinationChannels, const float *pLevelMatrix,
        UINT32 OperationSet)
{
    XA2VoiceImpl *This = impl_from_IXAudio2MasteringVoice(iface);
    XA2VoiceImpl *dst = pDestinationVoice ? impl_from_IXAudio2Voice(pDestinationVoice) : NULL;

    TRACE("%p, %p, %u, %u, %p, 0x%x\n", This, pDestinationVoice,
            SourceChannels, DestinationChannels, pLevelMatrix, OperationSet);

    return FAudioVoice_SetOutputMatrix(This->faudio_voice, dst ? dst->faudio_voice : NULL,
            SourceChannels, DestinationChannels, pLevelMatrix, OperationSet);
}

static void WINAPI XA2M_GetOutputMatrix(IXAudio2MasteringVoice *iface,
        IXAudio2Voice *pDestinationVoice, UINT32 SourceChannels,
        UINT32 DestinationChannels, float *pLevelMatrix)
{
    XA2VoiceImpl *This = impl_from_IXAudio2MasteringVoice(iface);
    XA2VoiceImpl *dst = pDestinationVoice ? impl_from_IXAudio2Voice(pDestinationVoice) : NULL;

    TRACE("%p, %p, %u, %u, %p\n", This, pDestinationVoice,
            SourceChannels, DestinationChannels, pLevelMatrix);

    FAudioVoice_GetOutputMatrix(This->faudio_voice, dst ? dst->faudio_voice : NULL,
            SourceChannels, DestinationChannels, pLevelMatrix);
}

static void WINAPI XA2M_DestroyVoice(IXAudio2MasteringVoice *iface)
{
    XA2VoiceImpl *This = impl_from_IXAudio2MasteringVoice(iface);

    TRACE("%p\n", This);

    EnterCriticalSection(&This->lock);

    destroy_voice(This);

    LeaveCriticalSection(&This->lock);
}

static void WINAPI XA2M_GetChannelMask(IXAudio2MasteringVoice *iface,
        DWORD *pChannelMask)
{
    XA2VoiceImpl *This = impl_from_IXAudio2MasteringVoice(iface);

    TRACE("%p, %p\n", This, pChannelMask);

    FAudioMasteringVoice_GetChannelMask(This->faudio_voice, pChannelMask);
}

const struct IXAudio2MasteringVoiceVtbl XAudio2MasteringVoice_Vtbl = {
    XA2M_GetVoiceDetails,
    XA2M_SetOutputVoices,
    XA2M_SetEffectChain,
    XA2M_EnableEffect,
    XA2M_DisableEffect,
    XA2M_GetEffectState,
    XA2M_SetEffectParameters,
    XA2M_GetEffectParameters,
    XA2M_SetFilterParameters,
    XA2M_GetFilterParameters,
    XA2M_SetOutputFilterParameters,
    XA2M_GetOutputFilterParameters,
    XA2M_SetVolume,
    XA2M_GetVolume,
    XA2M_SetChannelVolumes,
    XA2M_GetChannelVolumes,
    XA2M_SetOutputMatrix,
    XA2M_GetOutputMatrix,
    XA2M_DestroyVoice,
    XA2M_GetChannelMask
};
