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

XA2VoiceImpl *impl_from_IXAudio2SourceVoice(IXAudio2SourceVoice *iface)
{
    return CONTAINING_RECORD(iface, XA2VoiceImpl, IXAudio2SourceVoice_iface);
}

static void WINAPI XA2SRC_GetVoiceDetails(IXAudio2SourceVoice *iface,
        XAUDIO2_VOICE_DETAILS *pVoiceDetails)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SourceVoice(iface);
    TRACE("%p, %p\n", This, pVoiceDetails);
    FAudioVoice_GetVoiceDetails(This->faudio_voice, (FAudioVoiceDetails *)pVoiceDetails);
}

static HRESULT WINAPI XA2SRC_SetOutputVoices(IXAudio2SourceVoice *iface,
        const XAUDIO2_VOICE_SENDS *pSendList)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SourceVoice(iface);
    FAudioVoiceSends *faudio_sends;
    HRESULT hr;

    TRACE("%p, %p\n", This, pSendList);

    faudio_sends = wrap_voice_sends(pSendList);

    hr = FAudioVoice_SetOutputVoices(This->faudio_voice, faudio_sends);

    free_voice_sends(faudio_sends);

    return hr;
}

static HRESULT WINAPI XA2SRC_SetEffectChain(IXAudio2SourceVoice *iface,
        const XAUDIO2_EFFECT_CHAIN *pEffectChain)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SourceVoice(iface);
    HRESULT hr;

    TRACE("%p, %p\n", This, pEffectChain);

    free_effect_chain(This->effect_chain);
    This->effect_chain = wrap_effect_chain(pEffectChain);

    hr = FAudioVoice_SetEffectChain(This->faudio_voice, This->effect_chain);

    return hr;
}

static HRESULT WINAPI XA2SRC_EnableEffect(IXAudio2SourceVoice *iface, UINT32 EffectIndex,
        UINT32 OperationSet)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SourceVoice(iface);
    TRACE("%p, %u, 0x%x\n", This, EffectIndex, OperationSet);
    return FAudioVoice_EnableEffect(This->faudio_voice, EffectIndex, OperationSet);
}

static HRESULT WINAPI XA2SRC_DisableEffect(IXAudio2SourceVoice *iface, UINT32 EffectIndex,
        UINT32 OperationSet)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SourceVoice(iface);
    TRACE("%p, %u, 0x%x\n", This, EffectIndex, OperationSet);
    return FAudioVoice_DisableEffect(This->faudio_voice, EffectIndex, OperationSet);
}

static void WINAPI XA2SRC_GetEffectState(IXAudio2SourceVoice *iface, UINT32 EffectIndex,
        BOOL *pEnabled)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SourceVoice(iface);
    uint8_t result;
    TRACE("%p, %u, %p\n", This, EffectIndex, pEnabled);
    FAudioVoice_GetEffectState(This->faudio_voice, EffectIndex, &result);
    *pEnabled = result;
}

static HRESULT WINAPI XA2SRC_SetEffectParameters(IXAudio2SourceVoice *iface,
        UINT32 EffectIndex, const void *pParameters, UINT32 ParametersByteSize,
        UINT32 OperationSet)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SourceVoice(iface);
    TRACE("%p, %u, %p, 0x%x, 0x%x\n", This, EffectIndex, pParameters,
            ParametersByteSize, OperationSet);
    return FAudioVoice_SetEffectParameters(This->faudio_voice, EffectIndex,
            pParameters, ParametersByteSize, OperationSet);
}

static HRESULT WINAPI XA2SRC_GetEffectParameters(IXAudio2SourceVoice *iface,
        UINT32 EffectIndex, void *pParameters, UINT32 ParametersByteSize)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SourceVoice(iface);
    TRACE("%p, %u, %p, 0x%x\n", This, EffectIndex, pParameters,
            ParametersByteSize);
    return FAudioVoice_GetEffectParameters(This->faudio_voice, EffectIndex,
            pParameters, ParametersByteSize);
}

static HRESULT WINAPI XA2SRC_SetFilterParameters(IXAudio2SourceVoice *iface,
        const XAUDIO2_FILTER_PARAMETERS *pParameters, UINT32 OperationSet)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SourceVoice(iface);
    TRACE("%p, %p, 0x%x\n", This, pParameters, OperationSet);
    return FAudioVoice_SetFilterParameters(This->faudio_voice,
            (const FAudioFilterParameters *)pParameters, OperationSet);
}

static void WINAPI XA2SRC_GetFilterParameters(IXAudio2SourceVoice *iface,
        XAUDIO2_FILTER_PARAMETERS *pParameters)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SourceVoice(iface);
    TRACE("%p, %p\n", This, pParameters);
    FAudioVoice_GetFilterParameters(This->faudio_voice, (FAudioFilterParameters *)pParameters);
}

