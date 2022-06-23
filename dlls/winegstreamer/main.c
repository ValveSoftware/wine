/* GStreamer Base Functions
 *
 * Copyright 2002 Lionel Ulmer
 * Copyright 2010 Aric Stewart, CodeWeavers
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

#define WINE_NO_NAMELESS_EXTENSION

#define EXTERN_GUID DEFINE_GUID
#include "initguid.h"
#include "gst_private.h"
#include "winternl.h"
#include "rpcproxy.h"
#include "gst_guids.h"

static unixlib_handle_t unix_handle;

WINE_DEFAULT_DEBUG_CHANNEL(quartz);

DEFINE_GUID(GUID_NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

bool array_reserve(void **elements, size_t *capacity, size_t count, size_t size)
{
    unsigned int new_capacity, max_capacity;
    void *new_elements;

    if (count <= *capacity)
        return TRUE;

    max_capacity = ~(SIZE_T)0 / size;
    if (count > max_capacity)
        return FALSE;

    new_capacity = max(4, *capacity);
    while (new_capacity < count && new_capacity <= max_capacity / 2)
        new_capacity *= 2;
    if (new_capacity < count)
        new_capacity = max_capacity;

    if (!(new_elements = realloc(*elements, new_capacity * size)))
        return FALSE;

    *elements = new_elements;
    *capacity = new_capacity;

    return TRUE;
}

struct wg_parser *wg_parser_create(enum wg_parser_type type, bool unlimited_buffering)
{
    struct wg_parser_create_params params =
    {
        .type = type,
        .unlimited_buffering = unlimited_buffering,
    };

    if (__wine_unix_call(unix_handle, unix_wg_parser_create, &params))
        return NULL;
    return params.parser;
}

void wg_parser_destroy(struct wg_parser *parser)
{
    __wine_unix_call(unix_handle, unix_wg_parser_destroy, parser);
}

HRESULT wg_parser_connect(struct wg_parser *parser, uint64_t file_size, const WCHAR *uri)
{
    struct wg_parser_connect_params params =
    {
        .parser = parser,
        .file_size = file_size,
        .uri = uri,
    };

    return __wine_unix_call(unix_handle, unix_wg_parser_connect, &params);
}

HRESULT wg_parser_connect_unseekable(struct wg_parser *parser, const struct wg_format *in_format,
        uint32_t stream_count, const struct wg_format *out_formats, const struct wg_rect *apertures)
{
    struct wg_parser_connect_unseekable_params params =
    {
        .parser = parser,
        .in_format = in_format,
        .stream_count = stream_count,
        .out_formats = out_formats,
        .apertures = apertures,
    };

    return __wine_unix_call(unix_handle, unix_wg_parser_connect_unseekable, &params);
}

void wg_parser_disconnect(struct wg_parser *parser)
{
    __wine_unix_call(unix_handle, unix_wg_parser_disconnect, parser);
}

bool wg_parser_get_next_read_offset(struct wg_parser *parser, uint64_t *offset, uint32_t *size)
{
    struct wg_parser_get_next_read_offset_params params =
    {
        .parser = parser,
    };

    if (__wine_unix_call(unix_handle, unix_wg_parser_get_next_read_offset, &params))
        return false;
    *offset = params.offset;
    *size = params.size;
    return true;
}

void wg_parser_push_data(struct wg_parser *parser, enum wg_read_result result, const void *data, uint32_t size)
{
    struct wg_parser_push_data_params params =
    {
        .parser = parser,
        .result = result,
        .data = data,
        .size = size,
    };

    __wine_unix_call(unix_handle, unix_wg_parser_push_data, &params);
}

uint32_t wg_parser_get_stream_count(struct wg_parser *parser)
{
    struct wg_parser_get_stream_count_params params =
    {
        .parser = parser,
    };

    __wine_unix_call(unix_handle, unix_wg_parser_get_stream_count, &params);
    return params.count;
}

struct wg_parser_stream *wg_parser_get_stream(struct wg_parser *parser, uint32_t index)
{
    struct wg_parser_get_stream_params params =
    {
        .parser = parser,
        .index = index,
    };

    __wine_unix_call(unix_handle, unix_wg_parser_get_stream, &params);
    return params.stream;
}

void wg_parser_stream_get_preferred_format(struct wg_parser_stream *stream, struct wg_format *format)
{
    struct wg_parser_stream_get_preferred_format_params params =
    {
        .stream = stream,
        .format = format,
    };

    __wine_unix_call(unix_handle, unix_wg_parser_stream_get_preferred_format, &params);
}

void wg_parser_stream_enable(struct wg_parser_stream *stream, const struct wg_format *format, const struct wg_rect *aperture, uint32_t flags)
{
    struct wg_parser_stream_enable_params params =
    {
        .stream = stream,
        .format = format,
        .aperture = aperture,
        .flags = flags,
    };

    __wine_unix_call(unix_handle, unix_wg_parser_stream_enable, &params);
}

void wg_parser_stream_disable(struct wg_parser_stream *stream)
{
    __wine_unix_call(unix_handle, unix_wg_parser_stream_disable, stream);
}

bool wg_parser_stream_get_buffer(struct wg_parser_stream *stream, struct wg_parser_buffer *buffer)
{
    struct wg_parser_stream_get_buffer_params params =
    {
        .stream = stream,
        .buffer = buffer,
    };

    return !__wine_unix_call(unix_handle, unix_wg_parser_stream_get_buffer, &params);
}

bool wg_parser_stream_copy_buffer(struct wg_parser_stream *stream,
        void *data, uint32_t offset, uint32_t size)
{
    struct wg_parser_stream_copy_buffer_params params =
    {
        .stream = stream,
        .data = data,
        .offset = offset,
        .size = size,
    };

    return !__wine_unix_call(unix_handle, unix_wg_parser_stream_copy_buffer, &params);
}

void wg_parser_stream_release_buffer(struct wg_parser_stream *stream)
{
    __wine_unix_call(unix_handle, unix_wg_parser_stream_release_buffer, stream);
}

void wg_parser_stream_notify_qos(struct wg_parser_stream *stream,
        bool underflow, double proportion, int64_t diff, uint64_t timestamp)
{
    struct wg_parser_stream_notify_qos_params params =
    {
        .stream = stream,
        .underflow = underflow,
        .proportion = proportion,
        .diff = diff,
        .timestamp = timestamp,
    };

    __wine_unix_call(unix_handle, unix_wg_parser_stream_notify_qos, &params);
}

uint64_t wg_parser_stream_get_duration(struct wg_parser_stream *stream)
{
    struct wg_parser_stream_get_duration_params params =
    {
        .stream = stream,
    };

    __wine_unix_call(unix_handle, unix_wg_parser_stream_get_duration, &params);
    return params.duration;
}

bool wg_parser_stream_get_language(struct wg_parser_stream *stream, char *buffer, uint32_t size)
{
    struct wg_parser_stream_get_language_params params =
    {
        .stream = stream,
        .buffer = buffer,
        .size = size,
    };

    return !__wine_unix_call(unix_handle, unix_wg_parser_stream_get_language, &params);
}

void wg_parser_stream_seek(struct wg_parser_stream *stream, double rate,
        uint64_t start_pos, uint64_t stop_pos, DWORD start_flags, DWORD stop_flags)
{
    struct wg_parser_stream_seek_params params =
    {
        .stream = stream,
        .rate = rate,
        .start_pos = start_pos,
        .stop_pos = stop_pos,
        .start_flags = start_flags,
        .stop_flags = stop_flags,
    };

    __wine_unix_call(unix_handle, unix_wg_parser_stream_seek, &params);
}

bool wg_parser_stream_drain(struct wg_parser_stream *stream)
{
    return !__wine_unix_call(unix_handle, unix_wg_parser_stream_drain, stream);
}

struct wg_transform *wg_transform_create(const struct wg_encoded_format *input_format,
        const struct wg_format *output_format)
{
    struct wg_transform_create_params params =
    {
        .input_format = input_format,
        .output_format = output_format,
    };

    if (__wine_unix_call(unix_handle, unix_wg_transform_create, &params))
        return NULL;
    return params.transform;
}

void wg_transform_destroy(struct wg_transform *transform)
{
    __wine_unix_call(unix_handle, unix_wg_transform_destroy, transform);
}

HRESULT wg_transform_push_data(struct wg_transform *transform, const void *data, uint32_t size)
{
    struct wg_transform_push_data_params params =
    {
        .transform = transform,
        .data = data,
        .size = size,
    };

    return __wine_unix_call(unix_handle, unix_wg_transform_push_data, &params);
}

HRESULT wg_transform_read_data(struct wg_transform *transform, struct wg_sample *sample)
{
    struct wg_transform_read_data_params params =
    {
        .transform = transform,
        .sample = sample,
    };

    return __wine_unix_call(unix_handle, unix_wg_transform_read_data, &params);
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void *reserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(instance);
        NtQueryVirtualMemory(GetCurrentProcess(), instance, MemoryWineUnixFuncs,
                &unix_handle, sizeof(unix_handle), NULL);
    }
    return TRUE;
}

struct class_factory
{
    IClassFactory IClassFactory_iface;
    HRESULT (*create_instance)(IUnknown *outer, IUnknown **out);
};

static inline struct class_factory *impl_from_IClassFactory(IClassFactory *iface)
{
    return CONTAINING_RECORD(iface, struct class_factory, IClassFactory_iface);
}

static HRESULT WINAPI class_factory_QueryInterface(IClassFactory *iface, REFIID iid, void **out)
{
    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IClassFactory))
    {
        *out = iface;
        IClassFactory_AddRef(iface);
        return S_OK;
    }

    *out = NULL;
    WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
    return E_NOINTERFACE;
}

static ULONG WINAPI class_factory_AddRef(IClassFactory *iface)
{
    return 2;
}

static ULONG WINAPI class_factory_Release(IClassFactory *iface)
{
    return 1;
}

static HRESULT WINAPI class_factory_CreateInstance(IClassFactory *iface, IUnknown *outer, REFIID iid, void **out)
{
    struct class_factory *factory = impl_from_IClassFactory(iface);
    IUnknown *unk;
    HRESULT hr;

    TRACE("iface %p, outer %p, iid %s, out %p.\n", iface, outer, debugstr_guid(iid), out);

    if (outer && !IsEqualGUID(iid, &IID_IUnknown))
        return E_NOINTERFACE;

    *out = NULL;
    if (SUCCEEDED(hr = factory->create_instance(outer, &unk)))
    {
        hr = IUnknown_QueryInterface(unk, iid, out);
        IUnknown_Release(unk);
    }
    return hr;
}

static HRESULT WINAPI class_factory_LockServer(IClassFactory *iface, BOOL lock)
{
    TRACE("iface %p, lock %d.\n", iface, lock);
    return S_OK;
}

static const IClassFactoryVtbl class_factory_vtbl =
{
    class_factory_QueryInterface,
    class_factory_AddRef,
    class_factory_Release,
    class_factory_CreateInstance,
    class_factory_LockServer,
};

static struct class_factory avi_splitter_cf = {{&class_factory_vtbl}, avi_splitter_create};
static struct class_factory decodebin_parser_cf = {{&class_factory_vtbl}, decodebin_parser_create};
static struct class_factory mpeg_splitter_cf = {{&class_factory_vtbl}, mpeg_splitter_create};
static struct class_factory wave_parser_cf = {{&class_factory_vtbl}, wave_parser_create};
static struct class_factory wma_decoder_cf = {{&class_factory_vtbl}, wma_decoder_create};

HRESULT WINAPI DllGetClassObject(REFCLSID clsid, REFIID iid, void **out)
{
    struct class_factory *factory;
    HRESULT hr;

    TRACE("clsid %s, iid %s, out %p.\n", debugstr_guid(clsid), debugstr_guid(iid), out);

    if (!init_gstreamer())
        return CLASS_E_CLASSNOTAVAILABLE;

    if (SUCCEEDED(hr = mfplat_get_class_object(clsid, iid, out)))
        return hr;

    if (IsEqualGUID(clsid, &CLSID_AviSplitter))
        factory = &avi_splitter_cf;
    else if (IsEqualGUID(clsid, &CLSID_decodebin_parser))
        factory = &decodebin_parser_cf;
    else if (IsEqualGUID(clsid, &CLSID_MPEG1Splitter))
        factory = &mpeg_splitter_cf;
    else if (IsEqualGUID(clsid, &CLSID_WAVEParser))
        factory = &wave_parser_cf;
    else if (IsEqualGUID(clsid, &CLSID_WMADecMediaObject))
        factory = &wma_decoder_cf;
    else
    {
        FIXME("%s not implemented, returning CLASS_E_CLASSNOTAVAILABLE.\n", debugstr_guid(clsid));
        return CLASS_E_CLASSNOTAVAILABLE;
    }

    return IClassFactory_QueryInterface(&factory->IClassFactory_iface, iid, out);
}

static BOOL CALLBACK init_gstreamer_proc(INIT_ONCE *once, void *param, void **ctx)
{
    HINSTANCE handle;

    /* Unloading glib is a bad idea.. it installs atexit handlers,
     * so never unload the dll after loading */
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
            (LPCWSTR)init_gstreamer_proc, &handle);
    if (!handle)
        ERR("Failed to pin module.\n");

    return TRUE;
}

