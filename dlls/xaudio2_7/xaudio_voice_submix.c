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

XA2VoiceImpl *impl_from_IXAudio2SubmixVoice(IXAudio2SubmixVoice *iface)
{
    return CONTAINING_RECORD(iface, XA2VoiceImpl, IXAudio2SubmixVoice_iface);
}

static void WINAPI XA2SUB_GetVoiceDetails(IXAudio2SubmixVoice *iface,
        XAUDIO2_VOICE_DETAILS *pVoiceDetails)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SubmixVoice(iface);
    TRACE("%p, %p\n", This, pVoiceDetails);
    FAudioVoice_GetVoiceDetails(This->faudio_voice, (FAudioVoiceDetails *)pVoiceDetails);
}

static HRESULT WINAPI XA2SUB_SetOutputVoices(IXAudio2SubmixVoice *iface,
        const XAUDIO2_VOICE_SENDS *pSendList)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SubmixVoice(iface);
    FAudioVoiceSends *faudio_sends;
    HRESULT hr;

    TRACE("%p, %p\n", This, pSendList);

    faudio_sends = wrap_voice_sends(pSendList);

    hr = FAudioVoice_SetOutputVoices(This->faudio_voice, faudio_sends);

    free_voice_sends(faudio_sends);

    return hr;
}

static HRESULT WINAPI XA2SUB_SetEffectChain(IXAudio2SubmixVoice *iface,
        const XAUDIO2_EFFECT_CHAIN *pEffectChain)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SubmixVoice(iface);
    HRESULT hr;

    TRACE("%p, %p\n", This, pEffectChain);

    free_effect_chain(This->effect_chain);
    This->effect_chain = wrap_effect_chain(pEffectChain);

    hr = FAudioVoice_SetEffectChain(This->faudio_voice, This->effect_chain);

    return hr;
}

static HRESULT WINAPI XA2SUB_EnableEffect(IXAudio2SubmixVoice *iface, UINT32 EffectIndex,
        UINT32 OperationSet)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SubmixVoice(iface);
    TRACE("%p, %u, 0x%x\n", This, EffectIndex, OperationSet);
    return FAudioVoice_EnableEffect(This->faudio_voice, EffectIndex, OperationSet);
}

static HRESULT WINAPI XA2SUB_DisableEffect(IXAudio2SubmixVoice *iface, UINT32 EffectIndex,
        UINT32 OperationSet)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SubmixVoice(iface);
    TRACE("%p, %u, 0x%x\n", This, EffectIndex, OperationSet);
    return FAudioVoice_DisableEffect(This->faudio_voice, EffectIndex, OperationSet);
}

static void WINAPI XA2SUB_GetEffectState(IXAudio2SubmixVoice *iface, UINT32 EffectIndex,
        BOOL *pEnabled)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SubmixVoice(iface);
    uint8_t result;
    TRACE("%p, %u, %p\n", This, EffectIndex, pEnabled);
    FAudioVoice_GetEffectState(This->faudio_voice, EffectIndex, &result);
    *pEnabled = result;
}

static HRESULT WINAPI XA2SUB_SetEffectParameters(IXAudio2SubmixVoice *iface,
        UINT32 EffectIndex, const void *pParameters, UINT32 ParametersByteSize,
        UINT32 OperationSet)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SubmixVoice(iface);
    TRACE("%p, %u, %p, 0x%x, 0x%x\n", This, EffectIndex, pParameters,
            ParametersByteSize, OperationSet);
    return FAudioVoice_SetEffectParameters(This->faudio_voice, EffectIndex,
            pParameters, ParametersByteSize, OperationSet);
}

static HRESULT WINAPI XA2SUB_GetEffectParameters(IXAudio2SubmixVoice *iface,
        UINT32 EffectIndex, void *pParameters, UINT32 ParametersByteSize)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SubmixVoice(iface);
    TRACE("%p, %u, %p, 0x%x\n", This, EffectIndex, pParameters,
            ParametersByteSize);
    return FAudioVoice_GetEffectParameters(This->faudio_voice, EffectIndex,
            pParameters, ParametersByteSize);
}

static HRESULT WINAPI XA2SUB_SetFilterParameters(IXAudio2SubmixVoice *iface,
        const XAUDIO2_FILTER_PARAMETERS *pParameters, UINT32 OperationSet)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SubmixVoice(iface);
    TRACE("%p, %p, 0x%x\n", This, pParameters, OperationSet);
    return FAudioVoice_SetFilterParameters(This->faudio_voice, (const FAudioFilterParameters *)pParameters,
            OperationSet);
}

static void WINAPI XA2SUB_GetFilterParameters(IXAudio2SubmixVoice *iface,
        XAUDIO2_FILTER_PARAMETERS *pParameters)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SubmixVoice(iface);
    TRACE("%p, %p\n", This, pParameters);
    FAudioVoice_GetFilterParameters(This->faudio_voice, (FAudioFilterParameters *)pParameters);
}

static HRESULT WINAPI XA2SUB_SetOutputFilterParameters(IXAudio2SubmixVoice *iface,
        IXAudio2Voice *pDestinationVoice,
        const XAUDIO2_FILTER_PARAMETERS *pParameters, UINT32 OperationSet)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SubmixVoice(iface);
    XA2VoiceImpl *dst = pDestinationVoice ? impl_from_IXAudio2Voice(pDestinationVoice) : NULL;

    TRACE("%p, %p, %p, 0x%x\n", This, pDestinationVoice, pParameters, OperationSet);

    return FAudioVoice_SetOutputFilterParameters(This->faudio_voice,
            dst ? dst->faudio_voice : NULL, (const FAudioFilterParameters *)pParameters, OperationSet);
}

