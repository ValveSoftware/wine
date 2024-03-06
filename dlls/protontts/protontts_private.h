/*
 * protontts private header file.
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

#ifndef __PROTONTTS_PRIVATE_H
#define __PROTONTTS_PRIVATE_H

#include <stdbool.h>
#include <stdint.h>
#include "windef.h"
#include "winternl.h"

#include "wine/unixlib.h"

void free_tts(void);
HRESULT ttsengine_create(REFIID iid, void **obj);

typedef UINT64 tts_t;
typedef UINT64 tts_voice_t;

struct tts_voice_load_params
{
    tts_t tts;
    const char *model_path;
    INT64 speaker_id;
    tts_voice_t voice;
};

struct tts_voice_get_config_params
{
    tts_voice_t voice;
    float length_scale;
    INT32 sample_rate;
    INT32 sample_width;
    INT32 channels;
};

struct tts_voice_set_config_params
{
    tts_voice_t voice;
    const float *length_scale;
};

struct tts_voice_synthesize_params
{
    tts_voice_t voice;
    const char *text;
    UINT32 size;
    HANDLE abort_event;
};

struct tts_voice_audio_lock_params
{
    tts_voice_t voice;
    void *buf;
    UINT32 size;
    UINT8 done;
};

enum unix_funcs
{
    unix_process_attach,

    unix_tts_create,
    unix_tts_destroy,

    unix_tts_voice_load,
    unix_tts_voice_destroy,
    unix_tts_voice_get_config,
    unix_tts_voice_set_config,
    unix_tts_voice_synthesize,

    unix_tts_voice_audio_lock,
    unix_tts_voice_audio_release,
};

#endif /* __PROTONTTS_PRIVATE_H */
