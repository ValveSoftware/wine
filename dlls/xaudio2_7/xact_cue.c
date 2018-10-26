/*
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

#include "xact_private.h"

#include "ole2.h"
#include "rpcproxy.h"

#include "wine/debug.h"
#include "wine/heap.h"

WINE_DEFAULT_DEBUG_CHANNEL(xact3);

static inline XACT3CueImpl *impl_from_IXACT34Cue(IXACT34Cue *iface)
{
    return CONTAINING_RECORD(iface, IXACT3CueImpl, IXACT34Cue_iface);
}

static inline XACT3CueImpl *impl_from_IXACT37Cue(IXACT37Cue *iface)
{
    return CONTAINING_RECORD(iface, IXACT3CueImpl, IXACT37Cue_iface);
}

XACT3CueImpl *impl_from_IXACT3Cue(IXACT3Cue *iface)
{
#if XACT3_VER <= 4
    if (iface->lpVtbl == (void*)&XACT34Cue_Vtbl)
        return impl_from_IXACT34Cue((IXACT34Cue*)iface);
#elif XACT3_VER <= 7
    if (iface->lpVtbl == (void*)&XACT37Cue_Vtbl)
        return impl_from_IXACT37Cue((IXACT37Cue*)iface);
#endif
    ERR("Invalid IXACT3Cue pointer: %p\n", iface);
    return NULL;
}

static HRESULT IXACT3CueImpl_Play(IXACT3Cue *iface)
{
    XACT3CueImpl *This = impl_from_IXACTCue(iface);

    TRACE("(%p)\n", iface);

    return FACTCue_Play(This->fact_cue);
}

static HRESULT IXACT3CueImpl_Stop(IXACT3Cue *iface, DWORD dwFlags)
{
    XACT3CueImpl *This = impl_from_IXACTCue(iface);

    TRACE("(%p)->(%u)\n", iface, dwFlags);

    return FACTCue_Play(This->fact_cue, dwFlags);
}

static HRESULT IXACT3CueImpl_GetState(IXACT3Cue *iface, DWORD *pdwState)
{
    XACT3CueImpl *This = impl_from_IXACTCue(iface);

    TRACE("(%p)->(%p)\n", iface, pdwState);

    return FACTCue_Play(This->fact_cue, pdwState);
}

static HRESULT IXACT3CueImpl_Destroy(IXACT3Cue *iface)
{
    XACT3CueImpl *This = impl_from_IXACTCue(iface);
    HRESULT hr;

    TRACE("(%p)\n", iface);

    hr = FACTCue_Destroy(This->fact_cue);
    HeapFree(GetProcessHeap(), 0, This);
    return hr;
}

static HRESULT IXACT3CueImpl_SetMatrixCoefficients(IXACT3Cue *iface,
        UINT32 uSrcChannelCount, UINT32 uDstChannelCount,
        float *pMatrixCoefficients)
{
    XACT3CueImpl *This = impl_from_IXACTCue(iface);

    TRACE("(%p)->(%u, %u, %p)\n", iface, uSrcChannelCount, uDstChannelCount,
            pMatrixCoefficients);

    return FACTCue_SetMatrixCoefficients(This->fact_cue, uSrcChannelCount,
        uDstChannelCount, pMatrixCoefficients);
}

static XACTVARIABLEINDEX IXACT3CueImpl_GetVariableIndex(IXACT3Cue *iface,
        PCSTR szFriendlyName)
{
    XACT3CueImpl *This = impl_from_IXACTCue(iface);

    TRACE("(%p)->(%s)\n", iface, szFriendlyName);

    return FACTCue_GetVariableIndex(This->fact_cue, szFriendlyName);
}

static HRESULT IXACT3CueImpl_SetVariable(IXACT3Cue *iface,
        XACTVARIABLEINDEX nIndex, XACTVARIABLEVALUE nValue)
{
    XACT3CueImpl *This = impl_from_IXACTCue(iface);

    TRACE("(%p)->(%u, %f)\n", iface, nIndex, nValue);

    return FACTCue_SetVariable(This->fact_cue, nIndex, nValue);
}

static HRESULT IXACT3CueImpl_GetVariable(IXACT3Cue *iface,
        XACTVARIABLEINDEX nIndex, XACTVARIABLEVALUE *nValue)
{
    XACT3CueImpl *This = impl_from_IXACTCue(iface);

    TRACE("(%p)->(%u, %p)\n", iface, nIndex, nValue);

    return FACTCue_GetVariable(This->fact_cue, nIndex, nValue);
}

static HRESULT IXACT3CueImpl_Pause(IXACT3Cue *iface, BOOL fPause)
{
    XACT3CueImpl *This = impl_from_IXACTCue(iface);

    TRACE("(%p)->(%u)\n", iface, fPause);

    return FACTCue_Pause(This->fact_cue, fPause);
}

static HRESULT IXACT3CueImpl_GetProperties(IXACT3Cue *iface,
        LPXACT_CUE_INSTANCE_PROPERTIES *ppProperties)
{
    XACT3CueImpl *This = impl_from_IXACTCue(iface);
    FACTCueInstanceProperties *fProps;
    HRESULT hr;

    TRACE("(%p)->(%p)\n", iface, ppProperties);

    hr = FACTCue_GetProperties(This->fact_cue, &fProps);
    if(FAILED(hr))
        return hr;

    *ppProperties = (LPXACT_CUE_INSTANCE_PROPERTIES) fProps;
    return hr;
}

static HRESULT IXACT3CueImpl_SetOutputVoices(IXACT3Cue *iface,
        const XAUDIO2_VOICE_SENDS *pSendList)
{
    XACT3CueImpl *This = impl_from_IXACTCue(iface);
    FIXME("(%p): stub!\n", This);
    return S_OK;
}

static HRESULT IXACT3CueImpl_SetOutputVoiceMatrix(IXACT3Cue *iface,
        IXAudio2Voice *pDestinationVoice, UINT32 SourceChannels,
        UINT32 DestinationChannels, const float *pLevelMatrix)
{
    XACT3CueImpl *This = impl_from_IXACTCue(iface);
    FIXME("(%p): stub!\n", This);
    return S_OK;
}

const XACT3Cue34Vtbl XACT3Cue34_Vtbl =
{
    IXACT3CueImpl_Play,
    IXACT3CueImpl_Stop,
    IXACT3CueImpl_GetState,
    IXACT3CueImpl_Destroy,
    IXACT3CueImpl_SetMatrixCoefficients,
    IXACT3CueImpl_GetVariableIndex,
    IXACT3CueImpl_SetVariable,
    IXACT3CueImpl_GetVariable,
    IXACT3CueImpl_Pause,
    IXACT3CueImpl_GetProperties
};

const XACT3Cue37Vtbl XACT3Cue37_Vtbl =
{
    IXACT3CueImpl_Play,
    IXACT3CueImpl_Stop,
    IXACT3CueImpl_GetState,
    IXACT3CueImpl_Destroy,
    IXACT3CueImpl_SetMatrixCoefficients,
    IXACT3CueImpl_GetVariableIndex,
    IXACT3CueImpl_SetVariable,
    IXACT3CueImpl_GetVariable,
    IXACT3CueImpl_Pause,
    IXACT3CueImpl_GetProperties,
    IXACT3CueImpl_SetOutputVoices,
    IXACT3CueImpl_SetOutputVoiceMatrix
};