static void WINAPI XA2SUB_GetOutputFilterParameters(IXAudio2SubmixVoice *iface,
        IXAudio2Voice *pDestinationVoice,
        XAUDIO2_FILTER_PARAMETERS *pParameters)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SubmixVoice(iface);
    XA2VoiceImpl *dst = pDestinationVoice ? impl_from_IXAudio2Voice(pDestinationVoice) : NULL;

    TRACE("%p, %p, %p\n", This, pDestinationVoice, pParameters);

    FAudioVoice_GetOutputFilterParameters(This->faudio_voice,
            dst ? dst->faudio_voice : NULL, (FAudioFilterParameters *)pParameters);
}

static HRESULT WINAPI XA2SUB_SetVolume(IXAudio2SubmixVoice *iface, float Volume,
        UINT32 OperationSet)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SubmixVoice(iface);
    TRACE("%p, %f, 0x%x\n", This, Volume, OperationSet);
    return FAudioVoice_SetVolume(This->faudio_voice, Volume, OperationSet);
}

static void WINAPI XA2SUB_GetVolume(IXAudio2SubmixVoice *iface, float *pVolume)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SubmixVoice(iface);
    TRACE("%p, %p\n", This, pVolume);
    return FAudioVoice_GetVolume(This->faudio_voice, pVolume);
}

static HRESULT WINAPI XA2SUB_SetChannelVolumes(IXAudio2SubmixVoice *iface, UINT32 Channels,
        const float *pVolumes, UINT32 OperationSet)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SubmixVoice(iface);
    TRACE("%p, %u, %p, 0x%x\n", This, Channels, pVolumes, OperationSet);
    return FAudioVoice_SetChannelVolumes(This->faudio_voice, Channels,
            pVolumes, OperationSet);
}

static void WINAPI XA2SUB_GetChannelVolumes(IXAudio2SubmixVoice *iface, UINT32 Channels,
        float *pVolumes)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SubmixVoice(iface);
    TRACE("%p, %u, %p\n", This, Channels, pVolumes);
    return FAudioVoice_GetChannelVolumes(This->faudio_voice, Channels,
            pVolumes);
}

static HRESULT WINAPI XA2SUB_SetOutputMatrix(IXAudio2SubmixVoice *iface,
        IXAudio2Voice *pDestinationVoice, UINT32 SourceChannels,
        UINT32 DestinationChannels, const float *pLevelMatrix,
        UINT32 OperationSet)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SubmixVoice(iface);
    XA2VoiceImpl *dst = pDestinationVoice ? impl_from_IXAudio2Voice(pDestinationVoice) : NULL;

    TRACE("%p, %p, %u, %u, %p, 0x%x\n", This, pDestinationVoice,
            SourceChannels, DestinationChannels, pLevelMatrix, OperationSet);

    return FAudioVoice_SetOutputMatrix(This->faudio_voice, dst ? dst->faudio_voice : NULL,
            SourceChannels, DestinationChannels, pLevelMatrix, OperationSet);
}

static void WINAPI XA2SUB_GetOutputMatrix(IXAudio2SubmixVoice *iface,
        IXAudio2Voice *pDestinationVoice, UINT32 SourceChannels,
        UINT32 DestinationChannels, float *pLevelMatrix)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SubmixVoice(iface);
    XA2VoiceImpl *dst = pDestinationVoice ? impl_from_IXAudio2Voice(pDestinationVoice) : NULL;

    TRACE("%p, %p, %u, %u, %p\n", This, pDestinationVoice,
            SourceChannels, DestinationChannels, pLevelMatrix);

    FAudioVoice_GetOutputMatrix(This->faudio_voice, dst ? dst->faudio_voice : NULL,
            SourceChannels, DestinationChannels, pLevelMatrix);
}

static void WINAPI XA2SUB_DestroyVoice(IXAudio2SubmixVoice *iface)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SubmixVoice(iface);

    TRACE("%p\n", This);

    EnterCriticalSection(&This->lock);

    destroy_voice(This);

    LeaveCriticalSection(&This->lock);
}

const struct IXAudio2SubmixVoiceVtbl XAudio2SubmixVoice_Vtbl = {
    XA2SUB_GetVoiceDetails,
    XA2SUB_SetOutputVoices,
    XA2SUB_SetEffectChain,
    XA2SUB_EnableEffect,
    XA2SUB_DisableEffect,
    XA2SUB_GetEffectState,
    XA2SUB_SetEffectParameters,
    XA2SUB_GetEffectParameters,
    XA2SUB_SetFilterParameters,
    XA2SUB_GetFilterParameters,
    XA2SUB_SetOutputFilterParameters,
    XA2SUB_GetOutputFilterParameters,
    XA2SUB_SetVolume,
    XA2SUB_GetVolume,
    XA2SUB_SetChannelVolumes,
    XA2SUB_GetChannelVolumes,
    XA2SUB_SetOutputMatrix,
    XA2SUB_GetOutputMatrix,
    XA2SUB_DestroyVoice
};
