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

WINE_DEFAULT_DEBUG_CHANNEL(xaudio2);

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

void destroy_voice(XA2VoiceImpl *This)
{
    FAudioVoice_DestroyVoice(This->faudio_voice);
    free_effect_chain(This->effect_chain);
    This->effect_chain = NULL;
    This->in_use = FALSE;
}

XA2VoiceImpl *impl_from_IXAudio2Voice(IXAudio2Voice *iface)
{
    if(iface->lpVtbl == (void*)&XAudio2SourceVoice_Vtbl)
        return impl_from_IXAudio2SourceVoice((IXAudio2SourceVoice*)iface);
    if(iface->lpVtbl == (void*)&XAudio2MasteringVoice_Vtbl)
        return impl_from_IXAudio2MasteringVoice((IXAudio2MasteringVoice*)iface);
    if(iface->lpVtbl == (void*)&XAudio2SubmixVoice_Vtbl)
        return impl_from_IXAudio2SubmixVoice((IXAudio2SubmixVoice*)iface);
#if XAUDIO2_VER == 0
    if(iface->lpVtbl == (void*)&XAudio20SourceVoice_Vtbl)
        return impl_from_IXAudio20SourceVoice((IXAudio20SourceVoice*)iface);
    if(iface->lpVtbl == (void*)&XAudio20SubmixVoice_Vtbl)
        return impl_from_IXAudio20SubmixVoice((IXAudio20SubmixVoice*)iface);
    if(iface->lpVtbl == (void*)&XAudio20MasteringVoice_Vtbl)
        return impl_from_IXAudio20MasteringVoice((IXAudio20MasteringVoice*)iface);
#elif XAUDIO2_VER <= 3
    if(iface->lpVtbl == (void*)&XAudio23SourceVoice_Vtbl)
        return impl_from_IXAudio23SourceVoice((IXAudio23SourceVoice*)iface);
    if(iface->lpVtbl == (void*)&XAudio23SubmixVoice_Vtbl)
        return impl_from_IXAudio23SubmixVoice((IXAudio23SubmixVoice*)iface);
    if(iface->lpVtbl == (void*)&XAudio23MasteringVoice_Vtbl)
        return impl_from_IXAudio23MasteringVoice((IXAudio23MasteringVoice*)iface);
#elif XAUDIO2_VER <= 7
    if(iface->lpVtbl == (void*)&XAudio27SourceVoice_Vtbl)
        return impl_from_IXAudio27SourceVoice((IXAudio27SourceVoice*)iface);
    if(iface->lpVtbl == (void*)&XAudio27SubmixVoice_Vtbl)
        return impl_from_IXAudio27SubmixVoice((IXAudio27SubmixVoice*)iface);
    if(iface->lpVtbl == (void*)&XAudio27MasteringVoice_Vtbl)
        return impl_from_IXAudio27MasteringVoice((IXAudio27MasteringVoice*)iface);
#endif
    ERR("invalid IXAudio2Voice pointer: %p\n", iface);
    return NULL;
}