BOOL init_gstreamer(void)
{
    static INIT_ONCE once = INIT_ONCE_STATIC_INIT;

    InitOnceExecuteOnce(&once, init_gstreamer_proc, NULL, NULL);

    return TRUE;
}

static const REGPINTYPES reg_audio_mt = {&MEDIATYPE_Audio, &GUID_NULL};
static const REGPINTYPES reg_stream_mt = {&MEDIATYPE_Stream, &GUID_NULL};
static const REGPINTYPES reg_video_mt = {&MEDIATYPE_Video, &GUID_NULL};

static const REGPINTYPES reg_avi_splitter_sink_mt = {&MEDIATYPE_Stream, &MEDIASUBTYPE_Avi};

static const REGFILTERPINS2 reg_avi_splitter_pins[2] =
{
    {
        .nMediaTypes = 1,
        .lpMediaType = &reg_avi_splitter_sink_mt,
    },
    {
        .dwFlags = REG_PINFLAG_B_OUTPUT,
        .nMediaTypes = 1,
        .lpMediaType = &reg_video_mt,
    },
};

static const REGFILTER2 reg_avi_splitter =
{
    .dwVersion = 2,
    .dwMerit = MERIT_NORMAL,
    .u.s2.cPins2 = 2,
    .u.s2.rgPins2 = reg_avi_splitter_pins,
};

