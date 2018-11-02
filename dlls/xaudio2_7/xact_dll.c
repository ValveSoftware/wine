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

#include "initguid.h"
#include "xact3.h"

#include "rpcproxy.h"
#include "wine/debug.h"

#include <FACT.h>

WINE_DEFAULT_DEBUG_CHANNEL(xact3);

/* xaudio_allocator.c */
extern void* XAudio_Internal_Malloc(size_t size) DECLSPEC_HIDDEN;
extern void XAudio_Internal_Free(void* ptr) DECLSPEC_HIDDEN;
extern void* XAudio_Internal_Realloc(void* ptr, size_t size) DECLSPEC_HIDDEN;

static HINSTANCE instance;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD reason, void *pReserved)
{
    TRACE("(%p, %d, %p)\n", hinstDLL, reason, pReserved);

    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        instance = hinstDLL;
        DisableThreadLibraryCalls( hinstDLL );
        break;
    }
    return TRUE;
}

HRESULT WINAPI DllCanUnloadNow(void)
{
    return S_FALSE;
}

HRESULT WINAPI DllRegisterServer(void)
{
    TRACE("\n");
    return __wine_register_resources(instance);
}

HRESULT WINAPI DllUnregisterServer(void)
{
    TRACE("\n");
    return __wine_unregister_resources(instance);
}

typedef struct _XACT3CueImpl {
    IXACT3Cue IXACT3Cue_iface;
#if XACT3_VER <= 4
    IXACT34Cue IXACT34Cue_iface;
#endif

    FACTCue *fact_cue;
} XACT3CueImpl;

typedef struct _XACT3WaveImpl {
    IXACT3Wave IXACT3Wave_iface;

    FACTWave *fact_wave;
} XACT3WaveImpl;

typedef struct _XACT3SoundBankImpl {
    IXACT3SoundBank IXACT3SoundBank_iface;

    FACTSoundBank *fact_soundbank;
} XACT3SoundBankImpl;

typedef struct _XACT3WaveBankImpl {
    IXACT3WaveBank IXACT3WaveBank_iface;

    FACTWaveBank *fact_wavebank;
} XACT3WaveBankImpl;

typedef struct _XACT3EngineImpl {
    IXACT3Engine IXACT3Engine_iface;

    FACTAudioEngine *fact_engine;
} XACT3EngineImpl;

static inline XACT3CueImpl *impl_from_IXACT3Cue(IXACT3Cue *iface)
{
    return CONTAINING_RECORD(iface, XACT3CueImpl, IXACT3Cue_iface);
}

static HRESULT WINAPI IXACT3CueImpl_Play(IXACT3Cue *iface)
{
    XACT3CueImpl *This = impl_from_IXACT3Cue(iface);

    TRACE("(%p)\n", iface);

    return FACTCue_Play(This->fact_cue);
}

static HRESULT WINAPI IXACT3CueImpl_Stop(IXACT3Cue *iface, DWORD dwFlags)
{
    XACT3CueImpl *This = impl_from_IXACT3Cue(iface);

    TRACE("(%p)->(%u)\n", iface, dwFlags);

    return FACTCue_Stop(This->fact_cue, dwFlags);
}

static HRESULT WINAPI IXACT3CueImpl_GetState(IXACT3Cue *iface, DWORD *pdwState)
{
    XACT3CueImpl *This = impl_from_IXACT3Cue(iface);

    TRACE("(%p)->(%p)\n", iface, pdwState);

    return FACTCue_GetState(This->fact_cue, pdwState);
}

static HRESULT WINAPI IXACT3CueImpl_Destroy(IXACT3Cue *iface)
{
    XACT3CueImpl *This = impl_from_IXACT3Cue(iface);
    HRESULT hr;

    TRACE("(%p)\n", iface);

    hr = FACTCue_Destroy(This->fact_cue);
    HeapFree(GetProcessHeap(), 0, This);
    return hr;
}

static HRESULT WINAPI IXACT3CueImpl_SetMatrixCoefficients(IXACT3Cue *iface,
        UINT32 uSrcChannelCount, UINT32 uDstChannelCount,
        float *pMatrixCoefficients)
{
    XACT3CueImpl *This = impl_from_IXACT3Cue(iface);

    TRACE("(%p)->(%u, %u, %p)\n", iface, uSrcChannelCount, uDstChannelCount,
            pMatrixCoefficients);

    return FACTCue_SetMatrixCoefficients(This->fact_cue, uSrcChannelCount,
        uDstChannelCount, pMatrixCoefficients);
}

