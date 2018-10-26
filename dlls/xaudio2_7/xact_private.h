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

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "wine/list.h"

#include "xact3.h"

#include "FAudio/FACT.h"

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

#if XACT3_VER <= 4
extern const IXACT34CueVtbl XACT34Cue_Vtbl DECLSPEC_HIDDEN;
#elif XACT3_VER <= 7
extern const IXACT37CueVtbl XACT37Cue_Vtbl DECLSPEC_HIDDEN;
#endif

/* xact_engine.c */
extern HRESULT make_xact3_factory(REFIID riid, void **ppv) DECLSPEC_HIDDEN;

/* xact_soundbank.c */
extern XACT3SoundBankImpl *impl_from_IXACT3SoundBank(IXACT3SoundBank *iface) DECLSPEC_HIDDEN;
extern const IXACT3SoundBankVtbl XACT3SoundBank_Vtbl DECLSPEC_HIDDEN;

/* xact_wavebank.c */
extern XACT3WaveBankImpl *impl_from_IXACT3WaveBank(IXACT3WaveBank *iface) DECLSPEC_HIDDEN;
extern const IXACT3WaveBankVtbl XACT3WaveBank_Vtbl DECLSPEC_HIDDEN;

/* xact_cue.c */
extern XACT3CueImpl *impl_from_IXACT3Cue(IXACT3Cue *iface) DECLSPEC_HIDDEN;
extern const IXACT3CueVtbl XACT3Cue_Vtbl DECLSPEC_HIDDEN;

/* xact_wave.c */
extern XACT3WaveImpl *impl_from_IXACT3Wave(IXACT3Wave *iface) DECLSPEC_HIDDEN;
extern const IXACT3WaveVtbl XACT3Wave_Vtbl DECLSPEC_HIDDEN;

/* xaudio_allocator.c */
extern void* XAudio_Internal_Malloc(size_t size) DECLSPEC_HIDDEN;
extern void XAudio_Internal_Free(void* ptr) DECLSPEC_HIDDEN;
extern void* XAudio_Internal_Realloc(void* ptr, size_t size) DECLSPEC_HIDDEN;