static HRESULT WINAPI XA2SRC_SetOutputFilterParameters(IXAudio2SourceVoice *iface,
        IXAudio2Voice *pDestinationVoice,
        const XAUDIO2_FILTER_PARAMETERS *pParameters, UINT32 OperationSet)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SourceVoice(iface);
    XA2VoiceImpl *dst = pDestinationVoice ? impl_from_IXAudio2Voice(pDestinationVoice) : NULL;

    TRACE("%p, %p, %p, 0x%x\n", This, pDestinationVoice, pParameters, OperationSet);

    return FAudioVoice_SetOutputFilterParameters(This->faudio_voice,
            dst ? dst->faudio_voice : NULL, (const FAudioFilterParameters *)pParameters, OperationSet);
}

static void WINAPI XA2SRC_GetOutputFilterParameters(IXAudio2SourceVoice *iface,
        IXAudio2Voice *pDestinationVoice,
        XAUDIO2_FILTER_PARAMETERS *pParameters)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SourceVoice(iface);
    XA2VoiceImpl *dst = pDestinationVoice ? impl_from_IXAudio2Voice(pDestinationVoice) : NULL;

    TRACE("%p, %p, %p\n", This, pDestinationVoice, pParameters);

    FAudioVoice_GetOutputFilterParameters(This->faudio_voice,
            dst ? dst->faudio_voice : NULL, (FAudioFilterParameters *)pParameters);
}

static HRESULT WINAPI XA2SRC_SetVolume(IXAudio2SourceVoice *iface, float Volume,
        UINT32 OperationSet)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SourceVoice(iface);
    TRACE("%p, %f, 0x%x\n", This, Volume, OperationSet);
    return FAudioVoice_SetVolume(This->faudio_voice, Volume, OperationSet);
}

static void WINAPI XA2SRC_GetVolume(IXAudio2SourceVoice *iface, float *pVolume)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SourceVoice(iface);
    TRACE("%p, %p\n", This, pVolume);
    return FAudioVoice_GetVolume(This->faudio_voice, pVolume);
}

static HRESULT WINAPI XA2SRC_SetChannelVolumes(IXAudio2SourceVoice *iface, UINT32 Channels,
        const float *pVolumes, UINT32 OperationSet)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SourceVoice(iface);
    TRACE("%p, %u, %p, 0x%x\n", This, Channels, pVolumes, OperationSet);
    return FAudioVoice_SetChannelVolumes(This->faudio_voice, Channels,
            pVolumes, OperationSet);
}

static void WINAPI XA2SRC_GetChannelVolumes(IXAudio2SourceVoice *iface, UINT32 Channels,
        float *pVolumes)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SourceVoice(iface);
    TRACE("%p, %u, %p\n", This, Channels, pVolumes);
    return FAudioVoice_GetChannelVolumes(This->faudio_voice, Channels,
            pVolumes);
}

static HRESULT WINAPI XA2SRC_SetOutputMatrix(IXAudio2SourceVoice *iface,
        IXAudio2Voice *pDestinationVoice, UINT32 SourceChannels,
        UINT32 DestinationChannels, const float *pLevelMatrix,
        UINT32 OperationSet)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SourceVoice(iface);
    XA2VoiceImpl *dst = pDestinationVoice ? impl_from_IXAudio2Voice(pDestinationVoice) : NULL;

    TRACE("%p, %p, %u, %u, %p, 0x%x\n", This, pDestinationVoice,
            SourceChannels, DestinationChannels, pLevelMatrix, OperationSet);

    return FAudioVoice_SetOutputMatrix(This->faudio_voice, dst ? dst->faudio_voice : NULL,
            SourceChannels, DestinationChannels, pLevelMatrix, OperationSet);
}

static void WINAPI XA2SRC_GetOutputMatrix(IXAudio2SourceVoice *iface,
        IXAudio2Voice *pDestinationVoice, UINT32 SourceChannels,
        UINT32 DestinationChannels, float *pLevelMatrix)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SourceVoice(iface);
    XA2VoiceImpl *dst = pDestinationVoice ? impl_from_IXAudio2Voice(pDestinationVoice) : NULL;

    TRACE("%p, %p, %u, %u, %p\n", This, pDestinationVoice,
            SourceChannels, DestinationChannels, pLevelMatrix);

    FAudioVoice_GetOutputMatrix(This->faudio_voice, dst ? dst->faudio_voice : NULL,
            SourceChannels, DestinationChannels, pLevelMatrix);
}

static void WINAPI XA2SRC_DestroyVoice(IXAudio2SourceVoice *iface)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SourceVoice(iface);

    TRACE("%p\n", This);

    EnterCriticalSection(&This->lock);

    destroy_voice(This);

    LeaveCriticalSection(&This->lock);
}

static HRESULT WINAPI XA2SRC_Start(IXAudio2SourceVoice *iface, UINT32 Flags,
        UINT32 OperationSet)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SourceVoice(iface);

    TRACE("%p, 0x%x, 0x%x\n", This, Flags, OperationSet);

    return FAudioSourceVoice_Start(This->faudio_voice, Flags, OperationSet);
}