static XACTVARIABLEINDEX WINAPI IXACT3CueImpl_GetVariableIndex(IXACT3Cue *iface,
        PCSTR szFriendlyName)
{
    XACT3CueImpl *This = impl_from_IXACT3Cue(iface);

    TRACE("(%p)->(%s)\n", iface, szFriendlyName);

    return FACTCue_GetVariableIndex(This->fact_cue, szFriendlyName);
}

static HRESULT WINAPI IXACT3CueImpl_SetVariable(IXACT3Cue *iface,
        XACTVARIABLEINDEX nIndex, XACTVARIABLEVALUE nValue)
{
    XACT3CueImpl *This = impl_from_IXACT3Cue(iface);

    TRACE("(%p)->(%u, %f)\n", iface, nIndex, nValue);

    return FACTCue_SetVariable(This->fact_cue, nIndex, nValue);
}

static HRESULT WINAPI IXACT3CueImpl_GetVariable(IXACT3Cue *iface,
        XACTVARIABLEINDEX nIndex, XACTVARIABLEVALUE *nValue)
{
    XACT3CueImpl *This = impl_from_IXACT3Cue(iface);

    TRACE("(%p)->(%u, %p)\n", iface, nIndex, nValue);

    return FACTCue_GetVariable(This->fact_cue, nIndex, nValue);
}

static HRESULT WINAPI IXACT3CueImpl_Pause(IXACT3Cue *iface, BOOL fPause)
{
    XACT3CueImpl *This = impl_from_IXACT3Cue(iface);

    TRACE("(%p)->(%u)\n", iface, fPause);

    return FACTCue_Pause(This->fact_cue, fPause);
}

static HRESULT WINAPI IXACT3CueImpl_GetProperties(IXACT3Cue *iface,
        XACT_CUE_INSTANCE_PROPERTIES **ppProperties)
{
    XACT3CueImpl *This = impl_from_IXACT3Cue(iface);
    FACTCueInstanceProperties *fProps;
    HRESULT hr;

    TRACE("(%p)->(%p)\n", iface, ppProperties);

    hr = FACTCue_GetProperties(This->fact_cue, &fProps);
    if(FAILED(hr))
        return hr;

    *ppProperties = (XACT_CUE_INSTANCE_PROPERTIES*) fProps;
    return hr;
}

static HRESULT WINAPI IXACT3CueImpl_SetOutputVoices(IXACT3Cue *iface,
        const XAUDIO2_VOICE_SENDS *pSendList)
{
    XACT3CueImpl *This = impl_from_IXACT3Cue(iface);
    FIXME("(%p): stub!\n", This);
    return S_OK;
}

static HRESULT WINAPI IXACT3CueImpl_SetOutputVoiceMatrix(IXACT3Cue *iface,
        IXAudio2Voice *pDestinationVoice, UINT32 SourceChannels,
        UINT32 DestinationChannels, const float *pLevelMatrix)
{
    XACT3CueImpl *This = impl_from_IXACT3Cue(iface);
    FIXME("(%p): stub!\n", This);
    return S_OK;
}

static const IXACT3CueVtbl XACT3Cue_Vtbl =
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

#if XACT3_VER <= 4
static inline XACT3CueImpl *impl_from_IXACT34Cue(IXACT34Cue *iface)
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

static const IXACT34CueVtbl XACT34Cue_Vtbl =
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

static const IXACT3WaveVtbl XACT3Wave_Vtbl =
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

static inline XACT3SoundBankImpl *impl_from_IXACT3SoundBank(IXACT3SoundBank *iface)
{
    return CONTAINING_RECORD(iface, XACT3SoundBankImpl, IXACT3SoundBank_iface);
}

static XACTINDEX WINAPI IXACT3SoundBankImpl_GetCueIndex(IXACT3SoundBank *iface,
        PCSTR szFriendlyName)
{
    XACT3SoundBankImpl *This = impl_from_IXACT3SoundBank(iface);

    TRACE("(%p)->(%s)\n", This, szFriendlyName);

    return FACTSoundBank_GetCueIndex(This->fact_soundbank, szFriendlyName);
}

static HRESULT WINAPI IXACT3SoundBankImpl_GetNumCues(IXACT3SoundBank *iface,
        XACTINDEX *pnNumCues)
{
    XACT3SoundBankImpl *This = impl_from_IXACT3SoundBank(iface);

    TRACE("(%p)->(%p)\n", This, pnNumCues);

    return FACTSoundBank_GetNumCues(This->fact_soundbank, pnNumCues);
}

