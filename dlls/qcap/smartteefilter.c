/*
 * Implementation of the SmartTee filter
 *
 * Copyright 2015 Damjan Jovanovic
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

#include "qcap_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(quartz);

typedef struct {
    struct strmbase_filter filter;
    struct strmbase_sink sink;
    struct strmbase_source capture, preview;
} SmartTeeFilter;

static inline SmartTeeFilter *impl_from_strmbase_filter(struct strmbase_filter *filter)
{
    return CONTAINING_RECORD(filter, SmartTeeFilter, filter);
}

static inline SmartTeeFilter *impl_from_strmbase_pin(struct strmbase_pin *pin)
{
    return impl_from_strmbase_filter(pin->filter);
}

static struct strmbase_pin *smart_tee_get_pin(struct strmbase_filter *iface, unsigned int index)
{
    SmartTeeFilter *filter = impl_from_strmbase_filter(iface);

    if (index == 0)
        return &filter->sink.pin;
    else if (index == 1)
        return &filter->capture.pin;
    else if (index == 2)
        return &filter->preview.pin;
    return NULL;
}

static void smart_tee_destroy(struct strmbase_filter *iface)
{
    SmartTeeFilter *filter = impl_from_strmbase_filter(iface);

    strmbase_sink_cleanup(&filter->sink);
    strmbase_source_cleanup(&filter->capture);
    strmbase_source_cleanup(&filter->preview);
    strmbase_filter_cleanup(&filter->filter);
    free(filter);
}

static HRESULT smart_tee_wait_state(struct strmbase_filter *iface, DWORD timeout)
{
    return iface->state == State_Paused ? VFW_S_CANT_CUE : S_OK;
}

static const struct strmbase_filter_ops filter_ops =
{
    .filter_get_pin = smart_tee_get_pin,
    .filter_destroy = smart_tee_destroy,
    .filter_wait_state = smart_tee_wait_state,
};

static HRESULT sink_query_accept(struct strmbase_pin *base, const AM_MEDIA_TYPE *pmt)
{
    SmartTeeFilter *This = impl_from_strmbase_pin(base);
    TRACE("(%p, AM_MEDIA_TYPE(%p))\n", This, pmt);
    if (!pmt)
        return VFW_E_TYPE_NOT_ACCEPTED;
    /* We'll take any media type, but the output pins will later
     * struggle to connect downstream. */
    return S_OK;
}

static HRESULT sink_get_media_type(struct strmbase_pin *base,
        unsigned int iPosition, AM_MEDIA_TYPE *amt)
{
    SmartTeeFilter *This = impl_from_strmbase_pin(base);
    HRESULT hr;
    TRACE("(%p)->(%d, %p)\n", This, iPosition, amt);
    if (iPosition)
        return S_FALSE;
    EnterCriticalSection(&This->filter.filter_cs);
    if (This->sink.pin.peer)
    {
        CopyMediaType(amt, &This->sink.pin.mt);
        hr = S_OK;
    }
    else
        hr = S_FALSE;
    LeaveCriticalSection(&This->filter.filter_cs);
    return hr;
}

static HRESULT sink_query_interface(struct strmbase_pin *iface, REFIID iid, void **out)
{
    SmartTeeFilter *filter = impl_from_strmbase_pin(iface);

    if (IsEqualGUID(iid, &IID_IMemInputPin))
        *out = &filter->sink.IMemInputPin_iface;
    else
        return E_NOINTERFACE;

    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}

