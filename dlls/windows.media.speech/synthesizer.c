/* WinRT Windows.Media.Speech implementation
 *
 * Copyright 2021 Rémi Bernon for CodeWeavers
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

#include "private.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(speech);

struct voice_information
{
    IVoiceInformation IVoiceInformation_iface;
    LONG ref;

    HSTRING id;
    HSTRING display_name;
    HSTRING language;
    HSTRING description;
    VoiceGender gender;
};

static inline struct voice_information *impl_from_IVoiceInformation( IVoiceInformation *iface )
{
    return CONTAINING_RECORD(iface, struct voice_information, IVoiceInformation_iface);
}

static void voice_information_delete( struct voice_information *voice_info )
{
    WindowsDeleteString(voice_info->id);
    WindowsDeleteString(voice_info->display_name);
    WindowsDeleteString(voice_info->language);
    WindowsDeleteString(voice_info->description);
    free(voice_info);
}

static HRESULT WINAPI voice_information_QueryInterface( IVoiceInformation *iface, REFIID iid, void **ppvObject)
{
    struct voice_information *impl = impl_from_IVoiceInformation( iface );

    TRACE("iface %p, riid %s, ppv %p\n", iface, wine_dbgstr_guid(iid), ppvObject);

    if (IsEqualGUID(iid, &IID_IUnknown) ||
        IsEqualGUID(iid, &IID_IInspectable) ||
        IsEqualGUID(iid, &IID_IVoiceInformation))
    {
        IInspectable_AddRef((*ppvObject = &impl->IVoiceInformation_iface));
        return S_OK;
    }

    WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
    *ppvObject = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI voice_information_AddRef( IVoiceInformation *iface )
{
    struct voice_information *impl = impl_from_IVoiceInformation(iface);
    ULONG ref = InterlockedIncrement(&impl->ref);
    TRACE("iface %p, ref %lu.\n", iface, ref);
    return ref;
}

static ULONG WINAPI voice_information_Release( IVoiceInformation *iface )
{
    struct voice_information *impl = impl_from_IVoiceInformation(iface);
    ULONG ref = InterlockedDecrement(&impl->ref);
    TRACE("iface %p, ref %lu.\n", iface, ref);
    /* all voices are (for now) statically allocated in all_voices vector. so don't free them */
    return ref;
}