static HRESULT WINAPI IXACT3SoundBankImpl_GetCueProperties(IXACT3SoundBank *iface,
        XACTINDEX nCueIndex, XACT_CUE_PROPERTIES *pProperties)
{
    XACT3SoundBankImpl *This = impl_from_IXACT3SoundBank(iface);

    TRACE("(%p)->(%u, %p)\n", This, nCueIndex, pProperties);

    return FACTSoundBank_GetCueProperties(This->fact_soundbank, nCueIndex,
            (FACTCueProperties*) pProperties);
}

static HRESULT WINAPI IXACT3SoundBankImpl_Prepare(IXACT3SoundBank *iface,
        XACTINDEX nCueIndex, DWORD dwFlags, XACTTIME timeOffset,
        IXACT3Cue** ppCue)
{
    XACT3SoundBankImpl *This = impl_from_IXACT3SoundBank(iface);
    XACT3CueImpl *cue;
    FACTCue *fcue;
    HRESULT hr;

    TRACE("(%p)->(%u, 0x%x, %u, %p)\n", This, nCueIndex, dwFlags, timeOffset,
            ppCue);

    hr = FACTSoundBank_Prepare(This->fact_soundbank, nCueIndex, dwFlags,
            timeOffset, &fcue);
    if(FAILED(hr))
        return hr;

    cue = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*cue));
    if (!cue){
        FACTCue_Destroy(fcue);
        ERR("Failed to allocate XACT3CueImpl!");
        return hr;
    }

    cue->IXACT3Cue_iface.lpVtbl = &XACT3Cue_Vtbl;
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

static HRESULT WINAPI IXACT3SoundBankImpl_Play(IXACT3SoundBank *iface,
        XACTINDEX nCueIndex, DWORD dwFlags, XACTTIME timeOffset,
        IXACT3Cue** ppCue)
{
    XACT3SoundBankImpl *This = impl_from_IXACT3SoundBank(iface);
    XACT3CueImpl *cue;
    FACTCue *fcue;
    HRESULT hr;

    TRACE("(%p)->(%u, 0x%x, %u, %p)\n", This, nCueIndex, dwFlags, timeOffset,
            ppCue);

    /* If the application doesn't want a handle, don't generate one at all.
     * Let the engine handle that memory instead.
     * -flibit
     */
    if (ppCue == NULL){
        hr = FACTSoundBank_Play(This->fact_soundbank, nCueIndex, dwFlags,
                timeOffset, NULL);
    }else{
        hr = FACTSoundBank_Play(This->fact_soundbank, nCueIndex, dwFlags,
                timeOffset, &fcue);
        if(FAILED(hr))
            return hr;

        cue = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*cue));
        if (!cue){
            FACTCue_Destroy(fcue);
            ERR("Failed to allocate XACT3CueImpl!");
            return hr;
        }

        cue->IXACT3Cue_iface.lpVtbl = &XACT3Cue_Vtbl;
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
        *ppCue = (IXACT3Cue*)cue;
    }

    return hr;
}

static HRESULT WINAPI IXACT3SoundBankImpl_Stop(IXACT3SoundBank *iface,
        XACTINDEX nCueIndex, DWORD dwFlags)
{
    XACT3SoundBankImpl *This = impl_from_IXACT3SoundBank(iface);

    TRACE("(%p)->(%u)\n", This, dwFlags);

    return FACTSoundBank_Stop(This->fact_soundbank, nCueIndex, dwFlags);
}

static HRESULT WINAPI IXACT3SoundBankImpl_Destroy(IXACT3SoundBank *iface)
{
    XACT3SoundBankImpl *This = impl_from_IXACT3SoundBank(iface);
    HRESULT hr;

    TRACE("(%p)\n", This);

    hr = FACTSoundBank_Destroy(This->fact_soundbank);
    HeapFree(GetProcessHeap(), 0, This);
    return hr;
}

static HRESULT WINAPI IXACT3SoundBankImpl_GetState(IXACT3SoundBank *iface,
        DWORD *pdwState)
{
    XACT3SoundBankImpl *This = impl_from_IXACT3SoundBank(iface);

    TRACE("(%p)->(%p)\n", This, pdwState);

    return FACTSoundBank_GetState(This->fact_soundbank, pdwState);
}

