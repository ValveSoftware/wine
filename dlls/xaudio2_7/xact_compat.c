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

#if XACT3_VER <= 4
WINE_DEFAULT_DEBUG_CHANNEL(xact3);

XACT3CueImpl *impl_from_IXACT34Cue(IXACT34Cue *iface)
{
    return CONTAINING_RECORD(iface, XACT3CueImpl, IXACT34Cue_iface);
}

static HRESULT WINAPI IXACT34CueImpl_Play(IXACT34Cue *iface)
{
    XACT3CueImpl *This = impl_from_IXACT34Cue(iface);

    TRACE("(%p)\n", iface);

    return FACTCue_Play(This->fact_cue);
}

static HRESULT WINAPI IXACT34CueImpl_Stop(IXACT34Cue *iface, DWORD dwFlags)
{
    XACT3CueImpl *This = impl_from_IXACT34Cue(iface);

    TRACE("(%p)->(%u)\n", iface, dwFlags);

    return FACTCue_Stop(This->fact_cue, dwFlags);
}

static HRESULT WINAPI IXACT34CueImpl_GetState(IXACT34Cue *iface, DWORD *pdwState)
{
    XACT3CueImpl *This = impl_from_IXACT34Cue(iface);

    TRACE("(%p)->(%p)\n", iface, pdwState);

    return FACTCue_GetState(This->fact_cue, pdwState);
}

static HRESULT WINAPI IXACT34CueImpl_Destroy(IXACT34Cue *iface)
{
    XACT3CueImpl *This = impl_from_IXACT34Cue(iface);
    HRESULT hr;

    TRACE("(%p)\n", iface);

    hr = FACTCue_Destroy(This->fact_cue);
    HeapFree(GetProcessHeap(), 0, This);
    return hr;
}

static HRESULT WINAPI IXACT34CueImpl_SetMatrixCoefficients(IXACT34Cue *iface,
        UINT32 uSrcChannelCount, UINT32 uDstChannelCount,
        float *pMatrixCoefficients)
{
    XACT3CueImpl *This = impl_from_IXACT34Cue(iface);

    TRACE("(%p)->(%u, %u, %p)\n", iface, uSrcChannelCount, uDstChannelCount,
            pMatrixCoefficients);

    return FACTCue_SetMatrixCoefficients(This->fact_cue, uSrcChannelCount,
        uDstChannelCount, pMatrixCoefficients);
}

static XACTVARIABLEINDEX WINAPI IXACT34CueImpl_GetVariableIndex(IXACT34Cue *iface,
        PCSTR szFriendlyName)
{
    XACT3CueImpl *This = impl_from_IXACT34Cue(iface);

    TRACE("(%p)->(%s)\n", iface, szFriendlyName);

    return FACTCue_GetVariableIndex(This->fact_cue, szFriendlyName);
}

static HRESULT WINAPI IXACT34CueImpl_SetVariable(IXACT34Cue *iface,
        XACTVARIABLEINDEX nIndex, XACTVARIABLEVALUE nValue)
{
    XACT3CueImpl *This = impl_from_IXACT34Cue(iface);

    TRACE("(%p)->(%u, %f)\n", iface, nIndex, nValue);

    return FACTCue_SetVariable(This->fact_cue, nIndex, nValue);
}

static HRESULT WINAPI IXACT34CueImpl_GetVariable(IXACT34Cue *iface,
        XACTVARIABLEINDEX nIndex, XACTVARIABLEVALUE *nValue)
{
    XACT3CueImpl *This = impl_from_IXACT34Cue(iface);

    TRACE("(%p)->(%u, %p)\n", iface, nIndex, nValue);

    return FACTCue_GetVariable(This->fact_cue, nIndex, nValue);
}

static HRESULT WINAPI IXACT34CueImpl_Pause(IXACT34Cue *iface, BOOL fPause)
{
    XACT3CueImpl *This = impl_from_IXACT34Cue(iface);

    TRACE("(%p)->(%u)\n", iface, fPause);

    return FACTCue_Pause(This->fact_cue, fPause);
}

static HRESULT WINAPI IXACT34CueImpl_GetProperties(IXACT34Cue *iface,
        XACT_CUE_INSTANCE_PROPERTIES **ppProperties)
{
    XACT3CueImpl *This = impl_from_IXACT34Cue(iface);
    FACTCueInstanceProperties *fProps;
    HRESULT hr;

    TRACE("(%p)->(%p)\n", iface, ppProperties);

    hr = FACTCue_GetProperties(This->fact_cue, &fProps);
    if(FAILED(hr))
        return hr;

    *ppProperties = (XACT_CUE_INSTANCE_PROPERTIES*) fProps;
    return hr;
}

const IXACT34CueVtbl XACT34Cue_Vtbl =
{
    IXACT34CueImpl_Play,
    IXACT34CueImpl_Stop,
    IXACT34CueImpl_GetState,
    IXACT34CueImpl_Destroy,
    IXACT34CueImpl_SetMatrixCoefficients,
    IXACT34CueImpl_GetVariableIndex,
    IXACT34CueImpl_SetVariable,
    IXACT34CueImpl_GetVariable,
    IXACT34CueImpl_Pause,
    IXACT34CueImpl_GetProperties
};
#endif
