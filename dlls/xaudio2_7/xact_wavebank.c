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

static inline XACT3WaveBankImpl *impl_from_IXACT3WaveBank(IXACT3WaveBank *iface)
{
    return CONTAINING_RECORD(iface, XACT3WaveBankImpl, IXACT3WaveBank_iface);
}

static HRESULT WINAPI IXACT3WaveBankImpl_Destroy(IXACT3WaveBank *iface)
{
    XACT3WaveBankImpl *This = impl_from_IXACT3WaveBank(iface);
    HRESULT hr;

    TRACE("(%p)\n", This);

    hr = FACTWaveBank_Destroy(This->fact_wavebank);
    HeapFree(GetProcessHeap(), 0, This);
    return hr;
}

static HRESULT WINAPI IXACT3WaveBankImpl_GetNumWaves(IXACT3WaveBank *iface,
        XACTINDEX *pnNumWaves)
{
    XACT3WaveBankImpl *This = impl_from_IXACT3WaveBank(iface);

    TRACE("(%p)->(%p)\n", This, pnNumWaves);

    return FACTWaveBank_GetNumWaves(This->fact_wavebank, pnNumWaves);
}

static XACTINDEX WINAPI IXACT3WaveBankImpl_GetWaveIndex(IXACT3WaveBank *iface,
        PCSTR szFriendlyName)
{
    XACT3WaveBankImpl *This = impl_from_IXACT3WaveBank(iface);

    TRACE("(%p)->(%s)\n", This, szFriendlyName);

    return FACTWaveBank_GetWaveIndex(This->fact_wavebank, szFriendlyName);
}

static HRESULT WINAPI IXACT3WaveBankImpl_GetWaveProperties(IXACT3WaveBank *iface,
        XACTINDEX nWaveIndex, XACT_WAVE_PROPERTIES *pWaveProperties)
{
    XACT3WaveBankImpl *This = impl_from_IXACT3WaveBank(iface);

    TRACE("(%p)->(%u, %p)\n", This, nWaveIndex, pWaveProperties);

    return FACTWaveBank_GetWaveProperties(This->fact_wavebank, nWaveIndex,
            (FACTWaveProperties*) pWaveProperties);
}

static HRESULT WINAPI IXACT3WaveBankImpl_Prepare(IXACT3WaveBank *iface,
        XACTINDEX nWaveIndex, DWORD dwFlags, DWORD dwPlayOffset,
        XACTLOOPCOUNT nLoopCount, IXACT3Wave** ppWave)
{
    XACT3WaveBankImpl *This = impl_from_IXACT3WaveBank(iface);
    XACT3WaveImpl *wave;
    FACTWave *fwave;
    HRESULT hr;

    TRACE("(%p)->(%x, %u, %x, %u, %p)\n", This, nWaveIndex, dwFlags,
            dwPlayOffset, nLoopCount, ppWave);

    hr = FACTWaveBank_Prepare(This->fact_wavebank, nWaveIndex, dwFlags,
            dwPlayOffset, nLoopCount, &fwave);
    if(FAILED(hr))
        return hr;

    wave = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*wave));
    if (!wave){
        FACTWave_Destroy(fwave);
        ERR("Failed to allocate XACT3WaveImpl!");
        return hr;
    }

    wave->IXACT3Wave_iface.lpVtbl = &XACT3Wave_Vtbl;
    wave->fact_wave = fwave;
    *ppWave = (IXACT3Wave*)wave;

    TRACE("Created Wave: %p\n", wave);

    return hr;
}

static HRESULT WINAPI IXACT3WaveBankImpl_Play(IXACT3WaveBank *iface,
        XACTINDEX nWaveIndex, DWORD dwFlags, DWORD dwPlayOffset,
        XACTLOOPCOUNT nLoopCount, IXACT3Wave** ppWave)
{
    XACT3WaveBankImpl *This = impl_from_IXACT3WaveBank(iface);
    XACT3WaveImpl *wave;
    FACTWave *fwave;
    HRESULT hr;

    TRACE("(%p)->(%x, %u, %x, %u, %p)\n", This, nWaveIndex, dwFlags, dwPlayOffset,
            nLoopCount, ppWave);

    /* If the application doesn't want a handle, don't generate one at all.
     * Let the engine handle that memory instead.
     * -flibit
     */
    if (ppWave == NULL){
        hr = FACTWaveBank_Play(This->fact_wavebank, nWaveIndex, dwFlags,
                dwPlayOffset, nLoopCount, NULL);
    }else{
        hr = FACTWaveBank_Play(This->fact_wavebank, nWaveIndex, dwFlags,
                dwPlayOffset, nLoopCount, &fwave);
        if(FAILED(hr))
            return hr;

        wave = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*wave));
        if (!wave){
            FACTWave_Destroy(fwave);
            ERR("Failed to allocate XACT3WaveImpl!");
            return hr;
        }

        wave->IXACT3Wave_iface.lpVtbl = &XACT3Wave_Vtbl;
        wave->fact_wave = fwave;
        *ppWave = (IXACT3Wave*)wave;
    }

    return hr;
}

static HRESULT WINAPI IXACT3WaveBankImpl_Stop(IXACT3WaveBank *iface,
        XACTINDEX nWaveIndex, DWORD dwFlags)
{
    XACT3WaveBankImpl *This = impl_from_IXACT3WaveBank(iface);

    TRACE("(%p)->(%u, %u)\n", This, nWaveIndex, dwFlags);

    return FACTWaveBank_Stop(This->fact_wavebank, nWaveIndex, dwFlags);
}

static HRESULT WINAPI IXACT3WaveBankImpl_GetState(IXACT3WaveBank *iface,
        DWORD *pdwState)
{
    XACT3WaveBankImpl *This = impl_from_IXACT3WaveBank(iface);

    TRACE("(%p)->(%p)\n", This, pdwState);

    return FACTWaveBank_GetState(This->fact_wavebank, pdwState);
}

const IXACT3WaveBankVtbl XACT3WaveBank_Vtbl =
{
    IXACT3WaveBankImpl_Destroy,
    IXACT3WaveBankImpl_GetNumWaves,
    IXACT3WaveBankImpl_GetWaveIndex,
    IXACT3WaveBankImpl_GetWaveProperties,
    IXACT3WaveBankImpl_Prepare,
    IXACT3WaveBankImpl_Play,
    IXACT3WaveBankImpl_Stop,
    IXACT3WaveBankImpl_GetState
};
