/*
 * Copyright (c) 2015 Mark Harmstone
 * Copyright (c) 2015 Andrew Eikum for CodeWeavers
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

#include "xaudio_private.h"

#include "ole2.h"
#include "rpcproxy.h"

#include "wine/debug.h"
#include "wine/heap.h"

FAudioVoiceSends *wrap_voice_sends(const XAUDIO2_VOICE_SENDS *sends)
{
    FAudioVoiceSends *ret;
    int i;

    if(!sends)
        return NULL;

    ret = heap_alloc(sizeof(*ret) + sends->SendCount * sizeof(FAudioSendDescriptor));
    ret->SendCount = sends->SendCount;
    ret->pSends = (FAudioSendDescriptor*)(ret + 1);
    for(i = 0; i < sends->SendCount; ++i){
        XA2VoiceImpl *voice = impl_from_IXAudio2Voice(sends->pSends[i].pOutputVoice);
        ret->pSends[i].pOutputVoice = voice->faudio_voice;
        ret->pSends[i].Flags = sends->pSends[i].Flags;
    }
    return ret;
}

void free_voice_sends(FAudioVoiceSends *sends)
{
    if(!sends)
        return;
    heap_free(sends);
}
