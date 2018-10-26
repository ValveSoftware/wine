/*
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

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "wine/list.h"

#include "mmsystem.h"
#include "xaudio2.h"
#include "xapo.h"
#include "devpkey.h"
#include "mmdeviceapi.h"
#include "audioclient.h"

#include "FAudio/FAudio.h"
#include "FAudio/FAPO.h"

typedef struct _XA2XAPOImpl {
    IXAPO *xapo;
    IXAPOParameters *xapo_params;

    LONG ref;

    FAPO FAPO_vtbl;
} XA2XAPOImpl;

typedef struct _XA2XAPOFXImpl {
    IXAPO IXAPO_iface;
    IXAPOParameters IXAPOParameters_iface;

    FAPO *fapo;
} XA2XAPOFXImpl;

typedef struct _XA2VoiceImpl {
    IXAudio2SourceVoice IXAudio2SourceVoice_iface;
#if XAUDIO2_VER == 0
    IXAudio20SourceVoice IXAudio20SourceVoice_iface;
#elif XAUDIO2_VER <= 3
    IXAudio23SourceVoice IXAudio23SourceVoice_iface;
#elif XAUDIO2_VER <= 7
    IXAudio27SourceVoice IXAudio27SourceVoice_iface;
#endif

    IXAudio2SubmixVoice IXAudio2SubmixVoice_iface;
#if XAUDIO2_VER == 0
    IXAudio20SubmixVoice IXAudio20SubmixVoice_iface;
#elif XAUDIO2_VER <= 3
    IXAudio23SubmixVoice IXAudio23SubmixVoice_iface;
#elif XAUDIO2_VER <= 7
    IXAudio27SubmixVoice IXAudio27SubmixVoice_iface;
#endif

    IXAudio2MasteringVoice IXAudio2MasteringVoice_iface;
#if XAUDIO2_VER == 0
    IXAudio20MasteringVoice IXAudio20MasteringVoice_iface;
#elif XAUDIO2_VER <= 3
    IXAudio23MasteringVoice IXAudio23MasteringVoice_iface;
#elif XAUDIO2_VER <= 7
    IXAudio27MasteringVoice IXAudio27MasteringVoice_iface;
#endif

    FAudioVoiceCallback FAudioVoiceCallback_vtbl;
    FAudioEffectChain *effect_chain;

    BOOL in_use;

    CRITICAL_SECTION lock;

    IXAudio2VoiceCallback *cb;

    FAudioVoice *faudio_voice;

    struct list entry;
} XA2VoiceImpl;

typedef struct _IXAudio2Impl {
    IXAudio2 IXAudio2_iface;

#if XAUDIO2_VER == 0
    IXAudio20 IXAudio20_iface;
#elif XAUDIO2_VER <= 2
    IXAudio22 IXAudio22_iface;
#elif XAUDIO2_VER <= 7
    IXAudio27 IXAudio27_iface;
#endif

    CRITICAL_SECTION lock;

    struct list voices;

    FAudio *faudio;

    FAudioEngineCallback FAudioEngineCallback_vtbl;

    XA2VoiceImpl mst;

    DWORD last_query_glitches;

    UINT32 ncbs;
    IXAudio2EngineCallback **cbs;
} IXAudio2Impl;

#if XAUDIO2_VER == 0
extern const IXAudio20SourceVoiceVtbl XAudio20SourceVoice_Vtbl DECLSPEC_HIDDEN;
extern const IXAudio20SubmixVoiceVtbl XAudio20SubmixVoice_Vtbl DECLSPEC_HIDDEN;
extern const IXAudio20MasteringVoiceVtbl XAudio20MasteringVoice_Vtbl DECLSPEC_HIDDEN;
extern XA2VoiceImpl *impl_from_IXAudio20SourceVoice(IXAudio20SourceVoice *iface) DECLSPEC_HIDDEN;
extern XA2VoiceImpl *impl_from_IXAudio20SubmixVoice(IXAudio20SubmixVoice *iface) DECLSPEC_HIDDEN;
extern XA2VoiceImpl *impl_from_IXAudio20MasteringVoice(IXAudio20MasteringVoice *iface) DECLSPEC_HIDDEN;
#elif XAUDIO2_VER <= 3
extern const IXAudio23SourceVoiceVtbl XAudio23SourceVoice_Vtbl DECLSPEC_HIDDEN;
extern const IXAudio23SubmixVoiceVtbl XAudio23SubmixVoice_Vtbl DECLSPEC_HIDDEN;
extern const IXAudio23MasteringVoiceVtbl XAudio23MasteringVoice_Vtbl DECLSPEC_HIDDEN;
extern XA2VoiceImpl *impl_from_IXAudio23SourceVoice(IXAudio23SourceVoice *iface) DECLSPEC_HIDDEN;
extern XA2VoiceImpl *impl_from_IXAudio23SubmixVoice(IXAudio23SubmixVoice *iface) DECLSPEC_HIDDEN;
extern XA2VoiceImpl *impl_from_IXAudio23MasteringVoice(IXAudio23MasteringVoice *iface) DECLSPEC_HIDDEN;
#elif XAUDIO2_VER <= 7
extern const IXAudio27SourceVoiceVtbl XAudio27SourceVoice_Vtbl DECLSPEC_HIDDEN;
extern const IXAudio27SubmixVoiceVtbl XAudio27SubmixVoice_Vtbl DECLSPEC_HIDDEN;
extern const IXAudio27MasteringVoiceVtbl XAudio27MasteringVoice_Vtbl DECLSPEC_HIDDEN;
extern XA2VoiceImpl *impl_from_IXAudio27SourceVoice(IXAudio27SourceVoice *iface) DECLSPEC_HIDDEN;
extern XA2VoiceImpl *impl_from_IXAudio27SubmixVoice(IXAudio27SubmixVoice *iface) DECLSPEC_HIDDEN;
extern XA2VoiceImpl *impl_from_IXAudio27MasteringVoice(IXAudio27MasteringVoice *iface) DECLSPEC_HIDDEN;
#endif

#if XAUDIO2_VER == 0
extern const IXAudio20Vtbl XAudio20_Vtbl DECLSPEC_HIDDEN;
#elif XAUDIO2_VER <= 2
extern const IXAudio22Vtbl XAudio22_Vtbl DECLSPEC_HIDDEN;
#elif XAUDIO2_VER <= 7
extern const IXAudio27Vtbl XAudio27_Vtbl DECLSPEC_HIDDEN;
#endif

/* xaudio_engine.c */
extern HRESULT make_xaudio2_factory(REFIID riid, void **ppv) DECLSPEC_HIDDEN;
extern HRESULT xaudio2_initialize(IXAudio2Impl *This, UINT32 flags, XAUDIO2_PROCESSOR proc) DECLSPEC_HIDDEN;