static HRESULT copy_sample(IMediaSample *inputSample, IMemAllocator *allocator, IMediaSample **pOutputSample)
{
    REFERENCE_TIME startTime, endTime;
    BOOL haveStartTime = TRUE, haveEndTime = TRUE;
    IMediaSample *outputSample = NULL;
    BYTE *ptrIn, *ptrOut;
    AM_MEDIA_TYPE *mediaType = NULL;
    HRESULT hr;

    hr = IMediaSample_GetTime(inputSample, &startTime, &endTime);
    if (hr == S_OK)
        ;
    else if (hr == VFW_S_NO_STOP_TIME)
        haveEndTime = FALSE;
    else if (hr == VFW_E_SAMPLE_TIME_NOT_SET)
        haveStartTime = haveEndTime = FALSE;
    else
        goto end;

        hr = IMemAllocator_GetBuffer(allocator, &outputSample,
            haveStartTime ? &startTime : NULL, haveEndTime ? &endTime : NULL, 0);
        if (hr == VFW_E_NOT_COMMITTED)
        {
        HRESULT commit_hr;

        commit_hr = IMemAllocator_Commit(allocator);
        if (SUCCEEDED(commit_hr))
            hr = IMemAllocator_GetBuffer(allocator, &outputSample,
                haveStartTime ? &startTime : NULL, haveEndTime ? &endTime : NULL, 0);
        }
    if (FAILED(hr)) goto end;
    if (IMediaSample_GetSize(outputSample) < IMediaSample_GetActualDataLength(inputSample)) {
        ERR("insufficient space in sample\n");
        hr = VFW_E_BUFFER_OVERFLOW;
        goto end;
    }

    hr = IMediaSample_SetTime(outputSample, haveStartTime ? &startTime : NULL, haveEndTime ? &endTime : NULL);
    if (FAILED(hr)) goto end;

    hr = IMediaSample_GetPointer(inputSample, &ptrIn);
    if (FAILED(hr)) goto end;
    hr = IMediaSample_GetPointer(outputSample, &ptrOut);
    if (FAILED(hr)) goto end;
    memcpy(ptrOut, ptrIn, IMediaSample_GetActualDataLength(inputSample));
    IMediaSample_SetActualDataLength(outputSample, IMediaSample_GetActualDataLength(inputSample));

    hr = IMediaSample_SetDiscontinuity(outputSample, IMediaSample_IsDiscontinuity(inputSample) == S_OK);
    if (FAILED(hr)) goto end;

    haveStartTime = haveEndTime = TRUE;
    hr = IMediaSample_GetMediaTime(inputSample, &startTime, &endTime);
    if (hr == S_OK)
        ;
    else if (hr == VFW_S_NO_STOP_TIME)
        haveEndTime = FALSE;
    else if (hr == VFW_E_MEDIA_TIME_NOT_SET)
        haveStartTime = haveEndTime = FALSE;
    else
        goto end;
    hr = IMediaSample_SetMediaTime(outputSample, haveStartTime ? &startTime : NULL, haveEndTime ? &endTime : NULL);
    if (FAILED(hr)) goto end;

    hr = IMediaSample_GetMediaType(inputSample, &mediaType);
    if (FAILED(hr)) goto end;
    if (hr == S_OK) {
        hr = IMediaSample_SetMediaType(outputSample, mediaType);
        if (FAILED(hr)) goto end;
    }

    hr = IMediaSample_SetPreroll(outputSample, IMediaSample_IsPreroll(inputSample) == S_OK);
    if (FAILED(hr)) goto end;

    hr = IMediaSample_SetSyncPoint(outputSample, IMediaSample_IsSyncPoint(inputSample) == S_OK);
    if (FAILED(hr)) goto end;

end:
    if (mediaType)
        DeleteMediaType(mediaType);
    if (FAILED(hr) && outputSample) {
        IMediaSample_Release(outputSample);
        outputSample = NULL;
    }
    *pOutputSample = outputSample;
    return hr;
}

static BOOL mt_is_rgb24_video(const AM_MEDIA_TYPE *mt)
{
    return IsEqualGUID(&mt->majortype, &MEDIATYPE_Video)
            && IsEqualGUID(&mt->subtype, &MEDIASUBTYPE_RGB24)
            && IsEqualGUID(&mt->formattype, &FORMAT_VideoInfo)
            && mt->cbFormat >= sizeof(VIDEOINFOHEADER)
            && mt->pbFormat;
}

static BOOL mt_is_rgb32_video(const AM_MEDIA_TYPE *mt)
{
    return IsEqualGUID(&mt->majortype, &MEDIATYPE_Video)
            && IsEqualGUID(&mt->subtype, &MEDIASUBTYPE_RGB32)
            && IsEqualGUID(&mt->formattype, &FORMAT_VideoInfo)
            && mt->cbFormat >= sizeof(VIDEOINFOHEADER)
            && mt->pbFormat;
}

static HRESULT convert_sample_rgb24_to_rgb32(IMediaSample *sample)
{
    BYTE *data;
    LONG input_len, output_len, sample_size;
    LONG pixels, i;
    HRESULT hr;

    input_len = IMediaSample_GetActualDataLength(sample);
    if (input_len <= 0 || input_len % 3)
        return VFW_E_TYPE_NOT_ACCEPTED;

    pixels = input_len / 3;
    output_len = pixels * 4;
    sample_size = IMediaSample_GetSize(sample);
    if (output_len > sample_size)
        return VFW_E_BUFFER_OVERFLOW;

    if (FAILED(hr = IMediaSample_GetPointer(sample, &data)))
        return hr;

    for (i = pixels - 1; i >= 0; --i)
    {
        data[i * 4 + 0] = data[i * 3 + 0];
        data[i * 4 + 1] = data[i * 3 + 1];
        data[i * 4 + 2] = data[i * 3 + 2];
        data[i * 4 + 3] = 0xff;
    }

    return IMediaSample_SetActualDataLength(sample, output_len);
}

