/*
 * Copyright (c) 2016 Andrew Eikum for CodeWeavers
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

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "x3daudio.h"

#include "wine/debug.h"

#include <F3DAudio.h>

#if XAUDIO2_VER >= 8 || defined X3DAUDIO1_VER
WINE_DEFAULT_DEBUG_CHANNEL(xaudio2);
#endif

#ifdef X3DAUDIO1_VER
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD reason, void *pReserved)
{
    TRACE("(%p, %d, %p)\n", hinstDLL, reason, pReserved);

    switch (reason)
    {
    case DLL_WINE_PREATTACH:
        return FALSE;  /* prefer native version */
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls( hinstDLL );
        break;
    }
    return TRUE;
}
#endif /* X3DAUDIO1_VER */

#if XAUDIO2_VER >= 8
HRESULT CDECL X3DAudioInitialize(UINT32 chanmask, float speedofsound,
        X3DAUDIO_HANDLE handle)
{
    TRACE("0x%x, %f, %p\n", chanmask, speedofsound, handle);
    F3DAudioInitialize(chanmask, speedofsound, handle);
    return S_OK;
}
#endif /* XAUDIO2_VER >= 8 */

#ifdef X3DAUDIO1_VER
void CDECL LEGACY_X3DAudioInitialize(UINT32 chanmask, float speedofsound,
        X3DAUDIO_HANDLE handle)
{
    TRACE("0x%x, %f, %p\n", chanmask, speedofsound, handle);
    F3DAudioInitialize(chanmask, speedofsound, handle);
}
#endif /* X3DAUDIO1_VER */

#if XAUDIO2_VER >= 8 || defined X3DAUDIO1_VER
void CDECL X3DAudioCalculate(const X3DAUDIO_HANDLE handle,
        const X3DAUDIO_LISTENER *listener, const X3DAUDIO_EMITTER *emitter,
        UINT32 flags, X3DAUDIO_DSP_SETTINGS *out)
{
    TRACE("%p, %p, %p, 0x%x, %p\n", handle, listener, emitter, flags, out);
    F3DAudioCalculate(
        handle,
        (const F3DAUDIO_LISTENER*) listener,
        (const F3DAUDIO_EMITTER*) emitter,
        flags,
        (F3DAUDIO_DSP_SETTINGS*) out
    );
}
#endif /* XAUDIO2_VER >= 8 || defined X3DAUDIO1_VER */