static const REGPINTYPES reg_mpeg_splitter_sink_mts[4] =
{
    {&MEDIATYPE_Stream, &MEDIASUBTYPE_MPEG1Audio},
    {&MEDIATYPE_Stream, &MEDIASUBTYPE_MPEG1Video},
    {&MEDIATYPE_Stream, &MEDIASUBTYPE_MPEG1System},
    {&MEDIATYPE_Stream, &MEDIASUBTYPE_MPEG1VideoCD},
};

static const REGPINTYPES reg_mpeg_splitter_audio_mts[2] =
{
    {&MEDIATYPE_Audio, &MEDIASUBTYPE_MPEG1Packet},
    {&MEDIATYPE_Audio, &MEDIASUBTYPE_MPEG1AudioPayload},
};

static const REGPINTYPES reg_mpeg_splitter_video_mts[2] =
{
    {&MEDIATYPE_Video, &MEDIASUBTYPE_MPEG1Packet},
    {&MEDIATYPE_Video, &MEDIASUBTYPE_MPEG1Payload},
};

static const REGFILTERPINS2 reg_mpeg_splitter_pins[3] =
{
    {
        .nMediaTypes = 4,
        .lpMediaType = reg_mpeg_splitter_sink_mts,
    },
    {
        .dwFlags = REG_PINFLAG_B_ZERO | REG_PINFLAG_B_OUTPUT,
        .nMediaTypes = 2,
        .lpMediaType = reg_mpeg_splitter_audio_mts,
    },
    {
        .dwFlags = REG_PINFLAG_B_ZERO | REG_PINFLAG_B_OUTPUT,
        .nMediaTypes = 2,
        .lpMediaType = reg_mpeg_splitter_video_mts,
    },
};