static HRESULT WINAPI SmartTeeFilterInput_Receive(struct strmbase_sink *base, IMediaSample *inputSample)
{
    SmartTeeFilter *This = impl_from_strmbase_pin(&base->pin);
    IMediaSample *captureSample = NULL;
    IMediaSample *previewSample = NULL;
    HRESULT hrCapture = VFW_E_NOT_CONNECTED, hrPreview = VFW_E_NOT_CONNECTED;

    TRACE("(%p)->(%p)\n", This, inputSample);

    /* Modifying the image coming out of one pin doesn't modify the image
     * coming out of the other. MSDN claims the filter doesn't copy,
     * but unless it somehow uses copy-on-write, I just don't see how
     * that's possible. */

    /* FIXME: we should ideally do each of these in a separate thread */
    EnterCriticalSection(&This->filter.filter_cs);
    if (This->capture.pin.peer)
        hrCapture = copy_sample(inputSample, This->capture.pAllocator, &captureSample);
    LeaveCriticalSection(&This->filter.filter_cs);
    if (SUCCEEDED(hrCapture) && This->capture.pMemInputPin)
        hrCapture = IMemInputPin_Receive(This->capture.pMemInputPin, captureSample);
    if (captureSample)
        IMediaSample_Release(captureSample);

    EnterCriticalSection(&This->filter.filter_cs);
    if (This->preview.pin.peer)
        hrPreview = copy_sample(inputSample, This->preview.pAllocator, &previewSample);
    LeaveCriticalSection(&This->filter.filter_cs);

    if (SUCCEEDED(hrPreview) && mt_is_rgb24_video(&This->sink.pin.mt) && mt_is_rgb32_video(&This->preview.pin.mt))
        hrPreview = convert_sample_rgb24_to_rgb32(previewSample);

    /* No timestamps on preview stream: */
    if (SUCCEEDED(hrPreview))
        hrPreview = IMediaSample_SetTime(previewSample, NULL, NULL);
    if (SUCCEEDED(hrPreview) && This->preview.pMemInputPin)
        hrPreview = IMemInputPin_Receive(This->preview.pMemInputPin, previewSample);
    if (previewSample)
        IMediaSample_Release(previewSample);

    /* FIXME: how to merge the HRESULTs from the 2 pins? */
    if (SUCCEEDED(hrCapture))
        return hrCapture;
    else
        return hrPreview;
}

static const struct strmbase_sink_ops sink_ops =
{
    .base.pin_query_accept = sink_query_accept,
    .base.pin_get_media_type = sink_get_media_type,
    .base.pin_query_interface = sink_query_interface,
    .pfnReceive = SmartTeeFilterInput_Receive,
};

static HRESULT capture_query_accept(struct strmbase_pin *base, const AM_MEDIA_TYPE *amt)
{
    FIXME("(%p) stub\n", base);
    return S_OK;
}

static HRESULT source_get_media_type(struct strmbase_pin *iface,
        unsigned int index, AM_MEDIA_TYPE *mt)
{
    SmartTeeFilter *filter = impl_from_strmbase_pin(iface);
    HRESULT hr = S_OK;
    BOOL is_preview = iface == &filter->preview.pin;
    BOOL rgb24_video;

    EnterCriticalSection(&filter->filter.filter_cs);

    if (!filter->sink.pin.peer)
        hr = VFW_E_NOT_CONNECTED;
    else if (is_preview)
    {
        rgb24_video = mt_is_rgb24_video(&filter->sink.pin.mt);

        if (!index)
        {
            CopyMediaType(mt, &filter->sink.pin.mt);
            if (rgb24_video)
            {
                VIDEOINFOHEADER *vih = (VIDEOINFOHEADER *)mt->pbFormat;
                LONG width = vih->bmiHeader.biWidth;
                LONG height = vih->bmiHeader.biHeight < 0 ? -vih->bmiHeader.biHeight : vih->bmiHeader.biHeight;

                mt->subtype = MEDIASUBTYPE_RGB32;
                mt->lSampleSize = width * height * 4;
                vih->bmiHeader.biBitCount = 32;
                vih->bmiHeader.biCompression = BI_RGB;
                vih->bmiHeader.biSizeImage = mt->lSampleSize;
            }
        }
        else if (index == 1 && rgb24_video)
            CopyMediaType(mt, &filter->sink.pin.mt);
        else
            hr = VFW_S_NO_MORE_ITEMS;
    }
    else if (!index)
        CopyMediaType(mt, &filter->sink.pin.mt);
    else
        hr = VFW_S_NO_MORE_ITEMS;

    LeaveCriticalSection(&filter->filter.filter_cs);
    return hr;
}