/* xaudio_voice.c */
extern XA2VoiceImpl *impl_from_IXAudio2Voice(IXAudio2Voice *iface) DECLSPEC_HIDDEN;
extern void destroy_voice(XA2VoiceImpl *This) DECLSPEC_HIDDEN;

/* xaudio_voice_source.c */
extern XA2VoiceImpl *impl_from_IXAudio2SourceVoice(IXAudio2SourceVoice *iface) DECLSPEC_HIDDEN;
extern const IXAudio2SourceVoiceVtbl XAudio2SourceVoice_Vtbl DECLSPEC_HIDDEN;

/* xaudio_voice_submix.c */
extern XA2VoiceImpl *impl_from_IXAudio2SubmixVoice(IXAudio2SubmixVoice *iface) DECLSPEC_HIDDEN;
extern const struct IXAudio2SubmixVoiceVtbl XAudio2SubmixVoice_Vtbl DECLSPEC_HIDDEN;

/* xaudio_voice_master.c */
extern XA2VoiceImpl *impl_from_IXAudio2MasteringVoice(IXAudio2MasteringVoice *iface) DECLSPEC_HIDDEN;
extern const struct IXAudio2MasteringVoiceVtbl XAudio2MasteringVoice_Vtbl DECLSPEC_HIDDEN;

/* xaudio_voice_wrapeffect.c */
extern FAudioEffectChain *wrap_effect_chain(const XAUDIO2_EFFECT_CHAIN *pEffectChain) DECLSPEC_HIDDEN;
extern void free_effect_chain(FAudioEffectChain *chain) DECLSPEC_HIDDEN;

/* xaudio_voice_wrapsend.c */
extern FAudioVoiceSends *wrap_voice_sends(const XAUDIO2_VOICE_SENDS *sends) DECLSPEC_HIDDEN;
extern void free_voice_sends(FAudioVoiceSends *sends) DECLSPEC_HIDDEN;

/* xaudio_callbacks.c */
extern const FAudioVoiceCallback FAudioVoiceCallback_Vtbl DECLSPEC_HIDDEN;
extern const FAudioEngineCallback FAudioEngineCallback_Vtbl DECLSPEC_HIDDEN;

/* xapo.c */
extern HRESULT make_xapo_factory(REFCLSID clsid, REFIID riid, void **ppv) DECLSPEC_HIDDEN;

/* xaudio_allocator.c */
extern void* XAudio_Internal_Alloc(size_t size) DECLSPEC_HIDDEN;
extern void XAudio_Internal_Free(void* ptr) DECLSPEC_HIDDEN;
extern void* XAudio_Internal_Realloc(void* ptr, size_t size) DECLSPEC_HIDDEN;
