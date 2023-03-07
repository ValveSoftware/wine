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
MAKE_FUNCPTR(vosk_recognizer_new_grm);
MAKE_FUNCPTR(vosk_recognizer_free);
MAKE_FUNCPTR(vosk_recognizer_accept_waveform);
MAKE_FUNCPTR(vosk_recognizer_final_result);
MAKE_FUNCPTR(vosk_recognizer_reset);
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
    LOAD_FUNCPTR(vosk_recognizer_new)
    LOAD_FUNCPTR(vosk_recognizer_new_grm)
    LOAD_FUNCPTR(vosk_model_free)
    LOAD_FUNCPTR(vosk_recognizer_new)
    LOAD_FUNCPTR(vosk_recognizer_free)
    LOAD_FUNCPTR(vosk_recognizer_accept_waveform)
    LOAD_FUNCPTR(vosk_recognizer_final_result)
    LOAD_FUNCPTR(vosk_recognizer_reset)
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

static const char* map_lang_to_phasmophobia_dir(const char* lang, size_t len)
{
    if (!strncmp(lang, "ar", len))
        return "Arabic";
    if (!strncmp(lang, "ca", len))
        return "Catalan";
    if (!strncmp(lang, "zn", len))
        return "Chinese";
    if (!strncmp(lang, "cs", len))
        return "Czech";
    if (!strncmp(lang, "nl", len))
        return "Dutch";
    if (!strncmp(lang, "en", len))
        return "English";
    if (!strncmp(lang, "fr", len))
        return "French";
    if (!strncmp(lang, "de", len))
        return "German";
    if (!strncmp(lang, "de", len))
        return "German";
    if (!strncmp(lang, "el", len))
        return "Greek";
    if (!strncmp(lang, "it", len))
        return "Italian";
    if (!strncmp(lang, "ja", len))
        return "Japanese";
    if (!strncmp(lang, "pt", len))
        return "Portuguese";
    if (!strncmp(lang, "ru", len))
        return "Russian";
    if (!strncmp(lang, "es", len))
        return "Spanish";
    if (!strncmp(lang, "sw", len))
        return "Swedish";
    if (!strncmp(lang, "tr", len))
        return "Turkish";
    if (!strncmp(lang, "uk", len))
        return "Ukrainian";

    return "";
}

static NTSTATUS find_model_by_locale_and_path( const char *path, const char *locale, VoskModel **model )
{
    static const char *vosk_model_identifier_small = "vosk-model-small-";
    static const char *vosk_model_identifier = "vosk-model-";
    size_t ident_small_len = strlen(vosk_model_identifier_small);
    size_t ident_len = strlen(vosk_model_identifier);
    char *ent_name, *model_path, *best_match, *delim, *appid = getenv("SteamAppId"), *str = NULL;
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
        else if (strcmp(appid, "739630") != 0)
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

        if (!best_match && !strcmp(appid, "739630"))
        {
            if ((str = (char *)map_lang_to_phasmophobia_dir(locale, delim - locale)))
                best_match = strdup(str);
        }
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
    char *env, *path = NULL, *appid = getenv("SteamAppId");
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

    /* Hack to load Vosk models from Phasmophobia, so they don't need to be downloaded separately.*/
    if (status && appid && !strcmp(appid, "739630") && (env = getenv("PWD")))
    {
        suffix = "/Phasmophobia_Data/StreamingAssets/LanguageModels";

        if (!(path = malloc(strlen(env) + strlen(suffix) + 1)))
            return STATUS_NO_MEMORY;

        sprintf(path, "%s%s", env, suffix);
        status = find_model_by_locale_and_path(path, locale, model);
        free(path);
    }

    return status;
}

static NTSTATUS grammar_to_json_array(const char **grammar, UINT32 grammar_size, const char **array)
{
    size_t buf_size = strlen("[]") + 1, len;
    char *buf;
    UINT32 i;

    for (i = 0; i < grammar_size; ++i)
    {
        buf_size += strlen(grammar[i]) + 4; /* (4) - two double quotes, a comma and a space */
    }

    if (!(buf = malloc(buf_size)))
        return STATUS_NO_MEMORY;

    *array = buf;

    *buf = '[';
    buf++;

    for (i = 0; i < grammar_size; ++i)
    {
        *buf = '\"';
        buf++;
        len = strlen(grammar[i]);
        memcpy(buf, grammar[i], len);
        buf += len;
        *buf = '\"';
        buf++;
        if (i < (grammar_size - 1))
        {
            *buf = ',';
            buf++;
            *buf = ' ';
            buf++;
        }
    }

    *buf = ']';
    buf++;
    *buf = '\0';

    return STATUS_SUCCESS;
}