static const IXACT3SoundBankVtbl XACT3SoundBank_Vtbl =
{
    IXACT3SoundBankImpl_GetCueIndex,
    IXACT3SoundBankImpl_GetNumCues,
    IXACT3SoundBankImpl_GetCueProperties,
    IXACT3SoundBankImpl_Prepare,
    IXACT3SoundBankImpl_Play,
    IXACT3SoundBankImpl_Stop,
    IXACT3SoundBankImpl_Destroy,
    IXACT3SoundBankImpl_GetState
};

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

    TRACE("(%p)->(0x%x, %u, 0x%x, %u, %p)\n", This, nWaveIndex, dwFlags,
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

    TRACE("(%p)->(0x%x, %u, 0x%x, %u, %p)\n", This, nWaveIndex, dwFlags, dwPlayOffset,
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

static const IXACT3WaveBankVtbl XACT3WaveBank_Vtbl =
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

static inline XACT3EngineImpl *impl_from_IXACT3Engine(IXACT3Engine *iface)
{
    return CONTAINING_RECORD(iface, XACT3EngineImpl, IXACT3Engine_iface);
}

static HRESULT WINAPI IXACT3EngineImpl_QueryInterface(IXACT3Engine *iface,
        REFIID riid, void **ppvObject)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);

    TRACE("(%p)->(%s, %p)\n", This, debugstr_guid(riid), ppvObject);

    if(IsEqualGUID(riid, &IID_IUnknown) ||
            IsEqualGUID(riid, &IID_IXACT3Engine)){
        *ppvObject = &This->IXACT3Engine_iface;
    }
    else
        *ppvObject = NULL;

    if (*ppvObject){
        IUnknown_AddRef((IUnknown*)*ppvObject);
        return S_OK;
    }

    FIXME("(%p)->(%s,%p), not found\n", This, debugstr_guid(riid), ppvObject);

    return E_NOINTERFACE;
}

static ULONG WINAPI IXACT3EngineImpl_AddRef(IXACT3Engine *iface)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);
    ULONG ref = FACTAudioEngine_AddRef(This->fact_engine);
    TRACE("(%p)->(): Refcount now %u\n", This, ref);
    return ref;
}

static ULONG WINAPI IXACT3EngineImpl_Release(IXACT3Engine *iface)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);
    ULONG ref = FACTAudioEngine_Release(This->fact_engine);

    TRACE("(%p)->(): Refcount now %u\n", This, ref);

    if (!ref)
        HeapFree(GetProcessHeap(), 0, This);
    return ref;
}

static HRESULT WINAPI IXACT3EngineImpl_GetRendererCount(IXACT3Engine *iface,
        XACTINDEX *pnRendererCount)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);

    TRACE("(%p)->(%p)\n", This, pnRendererCount);

    return FACTAudioEngine_GetRendererCount(This->fact_engine, pnRendererCount);
}

static HRESULT WINAPI IXACT3EngineImpl_GetRendererDetails(IXACT3Engine *iface,
        XACTINDEX nRendererIndex, XACT_RENDERER_DETAILS *pRendererDetails)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);

    TRACE("(%p)->(%d, %p)\n", This, nRendererIndex, pRendererDetails);

    return FACTAudioEngine_GetRendererDetails(This->fact_engine,
            nRendererIndex, (FACTRendererDetails*) pRendererDetails);
}

static HRESULT WINAPI IXACT3EngineImpl_GetFinalMixFormat(IXACT3Engine *iface,
        WAVEFORMATEXTENSIBLE *pFinalMixFormat)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);

    TRACE("(%p)->(%p)\n", This, pFinalMixFormat);

    return FACTAudioEngine_GetFinalMixFormat(This->fact_engine,
            (FAudioWaveFormatExtensible*) pFinalMixFormat);
}

static HRESULT WINAPI IXACT3EngineImpl_Initialize(IXACT3Engine *iface,
        const XACT_RUNTIME_PARAMETERS *pParams)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);

    TRACE("(%p)->(%p)\n", This, pParams);

    /* TODO: Unwrap FAudio/FAudioMasteringVoice */
    if (pParams->pXAudio2 != NULL || pParams->pMasteringVoice != NULL)
        ERR("XAudio2 pointers are not yet supported!");

    return FACTAudioEngine_Initialize(This->fact_engine,
            (FACTRuntimeParameters*) pParams);
}

static HRESULT WINAPI IXACT3EngineImpl_ShutDown(IXACT3Engine *iface)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);

    TRACE("(%p)\n", This);

    return FACTAudioEngine_ShutDown(This->fact_engine);
}