static const REGFILTER2 reg_mpeg_splitter =
{
    .dwVersion = 2,
    .dwMerit = MERIT_NORMAL,
    .u.s2.cPins2 = 3,
    .u.s2.rgPins2 = reg_mpeg_splitter_pins,
};

static const REGPINTYPES reg_wave_parser_sink_mts[3] =
{
    {&MEDIATYPE_Stream, &MEDIASUBTYPE_WAVE},
    {&MEDIATYPE_Stream, &MEDIASUBTYPE_AU},
    {&MEDIATYPE_Stream, &MEDIASUBTYPE_AIFF},
};

static const REGFILTERPINS2 reg_wave_parser_pins[2] =
{
    {
        .nMediaTypes = 3,
        .lpMediaType = reg_wave_parser_sink_mts,
    },
    {
        .dwFlags = REG_PINFLAG_B_OUTPUT,
        .nMediaTypes = 1,
        .lpMediaType = &reg_audio_mt,
    },
};

static const REGFILTER2 reg_wave_parser =
{
    .dwVersion = 2,
    .dwMerit = MERIT_UNLIKELY,
    .u.s2.cPins2 = 2,
    .u.s2.rgPins2 = reg_wave_parser_pins,
};

static const REGFILTERPINS2 reg_decodebin_parser_pins[3] =
{
    {
        .nMediaTypes = 1,
        .lpMediaType = &reg_stream_mt,
    },
    {
        .dwFlags = REG_PINFLAG_B_OUTPUT,
        .nMediaTypes = 1,
        .lpMediaType = &reg_audio_mt,
    },
    {
        .dwFlags = REG_PINFLAG_B_OUTPUT,
        .nMediaTypes = 1,
        .lpMediaType = &reg_video_mt,
    },
};