static HRESULT WINAPI XA2SRC_Stop(IXAudio2SourceVoice *iface, UINT32 Flags,
        UINT32 OperationSet)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SourceVoice(iface);

    TRACE("%p, 0x%x, 0x%x\n", This, Flags, OperationSet);

    return FAudioSourceVoice_Stop(This->faudio_voice, Flags, OperationSet);
}

static HRESULT WINAPI XA2SRC_SubmitSourceBuffer(IXAudio2SourceVoice *iface,
        const XAUDIO2_BUFFER *pBuffer, const XAUDIO2_BUFFER_WMA *pBufferWMA)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SourceVoice(iface);

    TRACE("%p, %p, %p\n", This, pBuffer, pBufferWMA);

    return FAudioSourceVoice_SubmitSourceBuffer(This->faudio_voice, (FAudioBuffer*)pBuffer, (FAudioBufferWMA*)pBufferWMA);
}

static HRESULT WINAPI XA2SRC_FlushSourceBuffers(IXAudio2SourceVoice *iface)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SourceVoice(iface);

    TRACE("%p\n", This);

    return FAudioSourceVoice_FlushSourceBuffers(This->faudio_voice);
}

static HRESULT WINAPI XA2SRC_Discontinuity(IXAudio2SourceVoice *iface)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SourceVoice(iface);

    TRACE("%p\n", This);

    return FAudioSourceVoice_Discontinuity(This->faudio_voice);
}

static HRESULT WINAPI XA2SRC_ExitLoop(IXAudio2SourceVoice *iface, UINT32 OperationSet)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SourceVoice(iface);

    TRACE("%p, 0x%x\n", This, OperationSet);

    return FAudioSourceVoice_ExitLoop(This->faudio_voice, OperationSet);
}

static void WINAPI XA2SRC_GetState(IXAudio2SourceVoice *iface,
        XAUDIO2_VOICE_STATE *pVoiceState, UINT32 Flags)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SourceVoice(iface);

    TRACE("%p, %p, 0x%x\n", This, pVoiceState, Flags);

    return FAudioSourceVoice_GetState(This->faudio_voice, (FAudioVoiceState*)pVoiceState, Flags);
}

static HRESULT WINAPI XA2SRC_SetFrequencyRatio(IXAudio2SourceVoice *iface,
        float Ratio, UINT32 OperationSet)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SourceVoice(iface);

    TRACE("%p, %f, 0x%x\n", This, Ratio, OperationSet);

    return FAudioSourceVoice_SetFrequencyRatio(This->faudio_voice, Ratio, OperationSet);
}

static void WINAPI XA2SRC_GetFrequencyRatio(IXAudio2SourceVoice *iface, float *pRatio)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SourceVoice(iface);

    TRACE("%p, %p\n", This, pRatio);

    return FAudioSourceVoice_GetFrequencyRatio(This->faudio_voice, pRatio);
}

static HRESULT WINAPI XA2SRC_SetSourceSampleRate(
    IXAudio2SourceVoice *iface,
    UINT32 NewSourceSampleRate)
{
    XA2VoiceImpl *This = impl_from_IXAudio2SourceVoice(iface);

    TRACE("%p, %u\n", This, NewSourceSampleRate);

    return FAudioSourceVoice_SetSourceSampleRate(This->faudio_voice, NewSourceSampleRate);
}

const IXAudio2SourceVoiceVtbl XAudio2SourceVoice_Vtbl = {
    XA2SRC_GetVoiceDetails,
    XA2SRC_SetOutputVoices,
    XA2SRC_SetEffectChain,
    XA2SRC_EnableEffect,
    XA2SRC_DisableEffect,
    XA2SRC_GetEffectState,
    XA2SRC_SetEffectParameters,
    XA2SRC_GetEffectParameters,
    XA2SRC_SetFilterParameters,
    XA2SRC_GetFilterParameters,
    XA2SRC_SetOutputFilterParameters,
    XA2SRC_GetOutputFilterParameters,
    XA2SRC_SetVolume,
    XA2SRC_GetVolume,
    XA2SRC_SetChannelVolumes,
    XA2SRC_GetChannelVolumes,
    XA2SRC_SetOutputMatrix,
    XA2SRC_GetOutputMatrix,
    XA2SRC_DestroyVoice,
    XA2SRC_Start,
    XA2SRC_Stop,
    XA2SRC_SubmitSourceBuffer,
    XA2SRC_FlushSourceBuffers,
    XA2SRC_Discontinuity,
    XA2SRC_ExitLoop,
    XA2SRC_GetState,
    XA2SRC_SetFrequencyRatio,
    XA2SRC_GetFrequencyRatio,
    XA2SRC_SetSourceSampleRate
};
