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

static inline XACT3WaveImpl *impl_from_IXACT3Wave(IXACT3Wave *iface)
{
    return CONTAINING_RECORD(iface, XACT3WaveImpl, IXACT3Wave_iface);
}

static HRESULT WINAPI IXACT3WaveImpl_Destroy(IXACT3Wave *iface)
{
    XACT3WaveImpl *This = impl_from_IXACT3Wave(iface);
    HRESULT hr;

    TRACE("(%p)\n", This);

    hr = FACTWave_Destroy(This->fact_wave);
    HeapFree(GetProcessHeap(), 0, This);
    return hr;
}

static HRESULT WINAPI IXACT3WaveImpl_Play(IXACT3Wave *iface)
{
    XACT3WaveImpl *This = impl_from_IXACT3Wave(iface);

    TRACE("(%p)\n", This);

    return FACTWave_Play(This->fact_wave);
}

static HRESULT WINAPI IXACT3WaveImpl_Stop(IXACT3Wave *iface, DWORD dwFlags)
{
    XACT3WaveImpl *This = impl_from_IXACT3Wave(iface);

    TRACE("(%p)->(0x%x)\n", This, dwFlags);

    return FACTWave_Stop(This->fact_wave, dwFlags);
}

static HRESULT WINAPI IXACT3WaveImpl_Pause(IXACT3Wave *iface, BOOL fPause)
{
    XACT3WaveImpl *This = impl_from_IXACT3Wave(iface);

    TRACE("(%p)->(%u)\n", This, fPause);

    return FACTWave_Pause(This->fact_wave, fPause);
}

static HRESULT WINAPI IXACT3WaveImpl_GetState(IXACT3Wave *iface, DWORD *pdwState)
{
    XACT3WaveImpl *This = impl_from_IXACT3Wave(iface);

    TRACE("(%p)->(%p)\n", This, pdwState);

    return FACTWave_GetState(This->fact_wave, pdwState);
}

static HRESULT WINAPI IXACT3WaveImpl_SetPitch(IXACT3Wave *iface, XACTPITCH pitch)
{
    XACT3WaveImpl *This = impl_from_IXACT3Wave(iface);

    TRACE("(%p)->(%d)\n", This, pitch);

    return FACTWave_SetPitch(This->fact_wave, pitch);
}

static HRESULT WINAPI IXACT3WaveImpl_SetVolume(IXACT3Wave *iface, XACTVOLUME volume)
{
    XACT3WaveImpl *This = impl_from_IXACT3Wave(iface);

    TRACE("(%p)->(%f)\n", This, volume);

    return FACTWave_SetVolume(This->fact_wave, volume);
}

static HRESULT WINAPI IXACT3WaveImpl_SetMatrixCoefficients(IXACT3Wave *iface,
        UINT32 uSrcChannelCount, UINT32 uDstChannelCount,
        float *pMatrixCoefficients)
{
    XACT3WaveImpl *This = impl_from_IXACT3Wave(iface);

    TRACE("(%p)->(%u, %u, %p)\n", This, uSrcChannelCount, uDstChannelCount,
            pMatrixCoefficients);

    return FACTWave_SetMatrixCoefficients(This->fact_wave, uSrcChannelCount,
            uDstChannelCount, pMatrixCoefficients);
}

static HRESULT WINAPI IXACT3WaveImpl_GetProperties(IXACT3Wave *iface,
    XACT_WAVE_INSTANCE_PROPERTIES *pProperties)
{
    XACT3WaveImpl *This = impl_from_IXACT3Wave(iface);

    TRACE("(%p)->(%p)\n", This, pProperties);

    return FACTWave_GetProperties(This->fact_wave,
            (FACTWaveInstanceProperties*) pProperties);
}

const IXACT3WaveVtbl XACT3Wave_Vtbl =
{
    IXACT3WaveImpl_Destroy,
    IXACT3WaveImpl_Play,
    IXACT3WaveImpl_Stop,
    IXACT3WaveImpl_Pause,
    IXACT3WaveImpl_GetState,
    IXACT3WaveImpl_SetPitch,
    IXACT3WaveImpl_SetVolume,
    IXACT3WaveImpl_SetMatrixCoefficients,
    IXACT3WaveImpl_GetProperties
};