static HRESULT WINAPI IXACT3EngineImpl_DoWork(IXACT3Engine *iface)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);

    TRACE("(%p)\n", This);

    return FACTAudioEngine_DoWork(This->fact_engine);
}

static HRESULT WINAPI IXACT3EngineImpl_CreateSoundBank(IXACT3Engine *iface,
        const BYTE* pvBuffer, DWORD dwSize, DWORD dwFlags,
        DWORD dwAllocAttributes, IXACT3SoundBank **ppSoundBank)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);
    XACT3SoundBankImpl *sb;
    FACTSoundBank *fsb;
    HRESULT hr;

    TRACE("(%p)->(%p, %u, 0x%x, 0x%x, %p)\n", This, pvBuffer, dwSize, dwFlags,
            dwAllocAttributes, ppSoundBank);

    hr = FACTAudioEngine_CreateSoundBank(This->fact_engine, pvBuffer, dwSize,
            dwFlags, dwAllocAttributes, &fsb);
    if(FAILED(hr))
        return hr;

    sb = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*sb));
    if (!sb){
        FACTSoundBank_Destroy(fsb);
        ERR("Failed to allocate XACT3SoundBankImpl!");
        return hr;
    }

    sb->IXACT3SoundBank_iface.lpVtbl = &XACT3SoundBank_Vtbl;
    sb->fact_soundbank = fsb;
    *ppSoundBank = (IXACT3SoundBank*)sb;

    TRACE("Created SoundBank: %p\n", sb);

    return hr;
}

static HRESULT WINAPI IXACT3EngineImpl_CreateInMemoryWaveBank(IXACT3Engine *iface,
        const BYTE* pvBuffer, DWORD dwSize, DWORD dwFlags,
        DWORD dwAllocAttributes, IXACT3WaveBank **ppWaveBank)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);
    XACT3WaveBankImpl *wb;
    FACTWaveBank *fwb;
    HRESULT hr;

    TRACE("(%p)->(%p, %u, 0x%x, 0x%x, %p)\n", This, pvBuffer, dwSize, dwFlags,
            dwAllocAttributes, ppWaveBank);

    hr = FACTAudioEngine_CreateInMemoryWaveBank(This->fact_engine, pvBuffer,
            dwSize, dwFlags, dwAllocAttributes, &fwb);
    if(FAILED(hr))
        return hr;

    wb = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*wb));
    if (!wb){
        FACTWaveBank_Destroy(fwb);
        ERR("Failed to allocate XACT3WaveBankImpl!");
        return hr;
    }

    wb->IXACT3WaveBank_iface.lpVtbl = &XACT3WaveBank_Vtbl;
    wb->fact_wavebank = fwb;
    *ppWaveBank = (IXACT3WaveBank*)wb;

    TRACE("Created in-memory WaveBank: %p\n", wb);

    return hr;
}

static size_t wrap_io_read(
	void *data,
	void *dst,
	size_t size,
	size_t count
) {
	DWORD byte_read;
	if (!ReadFile((HANDLE) data, dst, size * count, &byte_read, NULL))
	{
		return 0;
	}
	return byte_read;
}

static int64_t wrap_io_seek(void *data, int64_t offset, int whence)
{
	DWORD windowswhence = 0;
	LARGE_INTEGER windowsoffset;
	HANDLE io = (HANDLE) data;

	switch (whence)
	{
	case FAUDIO_SEEK_SET:
		windowswhence = FILE_BEGIN;
		break;
	case FAUDIO_SEEK_CUR:
		windowswhence = FILE_CURRENT;
		break;
	case FAUDIO_SEEK_END:
		windowswhence = FILE_END;
		break;
	}

	windowsoffset.QuadPart = offset;
	if (!SetFilePointerEx(io, windowsoffset, &windowsoffset, windowswhence))
	{
		return -1;
	}
	return windowsoffset.QuadPart;
}

static int wrap_io_close(void *data)
{
	CloseHandle((HANDLE) data);
	return 0;
}

