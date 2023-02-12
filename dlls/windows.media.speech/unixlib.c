/*
 * Unixlib for Windows.Media.Speech
 *
 * Copyright 2023 Bernhard Kölbl for CodeWeavers
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <dlfcn.h>

#ifdef SONAME_LIBVOSK
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include <vosk_api.h>
#pragma GCC diagnostic pop
#endif

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "winerror.h"
#include "winternl.h"

#include "wine/debug.h"

#include "unixlib.h"

WINE_DEFAULT_DEBUG_CHANNEL(speech);
#ifdef SONAME_LIBVOSK
WINE_DECLARE_DEBUG_CHANNEL(winediag);

static void *vosk_handle;
#define MAKE_FUNCPTR( f ) static typeof(f) * p_##f
MAKE_FUNCPTR(vosk_model_new);
MAKE_FUNCPTR(vosk_model_free);
MAKE_FUNCPTR(vosk_recognizer_new);
MAKE_FUNCPTR(vosk_recognizer_free);
#undef MAKE_FUNCPTR

static NTSTATUS process_attach( void *args )
{
    if (!(vosk_handle = dlopen(SONAME_LIBVOSK, RTLD_NOW)))
    {
        ERR_(winediag)("Wine is unable to load the Unix side dependencies for speech recognition. "
                       "Make sure Vosk is installed and up to date on your system and try again.\n");
        return STATUS_DLL_NOT_FOUND;
    }

#define LOAD_FUNCPTR( f ) \
    if (!(p_##f = dlsym(vosk_handle, #f))) \
    { \
        ERR("failed to load %s\n", #f); \
        goto error; \
    }
    LOAD_FUNCPTR(vosk_model_new)
    LOAD_FUNCPTR(vosk_model_free)
    LOAD_FUNCPTR(vosk_recognizer_new)
    LOAD_FUNCPTR(vosk_recognizer_free)
#undef LOAD_FUNCPTR

    return STATUS_SUCCESS;

error:
    dlclose(vosk_handle);
    vosk_handle = NULL;
    return STATUS_DLL_NOT_FOUND;
}

static NTSTATUS process_detach( void *args )
{
    if (vosk_handle)
    {
        dlclose(vosk_handle);
        vosk_handle = NULL;
    }
    return STATUS_SUCCESS;
}

#else /* SONAME_LIBVOSK */

#define MAKE_UNSUPPORTED_FUNC( f ) \
    static NTSTATUS f( void *args ) \
    { \
        ERR("wine was compiled without Vosk support. Speech recognition won't work.\n"); \
        return STATUS_NOT_SUPPORTED; \
    }

MAKE_UNSUPPORTED_FUNC(process_attach)
MAKE_UNSUPPORTED_FUNC(process_detach)
#undef MAKE_UNSUPPORTED_FUNC

#endif /* SONAME_LIBVOSK */

const unixlib_entry_t __wine_unix_call_funcs[] =
{
    process_attach,
    process_detach,
};

const unixlib_entry_t __wine_unix_call_wow64_funcs[] =
{
    process_attach,
    process_detach,
};
