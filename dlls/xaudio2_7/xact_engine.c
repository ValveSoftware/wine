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
        const void* pvBuffer, DWORD dwSize, DWORD dwFlags,
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
        const void* pvBuffer, DWORD dwSize, DWORD dwFlags,
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

HRESULT make_xact3_factory(REFIID riid, void **ppv)
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