static HRESULT WINAPI IXACT3EngineImpl_CreateStreamingWaveBank(IXACT3Engine *iface,
        const XACT_STREAMING_PARAMETERS *pParms,
        IXACT3WaveBank **ppWaveBank)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);
    FACTStreamingParameters fakeParms;
    XACT3WaveBankImpl *wb;
    FAudioIOStream *fake;
    FACTWaveBank *fwb;
    HRESULT hr;

    TRACE("(%p)->(%p, %p)\n", This, pParms, ppWaveBank);

    /* We have to wrap the file around an IOStream first! */
    fake = (FAudioIOStream*) CoTaskMemAlloc(
            sizeof(FAudioIOStream));
    fake->data = pParms->file;
    fake->read = wrap_io_read;
    fake->seek = wrap_io_seek;
    fake->close = wrap_io_close;
    fakeParms.file = fake;
    fakeParms.flags = pParms->flags;
    fakeParms.offset = pParms->offset;
    fakeParms.packetSize = pParms->packetSize;

    hr = FACTAudioEngine_CreateStreamingWaveBank(This->fact_engine, &fakeParms,
            &fwb);
    if(FAILED(hr))
        return hr;

    wb = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*wb));
    if (!wb){
        FACTWaveBank_Destroy(fwb);
        ERR("Failed to allocate XACT3WaveBankImpl!");
        return hr;
    }

    wb->IXACT3WaveBank_iface.lpVtbl = &XACT3WaveBank_Vtbl;
    wb->fact_wavebank = fwb;
    *ppWaveBank = (IXACT3WaveBank*)wb;

    TRACE("Created streaming WaveBank: %p\n", wb);

    return hr;
}

static HRESULT WINAPI IXACT3EngineImpl_PrepareWave(IXACT3Engine *iface,
        DWORD dwFlags, PCSTR szWavePath, WORD wStreamingPacketSize,
        DWORD dwAlignment, DWORD dwPlayOffset, XACTLOOPCOUNT nLoopCount,
        IXACT3Wave **ppWave)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);
    FIXME("(%p): stub!\n", This);
    return S_OK;
}

static HRESULT WINAPI IXACT3EngineImpl_PrepareInMemoryWave(IXACT3Engine *iface,
        DWORD dwFlags, WAVEBANKENTRY entry, DWORD *pdwSeekTable,
        BYTE *pbWaveData, DWORD dwPlayOffset, XACTLOOPCOUNT nLoopCount,
        IXACT3Wave **ppWave)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);
    FIXME("(%p): stub!\n", This);
    return S_OK;
}

static HRESULT WINAPI IXACT3EngineImpl_PrepareStreamingWave(IXACT3Engine *iface,
        DWORD dwFlags, WAVEBANKENTRY entry,
        XACT_STREAMING_PARAMETERS streamingParams, DWORD dwAlignment,
        DWORD *pdwSeekTable, DWORD dwPlayOffset, XACTLOOPCOUNT nLoopCount,
        IXACT3Wave **ppWave)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);
    FIXME("(%p): stub!\n", This);
    return S_OK;
}

static void unwrap_notificationdesc(FACTNotificationDescription *fd,
        const XACT_NOTIFICATION_DESCRIPTION *xd)
{
    /* We have to unwrap the FACT object first! */
    fd->type = xd->type;
    fd->flags = xd->flags;
    fd->cueIndex = xd->cueIndex;
    fd->waveIndex = xd->waveIndex;
    fd->pvContext = xd->pvContext;
    if (xd->type == XACTNOTIFICATIONTYPE_CUEDESTROYED)
    {
        fd->pCue = ((XACT3CueImpl*) xd->pCue)->fact_cue;
    }
    else if (xd->type == XACTNOTIFICATIONTYPE_SOUNDBANKDESTROYED)
    {
        fd->pSoundBank = ((XACT3SoundBankImpl*) xd->pSoundBank)->fact_soundbank;
    }
    else if (xd->type == XACTNOTIFICATIONTYPE_WAVEBANKDESTROYED)
    {
        fd->pWaveBank = ((XACT3WaveBankImpl*) xd->pWaveBank)->fact_wavebank;
    }
    else if (xd->type == XACTNOTIFICATIONTYPE_WAVEDESTROYED)
    {
        fd->pWave = ((XACT3WaveImpl*) xd->pWave)->fact_wave;
    }
    else
    {
        /* If you didn't hit an above if, get ready for an assert! */
        ERR("Unrecognized XACT notification type!");
    }
}

static HRESULT WINAPI IXACT3EngineImpl_RegisterNotification(IXACT3Engine *iface,
        const XACT_NOTIFICATION_DESCRIPTION *pNotificationDesc)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);
    FACTNotificationDescription fdesc;

    TRACE("(%p)->(%p)\n", This, pNotificationDesc);

    unwrap_notificationdesc(&fdesc, pNotificationDesc);
    return FACTAudioEngine_RegisterNotification(This->fact_engine, &fdesc);
}

