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

static inline XACT3SoundBankImpl *impl_from_IXACT3SoundBank(IXACT3SoundBank *iface)
{
    return CONTAINING_RECORD(iface, IXACT3SoundBankImpl, IXACT3SoundBank_iface);
}

static XACTINDEX IXACT3SoundBankImpl_GetCueIndex(IXACT3SoundBank *iface,
        PCSTR szFriendlyName)
{
    XACT3SoundBankImpl *This = impl_from_IXACT3SoundBank(iface);

    TRACE("(%p)->(%s)\n", This, szFriendlyName);

    return FACTSoundBank_GetCueIndex(This->fact_soundbank, szFriendlyName);
}

static HRESULT IXACT3SoundBankImpl_GetNumCues(IXACT3SoundBank *iface,
        XACTINDEX *pnNumCues)
{
    XACT3SoundBankImpl *This = impl_from_IXACT3SoundBank(iface);

    TRACE("(%p)->(%p)\n", This, pnNumCues);

    return FACTSoundBank_GetNumCues(This->fact_soundbank, pnNumCues);
}

static HRESULT IXACT3SoundBankImpl_GetCueProperties(IXACT3SoundBank *iface,
        XACTINDEX nCueIndex, LPXACT_CUE_PROPERTIES pProperties)
{
    XACT3SoundBankImpl *This = impl_from_IXACT3SoundBank(iface);

    TRACE("(%p)->(%u, %p)\n", This, nCueIndex, pProperties);

    return FACTSoundBank_GetCueProperties(This->fact_soundbank, nCueIndex,
            (FACTCueProperties*) pProperties);
}

static HRESULT IXACT3SoundBankImpl_Prepare(IXACT3SoundBank *iface,
        XACTINDEX nCueIndex, DWORD dwFlags, XACTTIME timeOffset,
        XACTLOOPCOUNT nLoopCount, IXACT3Cue** ppCue)
{
    XACT3SoundBankImpl *This = impl_from_IXACT3SoundBank(iface);
    XACT3CueImpl *cue;
    FACTCue *fcue;
    HRESULT hr;

    TRACE("(%p)->(%u, %x, %u, %u, %p)\n", This, nCueIndex, dwFlags, timeOffset,
            nLoopCount, ppCue);

    hr = FACTSoundBank_Prepare(This->fact_soundbank, dwFlags, timeOffset,
            nLoopCount, &fcue);
    if(FAILED(hr))
        return hr;

    cue = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*cue));
    if (!cue){
        FACTCue_Destroy(fcue);
        ERR("Failed to allocate XACT3CueImpl!");
        return hr;
    }

    cue->IXACT3Cue_iface.lpVtbl = XACT37Cue_Vtbl;
#if XACT3_VER <= 4
    cue->IXACT34Cue_iface.lpVtbl = &XACT34Cue_Vtbl;
#endif
    cue->fact_cue = fcue;
#if XACT3_VER <= 4
    *ppCue = (IXACT3Cue*)&cue->IXACT34Cue_iface;
#else
    *ppCue = (IXACT3Cue*)&cue->IXACT3Cue_iface;
#endif

    TRACE("Created Cue: %p\n", cue);

    return hr;
}

static HRESULT IXACT3SoundBankImpl_Play(IXACT3SoundBank *iface,
        XACTINDEX nCueIndex, DWORD dwFlags, XACTTIME timeOffset,
        XACTLOOPCOUNT nLoopCount, IXACT3Cue** ppCue)
{
    XACT3SoundBankImpl *This = impl_from_IXACT3SoundBank(iface);
    XACT3CueImpl *cue;
    FACTCue *fcue;
    HRESULT hr;

    TRACE("(%p)->(%u, %x, %u, %u, %p)\n", This, nCueIndex, dwFlags, timeOffset,
            nLoopCount, ppCue;

    /* If the application doesn't want a handle, don't generate one at all.
     * Let the engine handle that memory instead.
     * -flibit
     */
    if (ppCue == NULL){
        hr = FACTSoundBank_Play(This->fact_soundbank, dwFlags, dwPlayOffset,
                nLoopCount, NULL);
    }else{
        hr = FACTSoundBank_Play(This->fact_soundbank, dwFlags, dwPlayOffset,
                nLoopCount, &fcue);
        if(FAILED(hr))
            return hr;

        cue = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*cue));
        if (!cue){
            FACTCue_Destroy(fcue);
            ERR("Failed to allocate XACT3CueImpl!");
            return hr;
        }

        cue->IXACT3Cue_iface.lpVtbl = XACT37Cue_Vtbl;
#if XACT3_VER <= 4
        cue->IXACT34Cue_iface.lpVtbl = &XACT34Cue_Vtbl;
#endif
        cue->fact_cue = fcue;
#if XACT3_VER <= 4
        *ppCue = (IXACT3Cue*)&cue->IXACT34Cue_iface;
#else
        *ppCue = (IXACT3Cue*)&cue->IXACT3Cue_iface;
#endif
        cue->fact_cue = fcue;
        *ppCue = cue;
    }

    return hr;
}

static HRESULT IXACT3SoundBankImpl_Stop(IXACT3SoundBank *iface, DWORD dwFlags)
{
    XACT3SoundBankImpl *This = impl_from_IXACT3SoundBank(iface);

    TRACE("(%p)->(%u)\n", This, dwFlags);

    return FACTSoundBank_Stop(This->fact_soundbank, dwFlags);
}

static HRESULT IXACT3SoundBankImpl_Destroy(IXACT3SoundBank *iface)
{
    XACT3SoundBankImpl *This = impl_from_IXACT3SoundBank(iface);
    HRESULT hr;

    TRACE("(%p)\n", This);

    hr = FACTSoundBank_Destroy(This->fact_soundbank);
    HeapFree(GetProcessHeap(), 0, This);
    return hr;
}

static HRESULT IXACT3SoundBankImpl_GetState(IXACT3SoundBank *iface,
        DWORD *pdwState)
{
    XACT3SoundBankImpl *This = impl_from_IXACT3SoundBank(iface);

    TRACE("(%p)->(%p)\n", This, pdwState);

    return FACTSoundBank_GetState(This->fact_soundbank, pdwState);
}

const XACT3SoundBankVtbl XACT3SoundBank_Vtbl =
{
    IXACT3SoundBankImpl_GetNumCues,
    IXACT3SoundBankImpl_GetCueIndex,
    IXACT3SoundBankImpl_GetCueProperties,
    IXACT3SoundBankImpl_Prepare,
    IXACT3SoundBankImpl_Play,
    IXACT3SoundBankImpl_Stop,
    IXACT3SoundBankImpl_Destroy,
    IXACT3SoundBankImpl_GetState
};