static HRESULT WINAPI SmartTeeFilterCapture_DecideAllocator(struct strmbase_source *base,
        IMemInputPin *pPin, IMemAllocator **pAlloc)
{
    SmartTeeFilter *This = impl_from_strmbase_pin(&base->pin);
    TRACE("(%p, %p, %p)\n", This, pPin, pAlloc);
    *pAlloc = This->sink.pAllocator;
    IMemAllocator_AddRef(This->sink.pAllocator);
    return IMemInputPin_NotifyAllocator(pPin, This->sink.pAllocator, TRUE);
}

static const struct strmbase_source_ops capture_ops =
{
    .base.pin_query_accept = capture_query_accept,
    .base.pin_get_media_type = source_get_media_type,
    .pfnAttemptConnection = BaseOutputPinImpl_AttemptConnection,
    .pfnDecideAllocator = SmartTeeFilterCapture_DecideAllocator,
};

static HRESULT preview_query_accept(struct strmbase_pin *base, const AM_MEDIA_TYPE *amt)
{
    FIXME("(%p) stub\n", base);
    return S_OK;
}

static HRESULT WINAPI SmartTeeFilterPreview_DecideAllocator(struct strmbase_source *base,
        IMemInputPin *pPin, IMemAllocator **pAlloc)
{
    SmartTeeFilter *This = impl_from_strmbase_pin(&base->pin);
    TRACE("(%p, %p, %p)\n", This, pPin, pAlloc);
    return BaseOutputPinImpl_DecideAllocator(base, pPin, pAlloc);
}

static HRESULT WINAPI SmartTeeFilterPreview_DecideBufferSize(struct strmbase_source *base,
        IMemAllocator *allocator, ALLOCATOR_PROPERTIES *props)
{
    VIDEOINFOHEADER *format = (VIDEOINFOHEADER *)base->pin.mt.pbFormat;
    ALLOCATOR_PROPERTIES ret_props;

    if (!props->cBuffers)
        props->cBuffers = 3;

    if (!props->cbBuffer)
    {
        if (base->pin.mt.lSampleSize)
            props->cbBuffer = base->pin.mt.lSampleSize;
        else if (format)
            props->cbBuffer = format->bmiHeader.biSizeImage;
        else
            return VFW_E_INVALIDMEDIATYPE;
    }

    if (!props->cbAlign)
        props->cbAlign = 1;

    return IMemAllocator_SetProperties(allocator, props, &ret_props);
}

static const struct strmbase_source_ops preview_ops =
{
    .base.pin_query_accept = preview_query_accept,
    .base.pin_get_media_type = source_get_media_type,
    .pfnAttemptConnection = BaseOutputPinImpl_AttemptConnection,
    .pfnDecideAllocator = SmartTeeFilterPreview_DecideAllocator,
    .pfnDecideBufferSize = SmartTeeFilterPreview_DecideBufferSize,
};

HRESULT smart_tee_create(IUnknown *outer, IUnknown **out)
{
    SmartTeeFilter *object;
    HRESULT hr;

    if (!(object = calloc(1, sizeof(*object))))
        return E_OUTOFMEMORY;

    strmbase_filter_init(&object->filter, outer, &CLSID_SmartTee, &filter_ops);
    strmbase_sink_init(&object->sink, &object->filter, L"Input", &sink_ops, NULL);
    hr = CoCreateInstance(&CLSID_MemoryAllocator, NULL, CLSCTX_INPROC_SERVER,
            &IID_IMemAllocator, (void **)&object->sink.pAllocator);
    if (FAILED(hr))
    {
        strmbase_filter_cleanup(&object->filter);
        free(object);
        return hr;
    }

    strmbase_source_init(&object->capture, &object->filter, L"Capture", &capture_ops);
    strmbase_source_init(&object->preview, &object->filter, L"Preview", &preview_ops);

    TRACE("Created smart tee %p.\n", object);
    *out = &object->filter.IUnknown_inner;
    return S_OK;
}