static HRESULT WINAPI IXACT3EngineImpl_UnRegisterNotification(IXACT3Engine *iface,
        const XACT_NOTIFICATION_DESCRIPTION *pNotificationDesc)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);
    FACTNotificationDescription fdesc;

    TRACE("(%p)->(%p)\n", This, pNotificationDesc);

    unwrap_notificationdesc(&fdesc, pNotificationDesc);
    return FACTAudioEngine_UnRegisterNotification(This->fact_engine, &fdesc);
}

static XACTCATEGORY WINAPI IXACT3EngineImpl_GetCategory(IXACT3Engine *iface,
        PCSTR szFriendlyName)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);

    TRACE("(%p)->(%s)\n", This, szFriendlyName);

    return FACTAudioEngine_GetCategory(This->fact_engine, szFriendlyName);
}

static HRESULT WINAPI IXACT3EngineImpl_Stop(IXACT3Engine *iface,
        XACTCATEGORY nCategory, DWORD dwFlags)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);

    TRACE("(%p)->(%u, 0x%x)\n", This, nCategory, dwFlags);

    return FACTAudioEngine_Stop(This->fact_engine, nCategory, dwFlags);
}

static HRESULT WINAPI IXACT3EngineImpl_SetVolume(IXACT3Engine *iface,
        XACTCATEGORY nCategory, XACTVOLUME nVolume)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);

    TRACE("(%p)->(%u, %f)\n", This, nCategory, nVolume);

    return FACTAudioEngine_SetVolume(This->fact_engine, nCategory, nVolume);
}

static HRESULT WINAPI IXACT3EngineImpl_Pause(IXACT3Engine *iface,
        XACTCATEGORY nCategory, BOOL fPause)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);

    TRACE("(%p)->(%u, %u)\n", This, nCategory, fPause);

    return FACTAudioEngine_Pause(This->fact_engine, nCategory, fPause);
}

static XACTVARIABLEINDEX WINAPI IXACT3EngineImpl_GetGlobalVariableIndex(
        IXACT3Engine *iface, PCSTR szFriendlyName)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);

    TRACE("(%p)->(%s)\n", This, szFriendlyName);

    return FACTAudioEngine_GetGlobalVariableIndex(This->fact_engine,
            szFriendlyName);
}

static HRESULT WINAPI IXACT3EngineImpl_SetGlobalVariable(IXACT3Engine *iface,
        XACTVARIABLEINDEX nIndex, XACTVARIABLEVALUE nValue)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);

    TRACE("(%p)->(%u, %f)\n", This, nIndex, nValue);

    return FACTAudioEngine_SetGlobalVariable(This->fact_engine, nIndex, nValue);
}

static HRESULT WINAPI IXACT3EngineImpl_GetGlobalVariable(IXACT3Engine *iface,
        XACTVARIABLEINDEX nIndex, XACTVARIABLEVALUE *nValue)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);

    TRACE("(%p)->(%u, %p)\n", This, nIndex, nValue);

    return FACTAudioEngine_GetGlobalVariable(This->fact_engine, nIndex, nValue);
}

static const IXACT3EngineVtbl XACT3Engine_Vtbl =
{
    IXACT3EngineImpl_QueryInterface,
    IXACT3EngineImpl_AddRef,
    IXACT3EngineImpl_Release,
    IXACT3EngineImpl_GetRendererCount,
    IXACT3EngineImpl_GetRendererDetails,
    IXACT3EngineImpl_GetFinalMixFormat,
    IXACT3EngineImpl_Initialize,
    IXACT3EngineImpl_ShutDown,
    IXACT3EngineImpl_DoWork,
    IXACT3EngineImpl_CreateSoundBank,
    IXACT3EngineImpl_CreateInMemoryWaveBank,
    IXACT3EngineImpl_CreateStreamingWaveBank,
    IXACT3EngineImpl_PrepareWave,
    IXACT3EngineImpl_PrepareInMemoryWave,
    IXACT3EngineImpl_PrepareStreamingWave,
    IXACT3EngineImpl_RegisterNotification,
    IXACT3EngineImpl_UnRegisterNotification,
    IXACT3EngineImpl_GetCategory,
    IXACT3EngineImpl_Stop,
    IXACT3EngineImpl_SetVolume,
    IXACT3EngineImpl_Pause,
    IXACT3EngineImpl_GetGlobalVariableIndex,
    IXACT3EngineImpl_SetGlobalVariable,
    IXACT3EngineImpl_GetGlobalVariable
};

