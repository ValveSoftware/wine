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
#include <stdarg.h>
#include <string.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <sys/stat.h>

#ifdef SONAME_LIBVOSK
#include <vosk_api.h>
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

static inline speech_recognizer_handle vosk_recognizer_to_handle( VoskRecognizer *recognizer )
{
    return (speech_recognizer_handle)(UINT_PTR)recognizer;
}

static inline VoskRecognizer *vosk_recognizer_from_handle( speech_recognizer_handle handle )
{
    return (VoskRecognizer *)(UINT_PTR)handle;
}

static NTSTATUS find_model_by_locale_and_path( const char *path, const char *locale, VoskModel **model )
{
    static const char *vosk_model_identifier_small = "vosk-model-small-";
    static const char *vosk_model_identifier = "vosk-model-";
    size_t ident_small_len = strlen(vosk_model_identifier_small);
    size_t ident_len = strlen(vosk_model_identifier);
    char *ent_name, *model_path, *best_match, *delim;
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    struct dirent *dirent;
    size_t path_len, len;
    DIR *dir;

    TRACE("path %s, locale %s, model %p.\n", path, debugstr_a(locale), model);

    if (!path || !locale || (len = strlen(locale)) < 4)
        return STATUS_UNSUCCESSFUL;

    if (!(dir = opendir(path)))
        return STATUS_UNSUCCESSFUL;

    delim = strchr(locale, '-');
    path_len = strlen(path);
    best_match = NULL;
    *model = NULL;

    while ((dirent = readdir(dir)))
    {
        ent_name = dirent->d_name;

        if (!strncmp(ent_name, vosk_model_identifier_small, ident_small_len))
            ent_name += ident_small_len;
        else if (!strncmp(ent_name, vosk_model_identifier, ident_len))
            ent_name += ident_len;
        else
            continue;

        /*
         * Find the first matching model for lang and region (en-us).
         * If there isn't any, pick the first one just matching lang (en).
         */
        if (!strncmp(ent_name, locale, len))
        {
            if (best_match) free(best_match);
            best_match = strdup(dirent->d_name);
            break;
        }

        if (!best_match && !strncmp(ent_name, locale, delim - locale))
            best_match = strdup(dirent->d_name);
    }

    closedir(dir);

    if (!best_match)
        return STATUS_UNSUCCESSFUL;

    if (!(model_path = malloc(path_len + 1 /* '/' */ + strlen(best_match) + 1)))
    {
        status = STATUS_NO_MEMORY;
        goto done;
    }

    sprintf(model_path, "%s/%s", path, best_match);

    TRACE("trying to load Vosk model %s.\n", debugstr_a(model_path));

    if ((*model = p_vosk_model_new(model_path)) != NULL)
        status = STATUS_SUCCESS;

done:
    free(model_path);
    free(best_match);

    return status;
}

static NTSTATUS find_model_by_locale( const char *locale, VoskModel **model )
{
    const char *suffix = NULL;
    char *env, *path = NULL;
    NTSTATUS status;

    TRACE("locale %s, model %p.\n", debugstr_a(locale), model);

    if (!model)
        return STATUS_UNSUCCESSFUL;

    if (!find_model_by_locale_and_path(getenv("VOSK_MODEL_PATH"), locale, model))
        return STATUS_SUCCESS;
    if (!find_model_by_locale_and_path("/usr/share/vosk", locale, model))
        return STATUS_SUCCESS;

    if ((env = getenv("XDG_CACHE_HOME")))
        suffix = "/vosk";
    else if ((env = getenv("HOME")))
        suffix = "/.cache/vosk";
    else
        return STATUS_UNSUCCESSFUL;

    if (!(path = malloc(strlen(env) + strlen(suffix) + 1)))
        return STATUS_NO_MEMORY;

    sprintf(path, "%s%s", env, suffix);
    status = find_model_by_locale_and_path(path, locale, model);
    free(path);

    return status;
}

static NTSTATUS speech_create_recognizer( void *args )
{
    struct speech_create_recognizer_params *params = args;
    VoskRecognizer *recognizer = NULL;
    VoskModel *model = NULL;
    NTSTATUS status = STATUS_SUCCESS;

    TRACE("args %p.\n", args);

    if (!vosk_handle)
        return STATUS_NOT_SUPPORTED;

    if ((status = find_model_by_locale(params->locale, &model)))
        return status;

    if (!(recognizer = p_vosk_recognizer_new(model, params->sample_rate)))
        status = STATUS_UNSUCCESSFUL;

    /* VoskModel is reference-counted.  A VoskRecognizer keeps a reference to its model. */
    p_vosk_model_free(model);

    params->handle = vosk_recognizer_to_handle(recognizer);
    return status;
}

static NTSTATUS speech_release_recognizer( void *args )
{
    struct speech_release_recognizer_params *params = args;

    TRACE("args %p.\n", args);

    if (!vosk_handle)
        return STATUS_NOT_SUPPORTED;

    p_vosk_recognizer_free(vosk_recognizer_from_handle(params->handle));

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
MAKE_UNSUPPORTED_FUNC(speech_create_recognizer)
MAKE_UNSUPPORTED_FUNC(speech_release_recognizer)
#undef MAKE_UNSUPPORTED_FUNC

#endif /* SONAME_LIBVOSK */

unixlib_entry_t __wine_unix_call_funcs[] =
{
    process_attach,
    process_detach,
    speech_create_recognizer,
    speech_release_recognizer,
};

unixlib_entry_t __wine_unix_call_wow64_funcs[] =
{
    process_attach,
    process_detach,
    speech_create_recognizer,
    speech_release_recognizer,
};
