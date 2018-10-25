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

static inline XA2VoiceImpl *impl_from_FAudioVoiceCallback(FAudioVoiceCallback *iface)
{
    return CONTAINING_RECORD(iface, XA2VoiceImpl, FAudioVoiceCallback_vtbl);
}

/* TODO callback v20 support */
static void FAUDIOCALL XA2VCB_OnVoiceProcessingPassStart(FAudioVoiceCallback *iface,
        UINT32 BytesRequired)
{
    XA2VoiceImpl *This = impl_from_FAudioVoiceCallback(iface);
    TRACE("%p\n", This);
    if(This->cb)
        IXAudio2VoiceCallback_OnVoiceProcessingPassStart(This->cb, BytesRequired);
}

static void FAUDIOCALL XA2VCB_OnVoiceProcessingPassEnd(FAudioVoiceCallback *iface)
{
    XA2VoiceImpl *This = impl_from_FAudioVoiceCallback(iface);
    TRACE("%p\n", This);
    if(This->cb)
        IXAudio2VoiceCallback_OnVoiceProcessingPassEnd(This->cb);
}

static void FAUDIOCALL XA2VCB_OnStreamEnd(FAudioVoiceCallback *iface)
{
    XA2VoiceImpl *This = impl_from_FAudioVoiceCallback(iface);
    TRACE("%p\n", This);
    if(This->cb)
        IXAudio2VoiceCallback_OnStreamEnd(This->cb);
}

static void FAUDIOCALL XA2VCB_OnBufferStart(FAudioVoiceCallback *iface,
        void *pBufferContext)
{
    XA2VoiceImpl *This = impl_from_FAudioVoiceCallback(iface);
    TRACE("%p\n", This);
    if(This->cb)
        IXAudio2VoiceCallback_OnBufferStart(This->cb, pBufferContext);
}

static void FAUDIOCALL XA2VCB_OnBufferEnd(FAudioVoiceCallback *iface,
        void *pBufferContext)
{
    XA2VoiceImpl *This = impl_from_FAudioVoiceCallback(iface);
    TRACE("%p\n", This);
    if(This->cb)
        IXAudio2VoiceCallback_OnBufferEnd(This->cb, pBufferContext);
}

static void FAUDIOCALL XA2VCB_OnLoopEnd(FAudioVoiceCallback *iface,
        void *pBufferContext)
{
    XA2VoiceImpl *This = impl_from_FAudioVoiceCallback(iface);
    TRACE("%p\n", This);
    if(This->cb)
        IXAudio2VoiceCallback_OnLoopEnd(This->cb, pBufferContext);
}

static void FAUDIOCALL XA2VCB_OnVoiceError(FAudioVoiceCallback *iface,
        void *pBufferContext, unsigned int Error)
{
    XA2VoiceImpl *This = impl_from_FAudioVoiceCallback(iface);
    TRACE("%p\n", This);
    if(This->cb)
        IXAudio2VoiceCallback_OnVoiceError(This->cb, pBufferContext, (HRESULT)Error);
}

const FAudioVoiceCallback FAudioVoiceCallback_Vtbl = {
    XA2VCB_OnBufferEnd,
    XA2VCB_OnBufferStart,
    XA2VCB_OnLoopEnd,
    XA2VCB_OnStreamEnd,
    XA2VCB_OnVoiceError,
    XA2VCB_OnVoiceProcessingPassEnd,
    XA2VCB_OnVoiceProcessingPassStart
};

static inline IXAudio2Impl *impl_from_FAudioEngineCallback(FAudioEngineCallback *iface)
{
    return CONTAINING_RECORD(iface, IXAudio2Impl, FAudioEngineCallback_vtbl);
}

static void FAUDIOCALL XA2ECB_OnProcessingPassStart(FAudioEngineCallback *iface)
{
    IXAudio2Impl *This = impl_from_FAudioEngineCallback(iface);
    int i;
    TRACE("%p\n", This);
    for(i = 0; i < This->ncbs && This->cbs[i]; ++i)
        IXAudio2EngineCallback_OnProcessingPassStart(This->cbs[i]);
}

static void FAUDIOCALL XA2ECB_OnProcessingPassEnd(FAudioEngineCallback *iface)
{
    IXAudio2Impl *This = impl_from_FAudioEngineCallback(iface);
    int i;
    TRACE("%p\n", This);
    for(i = 0; i < This->ncbs && This->cbs[i]; ++i)
        IXAudio2EngineCallback_OnProcessingPassEnd(This->cbs[i]);
}

static void FAUDIOCALL XA2ECB_OnCriticalError(FAudioEngineCallback *iface,
        uint32_t error)
{
    IXAudio2Impl *This = impl_from_FAudioEngineCallback(iface);
    int i;
    TRACE("%p\n", This);
    for(i = 0; i < This->ncbs && This->cbs[i]; ++i)
        IXAudio2EngineCallback_OnCriticalError(This->cbs[i], error);
}

const FAudioEngineCallback FAudioEngineCallback_Vtbl = {
    XA2ECB_OnCriticalError,
    XA2ECB_OnProcessingPassEnd,
    XA2ECB_OnProcessingPassStart
};