static HRESULT WINAPI voice_information_GetIids( IVoiceInformation *iface, ULONG *iid_count, IID **iids )
{
    FIXME("iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids);
    return E_NOTIMPL;
}

static HRESULT WINAPI voice_information_GetRuntimeClassName( IVoiceInformation *iface, HSTRING *class_name )
{
    FIXME("iface %p, class_name %p stub!\n", iface, class_name);
    return E_NOTIMPL;
}

static HRESULT WINAPI voice_information_GetTrustLevel( IVoiceInformation *iface, TrustLevel *trust_level )
{
    FIXME("iface %p, trust_level %p stub!\n", iface, trust_level);
    return E_NOTIMPL;
}

static HRESULT WINAPI voice_information_get_DisplayName( IVoiceInformation *iface, HSTRING *value )
{
    struct voice_information *impl = impl_from_IVoiceInformation(iface);

    TRACE("iface %p, value %p!n", iface, value);
    return WindowsDuplicateString(impl->display_name, value);
}

static HRESULT WINAPI voice_information_get_Id( IVoiceInformation *iface, HSTRING *value )
{
    struct voice_information *impl = impl_from_IVoiceInformation(iface);

    TRACE("iface %p, value %p\n", iface, value);
    return WindowsDuplicateString(impl->id, value);
}

static HRESULT WINAPI voice_information_get_Language( IVoiceInformation *iface, HSTRING *value )
{
    struct voice_information *impl = impl_from_IVoiceInformation(iface);

    TRACE("iface %p, value %p\n", iface, value);
    return WindowsDuplicateString(impl->language, value);
}

static HRESULT WINAPI voice_information_get_Description( IVoiceInformation *iface, HSTRING *value )
{
    struct voice_information *impl = impl_from_IVoiceInformation(iface);

    TRACE("iface %p, value %p\n", iface, value);
    return WindowsDuplicateString(impl->description, value);
}

static HRESULT WINAPI voice_information_get_Gender( IVoiceInformation *iface, VoiceGender *value )
{
    struct voice_information *impl = impl_from_IVoiceInformation(iface);

    TRACE("iface %p, value %p\n", iface, value);
    *value = impl->gender;
    return S_OK;
}

static const struct IVoiceInformationVtbl voice_information_vtbl =
{
    /*** IUnknown methods ***/
    voice_information_QueryInterface,
    voice_information_AddRef,
    voice_information_Release,

    /*** IInspectable methods ***/
    voice_information_GetIids,
    voice_information_GetRuntimeClassName,
    voice_information_GetTrustLevel,

    /*** IVoiceInformation methods ***/
    voice_information_get_DisplayName,
    voice_information_get_Id,
    voice_information_get_Language,
    voice_information_get_Description,
    voice_information_get_Gender,
};

HRESULT voice_information_allocate(const WCHAR *display_name, const WCHAR *id, const WCHAR *locale,
                                   VoiceGender gender, IVoiceInformation **pvoice)
{
    struct voice_information *voice_info;
    WCHAR *description;
    HRESULT hr;
    size_t len, langlen;

    voice_info = calloc(1, sizeof(*voice_info));
    if (!voice_info) return E_OUTOFMEMORY;

    len = wcslen(display_name) + 3;
    langlen = GetLocaleInfoEx(locale, LOCALE_SLOCALIZEDDISPLAYNAME, NULL, 0);
    description = malloc((len + langlen)  * sizeof(WCHAR));
    wcscpy(description, display_name);
    wcscat(description, L" - ");
    GetLocaleInfoEx(locale, LOCALE_SLOCALIZEDDISPLAYNAME, description + len, langlen);

    hr = WindowsCreateString(display_name, wcslen(display_name), &voice_info->display_name);
    if (SUCCEEDED(hr))
        hr = WindowsCreateString(id, wcslen(id), &voice_info->id);
    if (SUCCEEDED(hr))
        hr = WindowsCreateString(locale, wcslen(locale), &voice_info->language);
    if (SUCCEEDED(hr))
        hr = WindowsCreateString(description, len + langlen - 1, &voice_info->description);
    if (SUCCEEDED(hr))
    {
        voice_info->gender = gender;
        voice_info->IVoiceInformation_iface.lpVtbl = &voice_information_vtbl;

        *pvoice = &voice_info->IVoiceInformation_iface;
    }
    else
    {
        voice_information_delete(voice_info);
    }
    free(description);
    return hr;
}

/*
 *
 * IVectorView_VoiceInformation
 *
 */

struct voice_information_vector
{
    IVectorView_VoiceInformation IVectorView_VoiceInformation_iface;
    LONG ref;
};

static inline struct voice_information_vector *impl_from_IVectorView_VoiceInformation( IVectorView_VoiceInformation *iface )
{
    return CONTAINING_RECORD(iface, struct voice_information_vector, IVectorView_VoiceInformation_iface);
}

static HRESULT WINAPI vector_view_voice_information_QueryInterface( IVectorView_VoiceInformation *iface, REFIID iid, void **out )
{
    struct voice_information_vector *impl = impl_from_IVectorView_VoiceInformation(iface);

    TRACE("iface %p, iid %s, out %p stub!\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IUnknown) ||
        IsEqualGUID(iid, &IID_IInspectable) ||
        IsEqualGUID(iid, &IID_IVectorView_VoiceInformation))
    {
        IInspectable_AddRef((*out = &impl->IVectorView_VoiceInformation_iface));
        return S_OK;
    }

    WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI vector_view_voice_information_AddRef( IVectorView_VoiceInformation *iface )
{
    struct voice_information_vector *impl = impl_from_IVectorView_VoiceInformation(iface);
    ULONG ref = InterlockedIncrement(&impl->ref);
    TRACE("iface %p, ref %lu.\n", iface, ref);
    return ref;
}

static ULONG WINAPI vector_view_voice_information_Release( IVectorView_VoiceInformation *iface )
{
    struct voice_information_vector *impl = impl_from_IVectorView_VoiceInformation(iface);
    ULONG ref = InterlockedDecrement(&impl->ref);
    TRACE("iface %p, ref %lu.\n", iface, ref);
    return ref;
}

static HRESULT WINAPI vector_view_voice_information_GetIids( IVectorView_VoiceInformation *iface, ULONG *iid_count, IID **iids )
{
    FIXME("iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids);
    return E_NOTIMPL;
}

static HRESULT WINAPI vector_view_voice_information_GetRuntimeClassName( IVectorView_VoiceInformation *iface, HSTRING *class_name )
{
    FIXME("iface %p, class_name %p stub!\n", iface, class_name);
    return E_NOTIMPL;
}

static HRESULT WINAPI vector_view_voice_information_GetTrustLevel( IVectorView_VoiceInformation *iface, TrustLevel *trust_level )
{
    FIXME("iface %p, trust_level %p stub!\n", iface, trust_level);
    return E_NOTIMPL;
}

static HRESULT WINAPI vector_view_voice_information_GetAt( IVectorView_VoiceInformation *iface, UINT32 index, IVoiceInformation **value )
{
    FIXME("iface %p, index %#x, value %p stub!\n", iface, index, value);
    *value = NULL;
    return E_BOUNDS;
}

static HRESULT WINAPI vector_view_voice_information_get_Size( IVectorView_VoiceInformation *iface, UINT32 *value )
{
    FIXME("iface %p, value %p stub!\n", iface, value);
    *value = 0;
    return S_OK;
}

static HRESULT WINAPI vector_view_voice_information_IndexOf( IVectorView_VoiceInformation *iface,
                                                             IVoiceInformation *element, UINT32 *index, BOOLEAN *found )
{
    FIXME("iface %p, element %p, index %p, found %p stub!\n", iface, element, index, found);
    *index = 0;
    *found = FALSE;
    return S_OK;
}

static HRESULT WINAPI vector_view_voice_information_GetMany( IVectorView_VoiceInformation *iface, UINT32 start_index,
                                                             UINT32 items_size, IVoiceInformation **items, UINT *value )
{
    FIXME("iface %p, start_index %#x, items %p, value %p stub!\n", iface, start_index, items, value);
    *value = 0;
    return S_OK;
}

static const struct IVectorView_VoiceInformationVtbl vector_view_voice_information_vtbl =
{
    vector_view_voice_information_QueryInterface,
    vector_view_voice_information_AddRef,
    vector_view_voice_information_Release,
    /* IInspectable methods */
    vector_view_voice_information_GetIids,
    vector_view_voice_information_GetRuntimeClassName,
    vector_view_voice_information_GetTrustLevel,
    /* IVectorView<VoiceInformation> methods */
    vector_view_voice_information_GetAt,
    vector_view_voice_information_get_Size,
    vector_view_voice_information_IndexOf,
    vector_view_voice_information_GetMany,
};

static struct voice_information_vector all_voices =
{
    {&vector_view_voice_information_vtbl},
    0
};

/*
 *
 * ISpeechSynthesisStream
 *
 */

struct synthesis_stream
{
    ISpeechSynthesisStream ISpeechSynthesisStream_iface;
    LONG ref;

    IVector_IMediaMarker *markers;
};

static inline struct synthesis_stream *impl_from_ISpeechSynthesisStream( ISpeechSynthesisStream *iface )
{
    return CONTAINING_RECORD(iface, struct synthesis_stream, ISpeechSynthesisStream_iface);
}

HRESULT WINAPI synthesis_stream_QueryInterface( ISpeechSynthesisStream *iface, REFIID iid, void **out )
{
    struct synthesis_stream *impl = impl_from_ISpeechSynthesisStream(iface);

    TRACE("iface %p, iid %s, out %p stub!\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IUnknown)  ||
        IsEqualGUID(iid, &IID_IInspectable) ||
        IsEqualGUID(iid, &IID_IAgileObject) ||
        IsEqualGUID(iid, &IID_ISpeechSynthesisStream))
    {
        IInspectable_AddRef((*out = &impl->ISpeechSynthesisStream_iface));
        return S_OK;
    }

    FIXME("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
    *out = NULL;
    return E_NOINTERFACE;
}

ULONG WINAPI synthesis_stream_AddRef( ISpeechSynthesisStream *iface )
{
    struct synthesis_stream *impl = impl_from_ISpeechSynthesisStream(iface);
    ULONG ref = InterlockedIncrement(&impl->ref);
    TRACE("iface %p, ref %lu.\n", iface, ref);
    return ref;
}

ULONG WINAPI synthesis_stream_Release( ISpeechSynthesisStream *iface )
{
    struct synthesis_stream *impl = impl_from_ISpeechSynthesisStream(iface);
    ULONG ref = InterlockedDecrement(&impl->ref);

    TRACE("iface %p, ref %lu.\n", iface, ref);

    if (!ref)
        free(impl);

    return ref;
}

HRESULT WINAPI synthesis_stream_GetIids( ISpeechSynthesisStream *iface, ULONG *iid_count, IID **iids )
{
    FIXME("iface %p, iid_count %p, iids %p stub.\n", iface, iid_count, iids);
    return E_NOTIMPL;
}

HRESULT WINAPI synthesis_stream_GetRuntimeClassName( ISpeechSynthesisStream *iface, HSTRING *class_name )
{
    FIXME("iface %p, class_name %p stub.\n", iface, class_name);
    return E_NOTIMPL;
}

HRESULT WINAPI synthesis_stream_GetTrustLevel( ISpeechSynthesisStream *iface, TrustLevel *trust_level )
{
    FIXME("iface %p, trust_level %p stub.\n", iface, trust_level);
    return E_NOTIMPL;
}

HRESULT WINAPI synthesis_stream_get_Markers( ISpeechSynthesisStream *iface, IVectorView_IMediaMarker **value )
{
    struct synthesis_stream *impl = impl_from_ISpeechSynthesisStream(iface);
    FIXME("iface %p, value %p stub!\n", iface, value);
    return IVector_IMediaMarker_GetView(impl->markers, value);
}

static const struct ISpeechSynthesisStreamVtbl synthesis_stream_vtbl =
{
    /* IUnknown methods */
    synthesis_stream_QueryInterface,
    synthesis_stream_AddRef,
    synthesis_stream_Release,
    /* IInspectable methods */
    synthesis_stream_GetIids,
    synthesis_stream_GetRuntimeClassName,
    synthesis_stream_GetTrustLevel,
    /* ISpeechSynthesisStream methods */
    synthesis_stream_get_Markers
};


static HRESULT synthesis_stream_create( ISpeechSynthesisStream **out )
{
    struct synthesis_stream *impl;
    struct vector_iids markers_iids =
    {
        .iterable = &IID_IIterable_IMediaMarker,
        .iterator = &IID_IIterator_IMediaMarker,
        .vector = &IID_IVector_IMediaMarker,
        .view = &IID_IVectorView_IMediaMarker,
    };
    HRESULT hr;

    TRACE("out %p.\n", out);

    if (!(impl = calloc(1, sizeof(*impl))))
    {
        *out = NULL;
        return E_OUTOFMEMORY;
    }

    impl->ISpeechSynthesisStream_iface.lpVtbl = &synthesis_stream_vtbl;
    impl->ref = 1;
    if (FAILED(hr = vector_inspectable_create(&markers_iids, (IVector_IInspectable**)&impl->markers)))
        goto error;

    TRACE("created ISpeechSynthesisStream %p.\n", impl);
    *out = &impl->ISpeechSynthesisStream_iface;
    return S_OK;

error:
    free(impl);
    return hr;
}

/*
 *
 * SpeechSynthesizerOptions runtimeclass
 *
 */
struct synthesizer_options
{
    ISpeechSynthesizerOptions ISpeechSynthesizerOptions_iface;
    ISpeechSynthesizerOptions2 ISpeechSynthesizerOptions2_iface;
    ISpeechSynthesizerOptions3 ISpeechSynthesizerOptions3_iface;
    LONG ref;

    /* options */
    boolean include_word_boundary;
    boolean include_sentence_boundary;

    /* options 2 */
    double audio_volume;
    double speaking_rate;
    double audio_pitch;

    /* options 3 */
    enum SpeechAppendedSilence appended_silence;
    enum SpeechPunctuationSilence punctuation_silence;
};

static inline struct synthesizer_options *impl_from_ISpeechSynthesizerOptions( ISpeechSynthesizerOptions *iface )
{
    return CONTAINING_RECORD(iface, struct synthesizer_options, ISpeechSynthesizerOptions_iface);
}

static HRESULT WINAPI synthesizer_options_QueryInterface( ISpeechSynthesizerOptions *iface, REFIID iid, void **out)
{
    struct synthesizer_options *impl = impl_from_ISpeechSynthesizerOptions(iface);

    TRACE("iface %p, iid %s, out %p stub!\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IUnknown) ||
        IsEqualGUID(iid, &IID_IInspectable) ||
        IsEqualGUID(iid, &IID_IAgileObject) ||
        IsEqualGUID(iid, &IID_ISpeechSynthesizerOptions))
    {
        IInspectable_AddRef((*out = &impl->ISpeechSynthesizerOptions_iface));
        return S_OK;
    }

    if (IsEqualGUID(iid, &IID_ISpeechSynthesizerOptions2))
    {
        IInspectable_AddRef((*out = &impl->ISpeechSynthesizerOptions2_iface));
        return S_OK;
    }

    if (IsEqualGUID(iid, &IID_ISpeechSynthesizerOptions3))
    {
        IInspectable_AddRef((*out = &impl->ISpeechSynthesizerOptions3_iface));
        return S_OK;
    }

    FIXME("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI synthesizer_options_AddRef( ISpeechSynthesizerOptions *iface )
{
    struct synthesizer_options *impl = impl_from_ISpeechSynthesizerOptions(iface);
    ULONG ref = InterlockedIncrement(&impl->ref);

    TRACE("iface %p, ref %lu.\n", iface, ref);
    return ref;
}

static ULONG WINAPI synthesizer_options_Release( ISpeechSynthesizerOptions *iface )
{
    struct synthesizer_options *impl = impl_from_ISpeechSynthesizerOptions(iface);
    ULONG ref = InterlockedDecrement(&impl->ref);

    TRACE("iface %p, ref %lu.\n", iface, ref);
    if (ref == 0)
        free(impl);
    return ref;
}

static HRESULT WINAPI synthesizer_options_GetIids( ISpeechSynthesizerOptions *iface, ULONG *iid_count, IID **iids )
{
    FIXME("iface %p, iid_count %p, iids %p stub.\n", iface, iid_count, iids);
    return E_NOTIMPL;
}

static HRESULT WINAPI synthesizer_options_GetRuntimeClassName( ISpeechSynthesizerOptions *iface, HSTRING *class_name )
{
    FIXME("iface %p, class_name %p stub.\n", iface, class_name);
    return E_NOTIMPL;
}

static HRESULT WINAPI synthesizer_options_GetTrustLevel( ISpeechSynthesizerOptions *iface, TrustLevel *trust_level )
{
    FIXME("iface %p, trust_level %p stub.\n", iface, trust_level);
    return E_NOTIMPL;
}

static HRESULT WINAPI synthesizer_options_get_IncludeWordBoundaryMetadata( ISpeechSynthesizerOptions *iface, boolean *value )
{
    struct synthesizer_options *impl = impl_from_ISpeechSynthesizerOptions(iface);
    TRACE("iface %p, value %p semi-stub.\n", iface, value);

    *value = impl->include_word_boundary;
    return S_OK;
}

static HRESULT WINAPI synthesizer_options_put_IncludeWordBoundaryMetadata( ISpeechSynthesizerOptions *iface, boolean value )
{
    struct synthesizer_options *impl = impl_from_ISpeechSynthesizerOptions(iface);
    TRACE("iface %p, value %s semi-stub.\n", iface, value ? "true" : "false");

    impl->include_word_boundary = value;
    return S_OK;
}

static HRESULT WINAPI synthesizer_options_get_IncludeSentenceBoundaryMetadata( ISpeechSynthesizerOptions *iface, boolean *value )
{
    struct synthesizer_options *impl = impl_from_ISpeechSynthesizerOptions(iface);
    TRACE("iface %p, value %p stub.\n", iface, value);

    *value = impl->include_sentence_boundary;
    return S_OK;
}

static HRESULT WINAPI synthesizer_options_put_IncludeSentenceBoundaryMetadata( ISpeechSynthesizerOptions *iface, boolean value )
{
    struct synthesizer_options *impl = impl_from_ISpeechSynthesizerOptions(iface);
    TRACE("iface %p, value %s stub.\n", iface, value ? "true" : "false");

    impl->include_sentence_boundary = value;
    return S_OK;
}

static const struct ISpeechSynthesizerOptionsVtbl synthesizer_options_vtbl =
{
    /*** IUnknown methods ***/
    synthesizer_options_QueryInterface,
    synthesizer_options_AddRef,
    synthesizer_options_Release,
    /*** IInspectable methods ***/
    synthesizer_options_GetIids,
    synthesizer_options_GetRuntimeClassName,
    synthesizer_options_GetTrustLevel,
    /*** ISpeechSynthesizerOptions methods ***/
    synthesizer_options_get_IncludeWordBoundaryMetadata,
    synthesizer_options_put_IncludeWordBoundaryMetadata,
    synthesizer_options_get_IncludeSentenceBoundaryMetadata,
    synthesizer_options_put_IncludeSentenceBoundaryMetadata,
};

DEFINE_IINSPECTABLE(synthesizer_options2, ISpeechSynthesizerOptions2, struct synthesizer_options, ISpeechSynthesizerOptions_iface)

static HRESULT WINAPI synthesizer_options2_get_AudioVolume( ISpeechSynthesizerOptions2 *iface, DOUBLE *value)
{
    struct synthesizer_options *impl = impl_from_ISpeechSynthesizerOptions2(iface);

    TRACE("iface %p value %p semi-stub!\n", iface, value);
    *value = impl->audio_volume;
    return S_OK;
}

static HRESULT WINAPI synthesizer_options2_put_AudioVolume( ISpeechSynthesizerOptions2 *iface, DOUBLE value)
{
    struct synthesizer_options *impl = impl_from_ISpeechSynthesizerOptions2(iface);

    TRACE("iface %p value %g semi-stub!\n", iface, value);
    impl->audio_volume = value;
    return S_OK;
}

static HRESULT WINAPI synthesizer_options2_get_SpeakingRate( ISpeechSynthesizerOptions2 *iface, DOUBLE *value)
{
    struct synthesizer_options *impl = impl_from_ISpeechSynthesizerOptions2(iface);

    TRACE("iface %p value %p semi-stub!\n", iface, value);
    *value = impl->speaking_rate;
    return S_OK;
}

static HRESULT WINAPI synthesizer_options2_put_SpeakingRate( ISpeechSynthesizerOptions2 *iface, DOUBLE value)
{
    struct synthesizer_options *impl = impl_from_ISpeechSynthesizerOptions2(iface);

    TRACE("iface %p value %g semi-stub!\n", iface, value);
    impl->speaking_rate = value;
    return S_OK;
}

static HRESULT WINAPI synthesizer_options2_get_AudioPitch( ISpeechSynthesizerOptions2 *iface, DOUBLE *value)
{
    struct synthesizer_options *impl = impl_from_ISpeechSynthesizerOptions2(iface);

    TRACE("iface %p value %p semi-stub!\n", iface, value);
    *value = impl->audio_pitch;
    return S_OK;
}

static HRESULT WINAPI synthesizer_options2_put_AudioPitch( ISpeechSynthesizerOptions2 *iface, DOUBLE value)
{
    struct synthesizer_options *impl = impl_from_ISpeechSynthesizerOptions2(iface);

    TRACE("iface %p value %g semi-stub!\n", iface, value);
    impl->audio_pitch = value;
    return S_OK;
}

static const struct ISpeechSynthesizerOptions2Vtbl synthesizer_options2_vtbl =
{
    /*** IUnknown methods ***/
    synthesizer_options2_QueryInterface,
    synthesizer_options2_AddRef,
    synthesizer_options2_Release,
    /*** IInspectable methods ***/
    synthesizer_options2_GetIids,
    synthesizer_options2_GetRuntimeClassName,
    synthesizer_options2_GetTrustLevel,
    /*** ISpeechSynthesizerOptions methods ***/
    synthesizer_options2_get_AudioVolume,
    synthesizer_options2_put_AudioVolume,
    synthesizer_options2_get_SpeakingRate,
    synthesizer_options2_put_SpeakingRate,
    synthesizer_options2_get_AudioPitch,
    synthesizer_options2_put_AudioPitch,
};

DEFINE_IINSPECTABLE(synthesizer_options3, ISpeechSynthesizerOptions3, struct synthesizer_options, ISpeechSynthesizerOptions_iface)

static HRESULT WINAPI synthesizer_options3_get_AppendedSilence( ISpeechSynthesizerOptions3 *iface, enum SpeechAppendedSilence *value)
{
    struct synthesizer_options *impl = impl_from_ISpeechSynthesizerOptions3(iface);

    TRACE("iface %p value %p semi-stub!\n", iface, value);
    *value = impl->appended_silence;
    return S_OK;
}

static HRESULT WINAPI synthesizer_options3_put_AppendedSilence( ISpeechSynthesizerOptions3 *iface, enum SpeechAppendedSilence value)
{
    struct synthesizer_options *impl = impl_from_ISpeechSynthesizerOptions3(iface);

    TRACE("iface %p value %u semi-stub!\n", iface, value);
    impl->appended_silence = value;
    return S_OK;
}

static HRESULT WINAPI synthesizer_options3_get_PunctuationSilence( ISpeechSynthesizerOptions3 *iface, enum SpeechPunctuationSilence *value)
{
    struct synthesizer_options *impl = impl_from_ISpeechSynthesizerOptions3(iface);

    TRACE("iface %p value %p semi-stub!\n", iface, value);
    *value = impl->punctuation_silence;
    return S_OK;
}

static HRESULT WINAPI synthesizer_options3_put_PunctuationSilence( ISpeechSynthesizerOptions3 *iface, enum SpeechPunctuationSilence value)
{
    struct synthesizer_options *impl = impl_from_ISpeechSynthesizerOptions3(iface);

    TRACE("iface %p value %u semi-stub!\n", iface, value);
    impl->punctuation_silence = value;
    return S_OK;
}

static const struct ISpeechSynthesizerOptions3Vtbl synthesizer_options3_vtbl =
{
    /*** IUnknown methods ***/
    synthesizer_options3_QueryInterface,
    synthesizer_options3_AddRef,
    synthesizer_options3_Release,
    /*** IInspectable methods ***/
    synthesizer_options3_GetIids,
    synthesizer_options3_GetRuntimeClassName,
    synthesizer_options3_GetTrustLevel,
    /*** ISpeechSynthesizerOptions methods ***/
    synthesizer_options3_get_AppendedSilence,
    synthesizer_options3_put_AppendedSilence,
    synthesizer_options3_get_PunctuationSilence,
    synthesizer_options3_put_PunctuationSilence,
};

static HRESULT synthesizer_options_allocate( struct synthesizer_options **out )
{
    struct synthesizer_options *options;

    if (!(options = calloc(1, sizeof(*options)))) return E_OUTOFMEMORY;

    options->ISpeechSynthesizerOptions_iface.lpVtbl = &synthesizer_options_vtbl;
    options->ISpeechSynthesizerOptions2_iface.lpVtbl = &synthesizer_options2_vtbl;
    options->ISpeechSynthesizerOptions3_iface.lpVtbl = &synthesizer_options3_vtbl;
    /* all other values default to 0 or false */
    options->audio_pitch = 1.0;
    options->audio_volume = 1.0;
    options->speaking_rate = 1.0;
    options->ref = 1;
    *out = options;

    return S_OK;
}

/*
 *
 * SpeechSynthesizer runtimeclass
 *
 */

struct synthesizer
{
    ISpeechSynthesizer ISpeechSynthesizer_iface;
    ISpeechSynthesizer2 ISpeechSynthesizer2_iface;
    IClosable IClosable_iface;
    LONG ref;

    struct synthesizer_options *options;
};

/*
 *
 * ISpeechSynthesizer for SpeechSynthesizer runtimeclass
 *
 */

static inline struct synthesizer *impl_from_ISpeechSynthesizer( ISpeechSynthesizer *iface )
{
    return CONTAINING_RECORD(iface, struct synthesizer, ISpeechSynthesizer_iface);
}

static HRESULT WINAPI synthesizer_QueryInterface( ISpeechSynthesizer *iface, REFIID iid, void **out )
{
    struct synthesizer *impl = impl_from_ISpeechSynthesizer(iface);

    TRACE("iface %p, iid %s, out %p stub!\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IUnknown) ||
        IsEqualGUID(iid, &IID_IInspectable) ||
        IsEqualGUID(iid, &IID_ISpeechSynthesizer))
    {
        IInspectable_AddRef((*out = &impl->ISpeechSynthesizer_iface));
        return S_OK;
    }

    if (IsEqualGUID(iid, &IID_ISpeechSynthesizer2))
    {
        IInspectable_AddRef((*out = &impl->ISpeechSynthesizer2_iface));
        return S_OK;
    }

    if (IsEqualGUID(iid, &IID_IClosable))
    {
        IInspectable_AddRef((*out = &impl->IClosable_iface));
        return S_OK;
    }

    FIXME("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI synthesizer_AddRef( ISpeechSynthesizer *iface )
{
    struct synthesizer *impl = impl_from_ISpeechSynthesizer(iface);
    ULONG ref = InterlockedIncrement(&impl->ref);
    TRACE("iface %p, ref %lu.\n", iface, ref);
    return ref;
}

static ULONG WINAPI synthesizer_Release( ISpeechSynthesizer *iface )
{
    struct synthesizer *impl = impl_from_ISpeechSynthesizer(iface);
    ULONG ref = InterlockedDecrement(&impl->ref);

    TRACE("iface %p, ref %lu.\n", iface, ref);

    if (!ref)
    {
        if (impl->options)
            ISpeechSynthesizerOptions_Release(&impl->options->ISpeechSynthesizerOptions_iface);
        free(impl);
    }

    return ref;
}

static HRESULT WINAPI synthesizer_GetIids( ISpeechSynthesizer *iface, ULONG *iid_count, IID **iids )
{
    FIXME("iface %p, iid_count %p, iids %p stub.\n", iface, iid_count, iids);
    return E_NOTIMPL;
}

static HRESULT WINAPI synthesizer_GetRuntimeClassName( ISpeechSynthesizer *iface, HSTRING *class_name )
{
    FIXME("iface %p, class_name %p stub.\n", iface, class_name);
    return E_NOTIMPL;
}

static HRESULT WINAPI synthesizer_GetTrustLevel( ISpeechSynthesizer *iface, TrustLevel *trust_level )
{
    FIXME("iface %p, trust_level %p stub.\n", iface, trust_level);
    return E_NOTIMPL;
}

static HRESULT synthesizer_synthesize_text_to_stream_async( IInspectable *invoker, IInspectable **result )
{
    return synthesis_stream_create((ISpeechSynthesisStream **)result);
}

static HRESULT WINAPI synthesizer_SynthesizeTextToStreamAsync( ISpeechSynthesizer *iface, HSTRING text,
                                                               IAsyncOperation_SpeechSynthesisStream **operation )
{
    TRACE("iface %p, text %p, operation %p.\n", iface, text, operation);
    return async_operation_inspectable_create(&IID_IAsyncOperation_SpeechSynthesisStream, NULL,
                                              synthesizer_synthesize_text_to_stream_async, (IAsyncOperation_IInspectable **)operation);
}

static HRESULT synthesizer_synthesize_ssml_to_stream_async( IInspectable *invoker, IInspectable **result )
{
    return synthesis_stream_create((ISpeechSynthesisStream **)result);
}

static HRESULT WINAPI synthesizer_SynthesizeSsmlToStreamAsync( ISpeechSynthesizer *iface, HSTRING ssml,
                                                               IAsyncOperation_SpeechSynthesisStream **operation )
{
    TRACE("iface %p, ssml %p, operation %p.\n", iface, ssml, operation);
    return async_operation_inspectable_create(&IID_IAsyncOperation_SpeechSynthesisStream, NULL,
                                              synthesizer_synthesize_ssml_to_stream_async, (IAsyncOperation_IInspectable **)operation);
}

static HRESULT WINAPI synthesizer_put_Voice( ISpeechSynthesizer *iface, IVoiceInformation *value )
{
    FIXME("iface %p, value %p stub.\n", iface, value);
    return E_NOTIMPL;
}

static HRESULT WINAPI synthesizer_get_Voice( ISpeechSynthesizer *iface, IVoiceInformation **value )
{
    FIXME("iface %p, value %p stub.\n", iface, value);
    return E_NOTIMPL;
}

static const struct ISpeechSynthesizerVtbl synthesizer_vtbl =
{
    /* IUnknown methods */
    synthesizer_QueryInterface,
    synthesizer_AddRef,
    synthesizer_Release,
    /* IInspectable methods */
    synthesizer_GetIids,
    synthesizer_GetRuntimeClassName,
    synthesizer_GetTrustLevel,
    /* ISpeechSynthesizer methods */
    synthesizer_SynthesizeTextToStreamAsync,
    synthesizer_SynthesizeSsmlToStreamAsync,
    synthesizer_put_Voice,
    synthesizer_get_Voice,
};

/*
 *
 * ISpeechSynthesizer2 for SpeechSynthesizer runtimeclass
 *
 */

DEFINE_IINSPECTABLE(synthesizer2, ISpeechSynthesizer2, struct synthesizer, ISpeechSynthesizer_iface)

static HRESULT WINAPI synthesizer2_get_Options( ISpeechSynthesizer2 *iface, ISpeechSynthesizerOptions **value )
{
    struct synthesizer *impl = impl_from_ISpeechSynthesizer2(iface);

    WARN("iface %p, value %p semi-stub.\n", iface, value);
    if (!impl->options)
    {
        struct synthesizer_options *options;
        HRESULT hr = synthesizer_options_allocate(&options);
        if (FAILED(hr)) return hr;

        if (InterlockedCompareExchangePointer((void **)&impl->options, options, NULL) != NULL)
            /* another thread beat us */
            ISpeechSynthesizerOptions_AddRef(&options->ISpeechSynthesizerOptions_iface);
    }
    ISpeechSynthesizerOptions_AddRef(*value = &impl->options->ISpeechSynthesizerOptions_iface);
    return S_OK;
}

static const struct ISpeechSynthesizer2Vtbl synthesizer2_vtbl =
{
    /* IUnknown methods */
    synthesizer2_QueryInterface,
    synthesizer2_AddRef,
    synthesizer2_Release,
    /* IInspectable methods */
    synthesizer2_GetIids,
    synthesizer2_GetRuntimeClassName,
    synthesizer2_GetTrustLevel,
    /* ISpeechSynthesizer2 methods */
    synthesizer2_get_Options,
};

/*
 *
 * IClosable for SpeechSynthesizer runtimeclass
 *
 */

DEFINE_IINSPECTABLE(closable, IClosable, struct synthesizer, ISpeechSynthesizer_iface)

static HRESULT WINAPI closable_Close( IClosable *iface )
{
    FIXME("iface %p stub.\n", iface);
    return E_NOTIMPL;
}

static const struct IClosableVtbl closable_vtbl =
{
    /* IUnknown methods */
    closable_QueryInterface,
    closable_AddRef,
    closable_Release,
    /* IInspectable methods */
    closable_GetIids,
    closable_GetRuntimeClassName,
    closable_GetTrustLevel,
    /* IClosable methods */
    closable_Close,
};

/*
 *
 * Static interfaces for SpeechSynthesizer runtimeclass
 *
 */

struct synthesizer_statics
{
    IActivationFactory IActivationFactory_iface;
    IInstalledVoicesStatic IInstalledVoicesStatic_iface;
    LONG ref;
};

/*
 *
 * IActivationFactory for SpeechSynthesizer runtimeclass
 *
 */

static inline struct synthesizer_statics *impl_from_IActivationFactory( IActivationFactory *iface )
{
    return CONTAINING_RECORD(iface, struct synthesizer_statics, IActivationFactory_iface);
}

static HRESULT WINAPI factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct synthesizer_statics *impl = impl_from_IActivationFactory(iface);

    TRACE("iface %p, iid %s, out %p stub!\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IUnknown) ||
        IsEqualGUID(iid, &IID_IInspectable) ||
        IsEqualGUID(iid, &IID_IAgileObject) ||
        IsEqualGUID(iid, &IID_IActivationFactory))
    {
        IInspectable_AddRef((*out = &impl->IActivationFactory_iface));
        return S_OK;
    }

    if (IsEqualGUID(iid, &IID_IInstalledVoicesStatic))
    {
        IInspectable_AddRef((*out = &impl->IInstalledVoicesStatic_iface));
        return S_OK;
    }

    FIXME("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI factory_AddRef( IActivationFactory *iface )
{
    struct synthesizer_statics *impl = impl_from_IActivationFactory(iface);
    ULONG ref = InterlockedIncrement(&impl->ref);
    TRACE("iface %p, ref %lu.\n", iface, ref);
    return ref;
}

static ULONG WINAPI factory_Release( IActivationFactory *iface )
{
    struct synthesizer_statics *impl = impl_from_IActivationFactory(iface);
    ULONG ref = InterlockedDecrement(&impl->ref);
    TRACE("iface %p, ref %lu.\n", iface, ref);
    return ref;
}

static HRESULT WINAPI factory_GetIids( IActivationFactory *iface, ULONG *iid_count, IID **iids )
{
    FIXME("iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids);
    return E_NOTIMPL;
}

static HRESULT WINAPI factory_GetRuntimeClassName( IActivationFactory *iface, HSTRING *class_name )
{
    FIXME("iface %p, class_name %p stub!\n", iface, class_name);
    return E_NOTIMPL;
}

static HRESULT WINAPI factory_GetTrustLevel( IActivationFactory *iface, TrustLevel *trust_level )
{
    FIXME("iface %p, trust_level %p stub!\n", iface, trust_level);
    return E_NOTIMPL;
}

static HRESULT WINAPI factory_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    struct synthesizer *impl;

    TRACE("iface %p, instance %p.\n", iface, instance);

    if (!(impl = calloc(1, sizeof(*impl))))
    {
        *instance = NULL;
        return E_OUTOFMEMORY;
    }

    impl->ISpeechSynthesizer_iface.lpVtbl = &synthesizer_vtbl;
    impl->ISpeechSynthesizer2_iface.lpVtbl = &synthesizer2_vtbl;
    impl->IClosable_iface.lpVtbl = &closable_vtbl;
    impl->ref = 1;

    *instance = (IInspectable *)&impl->ISpeechSynthesizer_iface;
    return S_OK;
}

static const struct IActivationFactoryVtbl factory_vtbl =
{
    factory_QueryInterface,
    factory_AddRef,
    factory_Release,
    /* IInspectable methods */
    factory_GetIids,
    factory_GetRuntimeClassName,
    factory_GetTrustLevel,
    /* IActivationFactory methods */
    factory_ActivateInstance,
};

/*
 *
 * IInstalledVoicesStatic for SpeechSynthesizer runtimeclass
 *
 */

DEFINE_IINSPECTABLE(installed_voices_static, IInstalledVoicesStatic, struct synthesizer_statics, IActivationFactory_iface)

static HRESULT WINAPI installed_voices_static_get_AllVoices( IInstalledVoicesStatic *iface, IVectorView_VoiceInformation **value )
{
    TRACE("iface %p, value %p.\n", iface, value);
    *value = &all_voices.IVectorView_VoiceInformation_iface;
    IVectorView_VoiceInformation_AddRef(*value);
    return S_OK;
}

static HRESULT WINAPI installed_voices_static_get_DefaultVoice( IInstalledVoicesStatic *iface, IVoiceInformation **value )
{
    FIXME("iface %p, value %p stub!\n", iface, value);
    return E_NOTIMPL;
}

static const struct IInstalledVoicesStaticVtbl installed_voices_static_vtbl =
{
    installed_voices_static_QueryInterface,
    installed_voices_static_AddRef,
    installed_voices_static_Release,
    /* IInspectable methods */
    installed_voices_static_GetIids,
    installed_voices_static_GetRuntimeClassName,
    installed_voices_static_GetTrustLevel,
    /* IInstalledVoicesStatic methods */
    installed_voices_static_get_AllVoices,
    installed_voices_static_get_DefaultVoice,
};

static struct synthesizer_statics synthesizer_statics =
{
    .IActivationFactory_iface = {&factory_vtbl},
    .IInstalledVoicesStatic_iface = {&installed_voices_static_vtbl},
    .ref = 1
};

IActivationFactory *synthesizer_factory = &synthesizer_statics.IActivationFactory_iface;