static const REGFILTER2 reg_decodebin_parser =
{
    .dwVersion = 2,
    .dwMerit = MERIT_PREFERRED,
    .u.s2.cPins2 = 3,
    .u.s2.rgPins2 = reg_decodebin_parser_pins,
};

HRESULT WINAPI DllRegisterServer(void)
{
    IFilterMapper2 *mapper;
    HRESULT hr;

    TRACE(".\n");

    init_gstreamer();

    if (FAILED(hr = mfplat_DllRegisterServer()))
        return hr;

    if (FAILED(hr = __wine_register_resources()))
        return hr;

    if (FAILED(hr = CoCreateInstance(&CLSID_FilterMapper2, NULL, CLSCTX_INPROC_SERVER,
            &IID_IFilterMapper2, (void **)&mapper)))
        return hr;

    IFilterMapper2_RegisterFilter(mapper, &CLSID_AviSplitter, L"AVI Splitter", NULL, NULL, NULL, &reg_avi_splitter);
    IFilterMapper2_RegisterFilter(mapper, &CLSID_decodebin_parser,
            L"GStreamer splitter filter", NULL, NULL, NULL, &reg_decodebin_parser);
    IFilterMapper2_RegisterFilter(mapper, &CLSID_MPEG1Splitter,
            L"MPEG-I Stream Splitter", NULL, NULL, NULL, &reg_mpeg_splitter);
    IFilterMapper2_RegisterFilter(mapper, &CLSID_WAVEParser, L"Wave Parser", NULL, NULL, NULL, &reg_wave_parser);

    IFilterMapper2_Release(mapper);

    return mfplat_DllRegisterServer();
}

HRESULT WINAPI DllUnregisterServer(void)
{
    IFilterMapper2 *mapper;
    HRESULT hr;

    TRACE(".\n");

    if (FAILED(hr = __wine_unregister_resources()))
        return hr;

    if (FAILED(hr = CoCreateInstance(&CLSID_FilterMapper2, NULL, CLSCTX_INPROC_SERVER,
            &IID_IFilterMapper2, (void **)&mapper)))
        return hr;

    IFilterMapper2_UnregisterFilter(mapper, NULL, NULL, &CLSID_AviSplitter);
    IFilterMapper2_UnregisterFilter(mapper, NULL, NULL, &CLSID_decodebin_parser);
    IFilterMapper2_UnregisterFilter(mapper, NULL, NULL, &CLSID_MPEG1Splitter);
    IFilterMapper2_UnregisterFilter(mapper, NULL, NULL, &CLSID_WAVEParser);

    IFilterMapper2_Release(mapper);
    return S_OK;
}