struct xact3_cf {
    IClassFactory IClassFactory_iface;
    LONG ref;
};

static struct xact3_cf *impl_from_IClassFactory(IClassFactory *iface)
{
    return CONTAINING_RECORD(iface, struct xact3_cf, IClassFactory_iface);
}

static HRESULT WINAPI XACT3CF_QueryInterface(IClassFactory *iface, REFIID riid, void **ppobj)
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

static ULONG WINAPI XACT3CF_AddRef(IClassFactory *iface)
{
    struct xact3_cf *This = impl_from_IClassFactory(iface);
    ULONG ref = InterlockedIncrement(&This->ref);
    TRACE("(%p)->(): Refcount now %u\n", This, ref);
    return ref;
}

static ULONG WINAPI XACT3CF_Release(IClassFactory *iface)
{
    struct xact3_cf *This = impl_from_IClassFactory(iface);
    ULONG ref = InterlockedDecrement(&This->ref);
    TRACE("(%p)->(): Refcount now %u\n", This, ref);
    if (!ref)
        HeapFree(GetProcessHeap(), 0, This);
    return ref;
}

static HRESULT WINAPI XACT3CF_CreateInstance(IClassFactory *iface, IUnknown *pOuter,
                                               REFIID riid, void **ppobj)
{
    struct xact3_cf *This = impl_from_IClassFactory(iface);
    HRESULT hr;
    XACT3EngineImpl *object;

    TRACE("(%p)->(%p,%s,%p)\n", This, pOuter, debugstr_guid(riid), ppobj);

    *ppobj = NULL;

    if(pOuter)
        return CLASS_E_NOAGGREGATION;

    object = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*object));
    if(!object)
        return E_OUTOFMEMORY;

    object->IXACT3Engine_iface.lpVtbl = &XACT3Engine_Vtbl;

    FACTCreateEngineWithCustomAllocatorEXT(
	0,
        &object->fact_engine,
        XAudio_Internal_Malloc,
        XAudio_Internal_Free,
        XAudio_Internal_Realloc
    );

    hr = IXACT3Engine_QueryInterface(&object->IXACT3Engine_iface, riid, ppobj);
    if(FAILED(hr)){
        HeapFree(GetProcessHeap(), 0, object);
        return hr;
    }

    TRACE("Created XACT version %u: %p\n", 30 + XACT3_VER, object);

    return hr;
}

static HRESULT WINAPI XACT3CF_LockServer(IClassFactory *iface, BOOL dolock)
{
    struct xact3_cf *This = impl_from_IClassFactory(iface);
    FIXME("(%p)->(%d): stub!\n", This, dolock);
    return S_OK;
}

static const IClassFactoryVtbl XACT3CF_Vtbl =
{
    XACT3CF_QueryInterface,
    XACT3CF_AddRef,
    XACT3CF_Release,
    XACT3CF_CreateInstance,
    XACT3CF_LockServer
};

static HRESULT make_xact3_factory(REFIID riid, void **ppv)
{
    HRESULT hr;
    struct xact3_cf *ret = HeapAlloc(GetProcessHeap(), 0, sizeof(struct xact3_cf));
    ret->IClassFactory_iface.lpVtbl = &XACT3CF_Vtbl;
    ret->ref = 0;
    hr = IClassFactory_QueryInterface(&ret->IClassFactory_iface, riid, ppv);
    if(FAILED(hr))
        HeapFree(GetProcessHeap(), 0, ret);
    return hr;
}

HRESULT WINAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void **ppv)
{
    TRACE("(%s, %s, %p)\n", debugstr_guid(rclsid), debugstr_guid(riid), ppv);

    if(IsEqualGUID(rclsid, &CLSID_XACTEngine30) ||
            IsEqualGUID(rclsid, &CLSID_XACTEngine31) ||
            IsEqualGUID(rclsid, &CLSID_XACTEngine32) ||
            IsEqualGUID(rclsid, &CLSID_XACTEngine33) ||
            IsEqualGUID(rclsid, &CLSID_XACTEngine34) ||
            IsEqualGUID(rclsid, &CLSID_XACTEngine35) ||
            IsEqualGUID(rclsid, &CLSID_XACTEngine36) ||
            IsEqualGUID(rclsid, &CLSID_XACTEngine37))
        return make_xact3_factory(riid, ppv);

    return CLASS_E_CLASSNOTAVAILABLE;
}
