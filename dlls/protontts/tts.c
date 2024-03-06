/*
 * protontts SAPI engine implementation.
 *
 * Copyright 2023 Shaun Ren for CodeWeavers
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

#include <stdarg.h>

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "objbase.h"

#include "sapiddk.h"
#include "sperror.h"

#include "wine/debug.h"

#include "protontts_private.h"

#include "initguid.h"

WINE_DEFAULT_DEBUG_CHANNEL(protontts);

DEFINE_GUID(SPDFID_WaveFormatEx, 0xc31adbae,0x527f,0x4ff5,0xa2,0x30,0xf6,0x2b,0xb6,0x1f,0xf7,0x0c);

struct ttsengine
{
    ISpTTSEngine ISpTTSEngine_iface;
    ISpObjectWithToken ISpObjectWithToken_iface;
    LONG ref;

    ISpObjectToken *token;
    INT64 speaker_id;
    float base_length_scale;
    tts_voice_t voice;
};

static INIT_ONCE init_once = INIT_ONCE_STATIC_INIT;
static tts_t tts = 0;

static inline struct ttsengine *impl_from_ISpTTSEngine(ISpTTSEngine *iface)
{
    return CONTAINING_RECORD(iface, struct ttsengine, ISpTTSEngine_iface);
}

static inline struct ttsengine *impl_from_ISpObjectWithToken(ISpObjectWithToken *iface)
{
    return CONTAINING_RECORD(iface, struct ttsengine, ISpObjectWithToken_iface);
}

static BOOL WINAPI init_tts(INIT_ONCE *once, void *param, void **ctx)
{
    WINE_UNIX_CALL(unix_tts_create, &tts);
    return tts != 0;
}

void free_tts(void)
{
    if (tts) WINE_UNIX_CALL(unix_tts_destroy, &tts);
}

static tts_voice_t tts_voice_load(tts_t tts, const char *model_path, INT64 speaker_id)
{
    struct tts_voice_load_params params =
    {
        .tts = tts,
        .model_path = model_path,
        .speaker_id = speaker_id,
        .voice = 0,
    };

    WINE_UNIX_CALL(unix_tts_voice_load, &params);
    return params.voice;
}

static void tts_voice_set_length_scale(tts_voice_t voice, float length_scale)
{
    struct tts_voice_set_config_params params =
    {
        .voice = voice,
        .length_scale = &length_scale,
    };

    WINE_UNIX_CALL(unix_tts_voice_set_config, &params);
}

static void tts_voice_audio_lock(tts_voice_t voice, void **buf, UINT32 *size, bool *done)
{
    struct tts_voice_audio_lock_params params =
    {
        .voice = voice,
    };

    WINE_UNIX_CALL(unix_tts_voice_audio_lock, &params);
    *buf = params.buf;
    *size = params.size;
    *done = params.done;

    TRACE("buf = %p, size = %u, done = %d.\n", params.buf, params.size, params.done);
}

static HRESULT WINAPI ttsengine_QueryInterface(ISpTTSEngine *iface, REFIID iid, void **obj)
{
    struct ttsengine *This = impl_from_ISpTTSEngine(iface);

    TRACE("(%p)->(%s %p)\n", This, debugstr_guid(iid), obj);

    if (IsEqualIID(iid, &IID_IUnknown) ||
        IsEqualIID(iid, &IID_ISpTTSEngine))
    {
        *obj = &This->ISpTTSEngine_iface;
    }
    else if (IsEqualIID(iid, &IID_ISpObjectWithToken))
        *obj = &This->ISpObjectWithToken_iface;
    else
    {
        *obj = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown *)*obj);
    return S_OK;
}

static ULONG WINAPI ttsengine_AddRef(ISpTTSEngine *iface)
{
    struct ttsengine *This = impl_from_ISpTTSEngine(iface);
    ULONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%lu\n", This, ref);

    return ref;
}

static ULONG WINAPI ttsengine_Release(ISpTTSEngine *iface)
{
    struct ttsengine *This = impl_from_ISpTTSEngine(iface);
    ULONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%lu\n", This, ref);

    if (!ref)
    {
        if (This->token) ISpObjectToken_Release(This->token);
        if (This->voice) WINE_UNIX_CALL(unix_tts_voice_destroy, &This->voice);

        free(This);
    }

    return ref;
}

static DWORD CALLBACK synthesize_thread_proc(void *params)
{
    SetThreadDescription(GetCurrentThread(), L"protontts_synthesize");
    return WINE_UNIX_CALL(unix_tts_voice_synthesize, params);
}

static HRESULT WINAPI ttsengine_Speak(ISpTTSEngine *iface, DWORD flags, REFGUID fmtid,
                                      const WAVEFORMATEX *wfx, const SPVTEXTFRAG *frag_list,
                                      ISpTTSEngineSite *site)
{
    struct ttsengine *This = impl_from_ISpTTSEngine(iface);
    HANDLE abort_event;
    HANDLE thread = NULL;
    char *text = NULL;
    HRESULT hr = S_OK;

    TRACE("(%p, %#lx, %s, %p, %p, %p).\n", iface, flags, debugstr_guid(fmtid), wfx, frag_list, site);

    if (!This->voice)
        return SPERR_UNINITIALIZED;

    if (!(abort_event = CreateEventW(NULL, FALSE, FALSE, NULL)))
        return HRESULT_FROM_WIN32(GetLastError());

    tts_voice_set_length_scale(This->voice, This->base_length_scale);
    for (; frag_list; frag_list = frag_list->pNext)
    {
        struct tts_voice_synthesize_params params;
        bool done;

        if (ISpTTSEngineSite_GetActions(site) & SPVES_ABORT)
            return S_OK;

        params.size = WideCharToMultiByte(CP_UTF8, 0, frag_list->pTextStart, frag_list->ulTextLen, NULL, 0, NULL, NULL) + 1;
        if (!(text = malloc(params.size)))
        {
            hr = E_OUTOFMEMORY;
            goto done;
        }
        WideCharToMultiByte(CP_UTF8, 0, frag_list->pTextStart, frag_list->ulTextLen, text, params.size, NULL, NULL);
        text[params.size - 1] = '\0';

        params.voice       = This->voice;
        params.text        = text;
        params.abort_event = abort_event;

        if (!(thread = CreateThread(NULL, 0, synthesize_thread_proc, &params, 0, NULL)))
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
            goto done;
        }

        for (done = false; !done;)
        {
            void *buf;
            UINT32 size;

            Sleep(50);

            if (ISpTTSEngineSite_GetActions(site) & SPVES_ABORT)
            {
                SetEvent(abort_event);
                goto done;
            }

            tts_voice_audio_lock(This->voice, &buf, &size, &done);
            if (buf)
                hr = ISpTTSEngineSite_Write(site, buf, size, NULL);
            WINE_UNIX_CALL(unix_tts_voice_audio_release, &This->voice);

            if (FAILED(hr))
            {
                SetEvent(abort_event);
                goto done;
            }
        }

        CloseHandle(thread);
        thread = NULL;
    }

done:
    if (thread)
    {
        WaitForSingleObject(thread, INFINITE);
        CloseHandle(thread);
    }
    CloseHandle(abort_event);
    free(text);

    return hr;
}

static HRESULT WINAPI ttsengine_GetOutputFormat(ISpTTSEngine *iface, const GUID *fmtid,
                                                const WAVEFORMATEX *wfx, GUID *out_fmtid,
                                                WAVEFORMATEX **out_wfx)
{
    struct ttsengine *This = impl_from_ISpTTSEngine(iface);
    struct tts_voice_get_config_params params =
    {
        .voice = This->voice,
    };

    TRACE("(%p, %s, %p, %p, %p).\n", iface, debugstr_guid(fmtid), wfx, out_fmtid, out_wfx);

    if (!This->voice)
        return SPERR_UNINITIALIZED;

    *out_fmtid = SPDFID_WaveFormatEx;
    if (!(*out_wfx = CoTaskMemAlloc(sizeof(WAVEFORMATEX))))
        return E_OUTOFMEMORY;

    WINE_UNIX_CALL(unix_tts_voice_get_config, &params);

    (*out_wfx)->wFormatTag      = WAVE_FORMAT_PCM;
    (*out_wfx)->nChannels       = params.channels;
    (*out_wfx)->nSamplesPerSec  = params.sample_rate;
    (*out_wfx)->wBitsPerSample  = params.sample_width * 8;
    (*out_wfx)->nBlockAlign     = params.sample_width * params.channels;
    (*out_wfx)->nAvgBytesPerSec = params.sample_rate * params.sample_width * params.channels;
    (*out_wfx)->cbSize          = 0;

    return S_OK;
}

static ISpTTSEngineVtbl ttsengine_vtbl =
{
    ttsengine_QueryInterface,
    ttsengine_AddRef,
    ttsengine_Release,
    ttsengine_Speak,
    ttsengine_GetOutputFormat,
};

static HRESULT WINAPI objwithtoken_QueryInterface(ISpObjectWithToken *iface, REFIID iid, void **obj)
{
    struct ttsengine *This = impl_from_ISpObjectWithToken(iface);

    TRACE("(%p, %s, %p).\n", iface, debugstr_guid(iid), obj);

    return ISpTTSEngine_QueryInterface(&This->ISpTTSEngine_iface, iid, obj);
}

static ULONG WINAPI objwithtoken_AddRef(ISpObjectWithToken *iface)
{
    struct ttsengine *This = impl_from_ISpObjectWithToken(iface);

    TRACE("(%p).\n", iface);

    return ISpTTSEngine_AddRef(&This->ISpTTSEngine_iface);
}

static ULONG WINAPI objwithtoken_Release(ISpObjectWithToken *iface)
{
    struct ttsengine *This = impl_from_ISpObjectWithToken(iface);

    TRACE("(%p).\n", iface);

    return ISpTTSEngine_Release(&This->ISpTTSEngine_iface);
}

static HRESULT WINAPI objwithtoken_SetObjectToken(ISpObjectWithToken *iface, ISpObjectToken *token)
{
    struct ttsengine *This = impl_from_ISpObjectWithToken(iface);
    char *model_path = NULL;
    size_t model_path_size;
    WCHAR *value;
    int ret;
    HRESULT hr = S_OK;

    TRACE("(%p, %p).\n", iface, token);

    if (!token)
        return E_INVALIDARG;
    if (This->token)
        return SPERR_ALREADY_INITIALIZED;

    if (FAILED(hr = ISpObjectToken_GetStringValue(token, L"ModelPath", &value)))
        return hr;

    model_path_size = WideCharToMultiByte(CP_UTF8, 0, value, -1, NULL, 0, NULL, NULL);
    if (!(model_path = malloc(model_path_size)))
    {
        CoTaskMemFree(value);
        return E_OUTOFMEMORY;
    }
    WideCharToMultiByte(CP_UTF8, 0, value, -1, model_path, model_path_size, NULL, NULL);

    CoTaskMemFree(value);

    hr = ISpObjectToken_GetStringValue(token, L"SpeakerID", &value);
    if (FAILED(hr) && hr != SPERR_NOT_FOUND)
        goto done;
    else if (SUCCEEDED(hr))
    {
        ret = swscanf(value, L"%I64d", &This->speaker_id);
        CoTaskMemFree(value);
        if (ret != 1)
        {
            hr = E_INVALIDARG;
            goto done;
        }
    }

    hr = ISpObjectToken_GetStringValue(token, L"LengthScale", &value);
    if (FAILED(hr) && hr != SPERR_NOT_FOUND)
        goto done;
    else if (SUCCEEDED(hr))
    {
        ret = swscanf(value, L"%f", &This->base_length_scale);
        CoTaskMemFree(value);
        if (ret != 1)
        {
            hr = E_INVALIDARG;
            goto done;
        }
    }

    This->voice = tts_voice_load(tts, model_path, This->speaker_id);
    if (!This->voice)
    {
        hr = E_FAIL;
        goto done;
    }

    ISpObjectToken_AddRef(token);
    This->token = token;

done:
    free(model_path);
    return hr;
}

static HRESULT WINAPI objwithtoken_GetObjectToken(ISpObjectWithToken *iface, ISpObjectToken **token)
{
    struct ttsengine *This = impl_from_ISpObjectWithToken(iface);

    TRACE("(%p, %p).\n", iface, token);

    if (!token)
        return E_POINTER;

    *token = This->token;
    if (*token)
    {
        ISpObjectToken_AddRef(*token);
        return S_OK;
    }
    else
        return S_FALSE;
}

static const ISpObjectWithTokenVtbl objwithtoken_vtbl =
{
    objwithtoken_QueryInterface,
    objwithtoken_AddRef,
    objwithtoken_Release,
    objwithtoken_SetObjectToken,
    objwithtoken_GetObjectToken
};

HRESULT ttsengine_create(REFIID iid, void **obj)
{
    struct ttsengine *This;
    HRESULT hr;

    if (!InitOnceExecuteOnce(&init_once, init_tts, NULL, NULL) || !tts)
        return E_FAIL;

    if (!(This = malloc(sizeof(*This))))
        return E_OUTOFMEMORY;

    This->ISpTTSEngine_iface.lpVtbl = &ttsengine_vtbl;
    This->ISpObjectWithToken_iface.lpVtbl = &objwithtoken_vtbl;
    This->ref = 1;

    This->token = NULL;
    This->speaker_id = 0;
    This->base_length_scale = 1.0f;
    This->voice = 0;

    hr = ISpTTSEngine_QueryInterface(&This->ISpTTSEngine_iface, iid, obj);
    ISpTTSEngine_Release(&This->ISpTTSEngine_iface);
    return hr;
}