static NTSTATUS speech_create_recognizer( void *args )
{
    struct speech_create_recognizer_params *params = args;
    VoskRecognizer *recognizer = NULL;
    VoskModel *model = NULL;
    NTSTATUS status = STATUS_SUCCESS;
    const char *grammar_json;

    TRACE("args %p.\n", args);

    if (!vosk_handle)
        return STATUS_NOT_SUPPORTED;

    if ((status = find_model_by_locale(params->locale, &model)))
        return status;

    if (params->grammar && grammar_to_json_array(params->grammar, params->grammar_size, &grammar_json) == STATUS_SUCCESS)
    {
        if (!(recognizer = p_vosk_recognizer_new_grm(model, params->sample_rate, grammar_json)))
                status = STATUS_UNSUCCESSFUL;
    }
    else
    {
        if (!(recognizer = p_vosk_recognizer_new(model, params->sample_rate)))
                status = STATUS_UNSUCCESSFUL;
    }

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

static NTSTATUS speech_recognize_audio( void *args )
{
    struct speech_recognize_audio_params *params = args;
    VoskRecognizer *recognizer = vosk_recognizer_from_handle(params->handle);

    if (!vosk_handle)
        return STATUS_NOT_SUPPORTED;

    if (!recognizer)
        return STATUS_UNSUCCESSFUL;

    params->status = p_vosk_recognizer_accept_waveform(recognizer, (const char *)params->samples, params->samples_size);

    return STATUS_SUCCESS;
}

static NTSTATUS speech_get_recognition_result( void* args )
{
    struct speech_get_recognition_result_params *params = args;
    VoskRecognizer *recognizer = vosk_recognizer_from_handle(params->handle);
    static const char *result_json_start = "{\n  \"text\" : \"";
    const size_t json_start_len = strlen(result_json_start);
    static size_t last_result_len = 0;
    static char *last_result = NULL;
    const char *tmp = NULL;

    if (!vosk_handle)
        return STATUS_NOT_SUPPORTED;

    if (!recognizer)
        return STATUS_UNSUCCESSFUL;

    if (!last_result)
    {
        if ((tmp = p_vosk_recognizer_final_result(recognizer)))
        {
            last_result = strdup(tmp);
            tmp = last_result;

            /* Operations to remove the JSON wrapper "{\n  \"text\" : \"some recognized text\"\n}" -> "some recognized text\0" */
            memmove(last_result, last_result + json_start_len, strlen(last_result) - json_start_len + 1);
            last_result = strrchr(last_result, '\"');
            last_result[0] = '\0';

            last_result = (char *)tmp;
            last_result_len = strlen(last_result);
        }
        else return STATUS_NOT_FOUND;
    }
    else if (params->result_buf_size >= last_result_len + 1)
    {
        memcpy(params->result_buf, last_result, last_result_len + 1);
        p_vosk_recognizer_reset(recognizer);

        free (last_result);
        last_result = NULL;

        return STATUS_SUCCESS;
    }

    params->result_buf_size = last_result_len + 1;
    return STATUS_BUFFER_TOO_SMALL;
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
MAKE_UNSUPPORTED_FUNC(speech_recognize_audio)
MAKE_UNSUPPORTED_FUNC(speech_get_recognition_result)
#undef MAKE_UNSUPPORTED_FUNC

#endif /* SONAME_LIBVOSK */

unixlib_entry_t __wine_unix_call_funcs[] =
{
    process_attach,
    process_detach,
    speech_create_recognizer,
    speech_release_recognizer,
    speech_recognize_audio,
    speech_get_recognition_result,
};

unixlib_entry_t __wine_unix_call_wow64_funcs[] =
{
    process_attach,
    process_detach,
    speech_create_recognizer,
    speech_release_recognizer,
    speech_recognize_audio,
    speech_get_recognition_result,
};
