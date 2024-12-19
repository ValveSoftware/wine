/*
 * protontts unixlib.
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

#if 0
#pragma makedep unix
#endif

#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <sys/stat.h>

#include <piper/piper_c.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "winternl.h"

#include "wine/debug.h"

#include "protontts_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(protontts);

static inline void touch_tts_used_tag(void)
{
    const char *e;


    if ((e = getenv("STEAM_COMPAT_TRANSCODED_MEDIA_PATH")))
    {
        char buffer[PATH_MAX];
        int fd;

        snprintf(buffer, sizeof(buffer), "%s/tts-used", e);

        fd = open(buffer, O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
        if (fd == -1)
        {
            ERR("Failed to open/create \"%s/tts-used\"", e);
            return;
        }

        futimens(fd, NULL);

        close(fd);
    }
    else
    {
        ERR("STEAM_COMPAT_TRANSCODED_MEDIA_PATH not set, cannot create tts-used file");
    }
}

struct tts_audio_buf
{
    pthread_mutex_t mutex;
    pthread_cond_t empty_cond;
    void *buf;
    UINT32 size;
    bool done;
};

struct tts_voice
{
    Piper *piper;
    PiperVoice *voice;

    struct tts_audio_buf audio;
};

static ULONG_PTR zero_bits = 0;

static inline Piper *get_piper(tts_t tts)
{
    return (Piper *)(ULONG_PTR)tts;
}

static inline struct tts_voice *get_voice(tts_voice_t voice)
{
    return (struct tts_voice *)(ULONG_PTR)voice;
}

static NTSTATUS process_attach(void *args)
{
#ifdef _WIN64
    if (NtCurrentTeb()->WowTebOffset)
    {
        SYSTEM_BASIC_INFORMATION info;

        NtQuerySystemInformation(SystemEmulationBasicInformation, &info, sizeof(info), NULL);
        zero_bits = (ULONG_PTR)info.HighestUserAddress | 0x7fffffff;
    }
#endif
    touch_tts_used_tag();
    return STATUS_SUCCESS;
}

static NTSTATUS tts_create(void *args)
{
    Piper *piper = piperInitialize(NULL);

    *(tts_t *)args = (tts_t)(ULONG_PTR)piper;
    return piper ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

static NTSTATUS tts_destroy(void *args)
{
    Piper *piper = get_piper(*(tts_t *)args);

    piperTerminate(piper);
    return STATUS_SUCCESS;
}

static NTSTATUS tts_voice_load(void *args)
{
    struct tts_voice_load_params *params = args;
    const char *voice_files_dir = getenv("PROTON_VOICE_FILES");
    char *model_path;
    struct tts_voice *voice;

    TRACE("(%p).\n", args);

    if (!voice_files_dir)
    {
        ERR("Proton voice files not found.\n");
        return STATUS_UNSUCCESSFUL;
    }
    if (!(model_path = malloc(strlen(voice_files_dir) + 1 + strlen(params->model_path) + 1)))
        return STATUS_NO_MEMORY;
    sprintf(model_path, "%s/%s", voice_files_dir, params->model_path);

    if (!(voice = calloc(1, sizeof(*voice))))
    {
        free(model_path);
        return STATUS_NO_MEMORY;
    }

    voice->piper = get_piper(params->tts);
    voice->voice = piperLoadVoice(voice->piper, model_path, NULL, params->speaker_id, FALSE);
    free(model_path);
    if (!voice->voice)
    {
        free(voice);
        return STATUS_UNSUCCESSFUL;
    }

    pthread_mutex_init(&voice->audio.mutex, NULL);
    pthread_cond_init(&voice->audio.empty_cond, NULL);

    params->voice = (tts_voice_t)(ULONG_PTR)voice;

    TRACE("OK.\n");
    return STATUS_SUCCESS;
}

static NTSTATUS tts_voice_destroy(void *args)
{
    struct tts_voice *voice = get_voice(*(tts_voice_t *)args);

    piperFreeVoice(voice->voice);

    pthread_mutex_destroy(&voice->audio.mutex);
    pthread_cond_destroy(&voice->audio.empty_cond);

    free(voice);

    return STATUS_SUCCESS;
}

static NTSTATUS tts_voice_get_config(void *args)
{
    struct tts_voice_get_config_params *params = args;
    struct tts_voice *voice = get_voice(params->voice);
    PiperSynthesisConfig config;

    piperGetVoiceSynthesisConfig(voice->voice, &config);

    params->length_scale = config.lengthScale;
    params->sample_rate  = config.sampleRate;
    params->sample_width = config.sampleWidth;
    params->channels     = config.channels;

    return STATUS_SUCCESS;
}

static NTSTATUS tts_voice_set_config(void *args)
{
    struct tts_voice_set_config_params *params = args;
    struct tts_voice *voice = get_voice(params->voice);

    piperSetVoiceSynthesisConfig(voice->voice, NULL, params->length_scale, NULL, NULL, NULL);

    return STATUS_SUCCESS;
}

struct tts_to_audio_cb_params
{
    struct tts_audio_buf *audio;
    HANDLE abort_event;
};

static bool tts_to_audio_cb(const int16_t *data, size_t length, void *user_data)
{
    struct tts_to_audio_cb_params *params = user_data;
    const UINT32 size = sizeof(*data) * length;
    SIZE_T region_size = size;
    LARGE_INTEGER timeout = { .QuadPart = 0 };

    TRACE("(%p, %lu, %p).\n", data, length, user_data);

    pthread_mutex_lock(&params->audio->mutex);

    while (params->audio->buf)
        pthread_cond_wait(&params->audio->empty_cond, &params->audio->mutex);

    if (NtAllocateVirtualMemory(NtCurrentProcess(), &params->audio->buf, zero_bits, &region_size, MEM_COMMIT, PAGE_READWRITE))
    {
        pthread_mutex_unlock(&params->audio->mutex);
        ERR("failed to allocate audio buffer\n");
        return false;
    }
    memcpy(params->audio->buf, data, size);
    params->audio->size = size;

    pthread_mutex_unlock(&params->audio->mutex);

    return NtWaitForSingleObject(params->abort_event, FALSE, &timeout) == STATUS_TIMEOUT;
}

static NTSTATUS tts_voice_synthesize(void *args)
{
    struct tts_voice_synthesize_params *params = args;
    struct tts_voice *voice = get_voice(params->voice);
    struct tts_to_audio_cb_params cb_params =
    {
        .audio = &voice->audio,
        .abort_event = params->abort_event,
    };
    PiperSynthesisResult result;
    bool success;

    TRACE("(%p).\n", args);

    voice->audio.done = false;
    success = piperTextToAudio(voice->piper, voice->voice, params->text, &result, &cb_params, tts_to_audio_cb);
    voice->audio.done = true;

    return success ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

static NTSTATUS tts_voice_audio_lock(void *args)
{
    struct tts_voice_audio_lock_params *params = args;
    struct tts_voice *voice = get_voice(params->voice);

    TRACE("(%p).\n", args);

    pthread_mutex_lock(&voice->audio.mutex);

    params->buf  = voice->audio.buf;
    params->size = voice->audio.size;
    params->done = voice->audio.done;

    return STATUS_SUCCESS;
}

static NTSTATUS tts_voice_audio_release(void *args)
{
    struct tts_voice *voice = get_voice(*(tts_voice_t *)args);
    SIZE_T size = 0;

    TRACE("(%p).\n", args);

    if (voice->audio.buf)
        NtFreeVirtualMemory(NtCurrentProcess(), &voice->audio.buf, &size, MEM_RELEASE);
    voice->audio.buf = NULL;
    voice->audio.size = 0;

    pthread_cond_signal(&voice->audio.empty_cond);
    pthread_mutex_unlock(&voice->audio.mutex);

    return STATUS_SUCCESS;
}

const unixlib_entry_t __wine_unix_call_funcs[] =
{
    process_attach,

    tts_create,
    tts_destroy,

    tts_voice_load,
    tts_voice_destroy,
    tts_voice_get_config,
    tts_voice_set_config,
    tts_voice_synthesize,

    tts_voice_audio_lock,
    tts_voice_audio_release,
};

#ifdef _WIN64

typedef ULONG PTR32;

static NTSTATUS wow64_tts_voice_load(void *args)
{
    struct
    {
        tts_t tts;
        PTR32 model_path;
        INT64 speaker_id;
        tts_voice_t voice;
    } *params32 = args;
    struct tts_voice_load_params params =
    {
        .tts = params32->tts,
        .model_path = ULongToPtr(params32->model_path),
        .speaker_id = params32->speaker_id,
    };
    NTSTATUS ret;

    ret = tts_voice_load(&params);
    params32->voice = params.voice;
    return ret;
}

static NTSTATUS wow64_tts_voice_set_config(void *args)
{
    struct
    {
        tts_voice_t voice;
        PTR32 length_scale;
    } *params32 = args;
    struct tts_voice_set_config_params params =
    {
        .voice = params32->voice,
        .length_scale = ULongToPtr(params32->length_scale),
    };

    return tts_voice_set_config(&params);
}


static NTSTATUS wow64_tts_voice_synthesize(void *args)
{
    struct
    {
        tts_voice_t voice;
        PTR32 text;
        UINT32 size;
        PTR32 abort_event;
    } *params32 = args;
    struct tts_voice_synthesize_params params =
    {
        .voice = params32->voice,
        .text = ULongToPtr(params32->text),
        .size = params32->size,
        .abort_event = ULongToHandle(params32->abort_event),
    };

    return tts_voice_synthesize(&params);
}

static NTSTATUS wow64_tts_voice_audio_lock(void *args)
{
    struct
    {
        tts_voice_t voice;
        PTR32 buf;
        UINT32 size;
        UINT8 done;
    } *params32 = args;
    struct tts_voice_audio_lock_params params =
    {
        .voice = params32->voice,
    };
    NTSTATUS ret;

    ret = tts_voice_audio_lock(&params);
    params32->buf = PtrToUlong(params.buf);
    params32->size = params.size;
    params32->done = params.done;
    return ret;
}

const unixlib_entry_t __wine_unix_call_wow64_funcs[] =
{
    process_attach,

    tts_create,
    tts_destroy,

    wow64_tts_voice_load,
    tts_voice_destroy,
    tts_voice_get_config,
    wow64_tts_voice_set_config,
    wow64_tts_voice_synthesize,

    wow64_tts_voice_audio_lock,
    tts_voice_audio_release,
};

#endif /* _WIN64 */
