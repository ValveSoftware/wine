/* Copyright 2022 Rémi Bernon for CodeWeavers
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

#include "gst_private.h"

#include "mfapi.h"
#include "mferror.h"
#include "mfobjects.h"
#include "mftransform.h"
#include "wmcodecdsp.h"

#include "initguid.h"
#include "d3d11.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(mfplat);
WINE_DECLARE_DEBUG_CHANNEL(winediag);

extern GUID MFVideoFormat_ABGR32;

static const GUID *const input_types[] =
{
    &MFVideoFormat_IYUV,
    &MFVideoFormat_YV12,
    &MFVideoFormat_NV12,
    &MFVideoFormat_420O,
    &MFVideoFormat_UYVY,
    &MFVideoFormat_YUY2,
    &MEDIASUBTYPE_P208,
    &MFVideoFormat_NV11,
    &MFVideoFormat_AYUV,
    &MFVideoFormat_ARGB32,
    &MFVideoFormat_RGB32,
    &MFVideoFormat_RGB24,
    &MFVideoFormat_I420,
    &MFVideoFormat_YVYU,
    &MFVideoFormat_RGB555,
    &MFVideoFormat_RGB565,
    &MFVideoFormat_RGB8,
    &MFVideoFormat_Y216,
    &MFVideoFormat_v410,
    &MFVideoFormat_Y41P,
    &MFVideoFormat_Y41T,
    &MFVideoFormat_Y42T,
};
static const GUID *const output_types[] =
{
    &MFVideoFormat_YUY2,
    &MFVideoFormat_IYUV,
    &MFVideoFormat_I420,
    &MFVideoFormat_NV12,
    &MFVideoFormat_RGB24,
    &MFVideoFormat_ARGB32,
    &MFVideoFormat_RGB32,
    &MFVideoFormat_YV12,
    &MFVideoFormat_AYUV,
    &MFVideoFormat_RGB555,
    &MFVideoFormat_RGB565,
    &MFVideoFormat_ABGR32,
};

struct video_processor
{
    IMFTransform IMFTransform_iface;
    LONG refcount;

    IMFAttributes *attributes;
    IMFAttributes *output_attributes;

    IMFMediaType *input_type;
    MFT_INPUT_STREAM_INFO input_info;
    IMFMediaType *output_type;
    MFT_OUTPUT_STREAM_INFO output_info;

    IMFSample *input_sample;
    wg_transform_t wg_transform;
    struct wg_sample_queue *wg_sample_queue;

    IUnknown *device_manager;
    IMFVideoSampleAllocatorEx *allocator;
};

static HRESULT normalize_stride(IMFMediaType *media_type, BOOL bottom_up, IMFMediaType **ret)
{
    MFVIDEOFORMAT *format;
    LONG stride;
    UINT32 size;
    HRESULT hr;

    if (SUCCEEDED(hr = IMFMediaType_GetUINT32(media_type, &MF_MT_DEFAULT_STRIDE, (UINT32 *)&stride)))
    {
        *ret = media_type;
        IMFMediaType_AddRef(media_type);
        return hr;
    }

    if (SUCCEEDED(hr = MFCreateMFVideoFormatFromMFMediaType(media_type, &format, &size)))
    {
        if (bottom_up) format->videoInfo.VideoFlags |= MFVideoFlag_BottomUpLinearRep;
        hr = MFCreateVideoMediaType(format, (IMFVideoMediaType **)ret);
        CoTaskMemFree(format);
    }

    return hr;
}

static HRESULT try_create_wg_transform(struct video_processor *impl)
{
    BOOL bottom_up = !impl->device_manager; /* when not D3D-enabled, the transform outputs bottom up RGB buffers */
    IMFMediaType *input_type, *output_type;
    struct wg_transform_attrs attrs = {0};
    HRESULT hr;

    if (impl->wg_transform)
    {
        wg_transform_destroy(impl->wg_transform);
        impl->wg_transform = 0;
    }

    if (FAILED(hr = normalize_stride(impl->input_type, bottom_up, &input_type)))
        return hr;
    if (FAILED(hr = normalize_stride(impl->output_type, bottom_up, &output_type)))
    {
        IMFMediaType_Release(input_type);
        return hr;
    }
    hr = wg_transform_create_mf(input_type, output_type, &attrs, &impl->wg_transform);
    IMFMediaType_Release(output_type);
    IMFMediaType_Release(input_type);

    return hr;
}

static HRESULT video_processor_init_allocator(struct video_processor *processor)
{
    IMFVideoSampleAllocatorEx *allocator;
    UINT32 count;
    HRESULT hr;

    if (processor->allocator)
        return S_OK;

    if (FAILED(hr = MFCreateVideoSampleAllocatorEx(&IID_IMFVideoSampleAllocatorEx, (void **)&allocator)))
        return hr;
    if (FAILED(IMFAttributes_GetUINT32(processor->attributes, &MF_SA_MINIMUM_OUTPUT_SAMPLE_COUNT, &count)))
        count = 2;
    if (FAILED(hr = IMFVideoSampleAllocatorEx_SetDirectXManager(allocator, processor->device_manager))
            || FAILED(hr = IMFVideoSampleAllocatorEx_InitializeSampleAllocatorEx(allocator, count, max(count + 2, 10),
            processor->output_attributes, processor->output_type)))
    {
        IMFVideoSampleAllocatorEx_Release(allocator);
        return hr;
    }

    processor->allocator = allocator;
    return S_OK;
}

static HRESULT video_processor_uninit_allocator(struct video_processor *processor)
{
    HRESULT hr;

    if (!processor->allocator)
        return S_OK;

    if (SUCCEEDED(hr = IMFVideoSampleAllocatorEx_UninitializeSampleAllocator(processor->allocator)))
        hr = IMFVideoSampleAllocatorEx_SetDirectXManager(processor->allocator, NULL);
    IMFVideoSampleAllocatorEx_Release(processor->allocator);
    processor->allocator = NULL;

    return hr;
}

static HRESULT video_processor_get_d3d11_resource(IMFSample *sample, ID3D11Resource **resource)
{
    IMFMediaBuffer *buffer;
    DWORD count;
    HRESULT hr;

    if (FAILED(IMFSample_GetBufferCount(sample, &count)) || count > 1)
        return E_FAIL;

    if (SUCCEEDED(hr = IMFSample_GetBufferByIndex(sample, 0, &buffer)))
    {
        IMFDXGIBuffer *dxgi_buffer;

        if (SUCCEEDED(hr = IMFMediaBuffer_QueryInterface(buffer, &IID_IMFDXGIBuffer, (void **)&dxgi_buffer)))
        {
            hr = IMFDXGIBuffer_GetResource(dxgi_buffer, &IID_ID3D11Resource, (void **)resource);
            IMFDXGIBuffer_Release(dxgi_buffer);
        }

        IMFMediaBuffer_Release(buffer);
    }

    return hr;
}

static HRESULT get_d3d11_video_device(ID3D11Resource *resource, ID3D11VideoDevice **video_device)
{
    ID3D11Device *device;
    HRESULT hr;

    ID3D11Resource_GetDevice(resource, &device);
    hr = ID3D11Device_QueryInterface(device, &IID_ID3D11VideoDevice, (void **)video_device);
    ID3D11Device_Release(device);
    return hr;
}

static HRESULT get_d3d11_video_context(ID3D11Resource *resource, ID3D11VideoContext **video_context)
{
    ID3D11DeviceContext *context;
    ID3D11Device *device;
    HRESULT hr;

    ID3D11Resource_GetDevice(resource, &device);
    ID3D11Device_GetImmediateContext(device, &context);
    hr = ID3D11DeviceContext_QueryInterface(context, &IID_ID3D11VideoContext, (void **)video_context);
    ID3D11DeviceContext_Release(context);
    ID3D11Device_Release(device);
    return hr;
}

static HRESULT create_video_processor_enumerator(ID3D11Resource *resource, UINT64 input_size, UINT64 output_size,
        ID3D11VideoDevice **video_device, ID3D11VideoProcessorEnumerator **enumerator)
{
    D3D11_VIDEO_PROCESSOR_CONTENT_DESC enum_desc = {0};
    HRESULT hr;

    enum_desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    enum_desc.InputFrameRate.Denominator = 1;
    enum_desc.InputFrameRate.Numerator = 1;
    enum_desc.InputWidth = input_size >> 32;
    enum_desc.InputHeight = (UINT32)input_size;
    enum_desc.OutputFrameRate.Denominator = 1;
    enum_desc.OutputFrameRate.Numerator = 1;
    enum_desc.OutputWidth = output_size >> 32;
    enum_desc.OutputHeight = (UINT32)output_size;
    enum_desc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

    if (FAILED(hr = get_d3d11_video_device(resource, video_device)))
        return hr;
    if (FAILED(hr = ID3D11VideoDevice_CreateVideoProcessorEnumerator(*video_device, &enum_desc, enumerator)))
    {
        ID3D11VideoDevice_Release(*video_device);
        *video_device = NULL;
    }

    return hr;
}

struct resource_desc
{
    GUID subtype;
    UINT64 frame_size;
};

static HRESULT init_d3d11_video_processor(const struct resource_desc *input_desc, ID3D11Resource *input,
        const struct resource_desc *output_desc, ID3D11Resource *output, ID3D11VideoProcessor **processor,
        ID3D11VideoProcessorInputView **input_view, ID3D11VideoProcessorOutputView **output_view)
{
    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC output_view_desc = {0};
    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC input_view_desc = {0};
    ID3D11VideoProcessorEnumerator *enumerator;
    UINT input_flags = 0, output_flags = 0;
    ID3D11VideoDevice *device;
    HRESULT hr;

    *processor = NULL;
    *input_view = NULL;
    *output_view = NULL;

    input_view_desc.FourCC = input_desc->subtype.Data1;
    input_view_desc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    input_view_desc.Texture2D.MipSlice = 0;
    input_view_desc.Texture2D.ArraySlice = 0;

    output_view_desc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    output_view_desc.Texture2D.MipSlice = 0;

    /* assume input and output have the same device */
    if (FAILED(hr = create_video_processor_enumerator(input, input_desc->frame_size,
            output_desc->frame_size, &device, &enumerator)))
        return hr;

    if (FAILED(hr = ID3D11VideoProcessorEnumerator_CheckVideoProcessorFormat(enumerator,
            input_desc->subtype.Data1, &input_flags))
            || FAILED(hr = ID3D11VideoProcessorEnumerator_CheckVideoProcessorFormat(enumerator,
            output_desc->subtype.Data1, &output_flags)))
        goto failed;
    if (!(input_flags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT)
            || !(output_flags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT))
    {
        hr = MF_E_INVALIDMEDIATYPE;
        goto failed;
    }

    if (FAILED(hr = ID3D11VideoDevice_CreateVideoProcessorInputView(device, input, enumerator,
            &input_view_desc, input_view)))
        goto failed;
    if (FAILED(hr = ID3D11VideoDevice_CreateVideoProcessorOutputView(device, output, enumerator,
            &output_view_desc, output_view)))
    {
        ID3D11VideoProcessorInputView_Release(*input_view);
        *input_view = NULL;
        goto failed;
    }

    if (FAILED(hr = ID3D11VideoDevice_CreateVideoProcessor(device, enumerator, 0, processor)))
    {
        ID3D11VideoProcessorOutputView_Release(*output_view);
        *output_view = NULL;
        ID3D11VideoProcessorInputView_Release(*input_view);
        *input_view = NULL;
        goto failed;
    }

failed:
    ID3D11VideoProcessorEnumerator_Release(enumerator);
    ID3D11VideoDevice_Release(device);
    return hr;
}

static HRESULT video_processor_process_output_d3d11(struct video_processor *processor,
        IMFSample *input_sample, IMFSample *output_sample)
{
    D3D11_VIDEO_PROCESSOR_STREAM streams = {0};
    struct resource_desc input_desc, output_desc;
    ID3D11VideoProcessorOutputView *output_view;
    ID3D11VideoProcessorInputView *input_view;
    ID3D11VideoProcessor *video_processor;
    ID3D11VideoContext *video_context;
    ID3D11Resource *input, *output;
    MFVideoArea aperture;
    RECT rect = {0};
    LONGLONG time;
    DWORD flags;
    HRESULT hr;

    if (FAILED(hr = IMFMediaType_GetUINT64(processor->input_type, &MF_MT_FRAME_SIZE, &input_desc.frame_size))
            || FAILED(hr = IMFMediaType_GetGUID(processor->input_type, &MF_MT_SUBTYPE, &input_desc.subtype))
            || FAILED(hr = IMFMediaType_GetUINT64(processor->output_type, &MF_MT_FRAME_SIZE, &output_desc.frame_size))
            || FAILED(hr = IMFMediaType_GetGUID(processor->output_type, &MF_MT_SUBTYPE, &output_desc.subtype)))
        return hr;

    if (FAILED(hr = video_processor_get_d3d11_resource(input_sample, &input)))
        return hr;
    if (FAILED(hr = video_processor_get_d3d11_resource(output_sample, &output)))
    {
        ID3D11Resource_Release(input);
        return hr;
    }

    if (FAILED(hr = get_d3d11_video_context(input, &video_context)))
        goto failed;
    if (FAILED(hr = init_d3d11_video_processor(&input_desc, input, &output_desc, output,
            &video_processor, &input_view, &output_view)))
    {
        ID3D11VideoContext_Release(video_context);
        goto failed;
    }

    streams.Enable = TRUE;
    streams.OutputIndex = 0;
    streams.InputFrameOrField = 0;
    streams.PastFrames = 0;
    streams.FutureFrames = 0;
    streams.pInputSurface = input_view;

    if (SUCCEEDED(IMFMediaType_GetBlob(processor->input_type, &MF_MT_MINIMUM_DISPLAY_APERTURE, (BYTE *)&aperture, sizeof(aperture), NULL)))
        SetRect(&rect, aperture.OffsetX.value, aperture.OffsetY.value, aperture.OffsetX.value + aperture.Area.cx,
                aperture.OffsetY.value + aperture.Area.cy);
    else
        SetRect(&rect, 0, 0, input_desc.frame_size >> 32, (UINT32)input_desc.frame_size);
    ID3D11VideoContext_VideoProcessorSetStreamSourceRect(video_context, video_processor, 0, TRUE, &rect);

    if (SUCCEEDED(IMFMediaType_GetBlob(processor->output_type, &MF_MT_MINIMUM_DISPLAY_APERTURE, (BYTE *)&aperture, sizeof(aperture), NULL)))
        SetRect(&rect, aperture.OffsetX.value, aperture.OffsetY.value, aperture.OffsetX.value + aperture.Area.cx,
                aperture.OffsetY.value + aperture.Area.cy);
    else
        SetRect(&rect, 0, 0, output_desc.frame_size >> 32, (UINT32)output_desc.frame_size);
    ID3D11VideoContext_VideoProcessorSetStreamDestRect(video_context, video_processor, 0, TRUE, &rect);

    ID3D11VideoContext_VideoProcessorBlt(video_context, video_processor, output_view, 0, 1, &streams);

    IMFSample_CopyAllItems(input_sample, (IMFAttributes *)output_sample);
    if (SUCCEEDED(IMFSample_GetSampleDuration(input_sample, &time)))
        IMFSample_SetSampleDuration(output_sample, time);
    if (SUCCEEDED(IMFSample_GetSampleTime(input_sample, &time)))
        IMFSample_SetSampleTime(output_sample, time);
    if (SUCCEEDED(IMFSample_GetSampleFlags(input_sample, &flags)))
        IMFSample_SetSampleFlags(output_sample, flags);

    ID3D11VideoProcessorOutputView_Release(output_view);
    ID3D11VideoProcessorInputView_Release(input_view);
    ID3D11VideoProcessor_Release(video_processor);
    ID3D11VideoContext_Release(video_context);

failed:
    ID3D11Resource_Release(output);
    ID3D11Resource_Release(input);
    return hr;
}

static struct video_processor *impl_from_IMFTransform(IMFTransform *iface)
{
    return CONTAINING_RECORD(iface, struct video_processor, IMFTransform_iface);
}

static HRESULT WINAPI video_processor_QueryInterface(IMFTransform *iface, REFIID iid, void **out)
{
    struct video_processor *impl = impl_from_IMFTransform(iface);

    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IMFTransform))
        *out = &impl->IMFTransform_iface;
    else
    {
        *out = NULL;
        WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}

static ULONG WINAPI video_processor_AddRef(IMFTransform *iface)
{
    struct video_processor *impl = impl_from_IMFTransform(iface);
    ULONG refcount = InterlockedIncrement(&impl->refcount);

    TRACE("iface %p increasing refcount to %lu.\n", iface, refcount);

    return refcount;
}

static ULONG WINAPI video_processor_Release(IMFTransform *iface)
{
    struct video_processor *impl = impl_from_IMFTransform(iface);
    ULONG refcount = InterlockedDecrement(&impl->refcount);

    TRACE("iface %p decreasing refcount to %lu.\n", iface, refcount);

    if (!refcount)
    {
        video_processor_uninit_allocator(impl);
        if (impl->device_manager)
            IUnknown_Release(impl->device_manager);
        if (impl->wg_transform)
            wg_transform_destroy(impl->wg_transform);
        if (impl->input_type)
            IMFMediaType_Release(impl->input_type);
        if (impl->output_type)
            IMFMediaType_Release(impl->output_type);
        if (impl->attributes)
            IMFAttributes_Release(impl->attributes);
        if (impl->output_attributes)
            IMFAttributes_Release(impl->output_attributes);

        wg_sample_queue_destroy(impl->wg_sample_queue);
        free(impl);
    }

    return refcount;
}

static HRESULT WINAPI video_processor_GetStreamLimits(IMFTransform *iface, DWORD *input_minimum,
        DWORD *input_maximum, DWORD *output_minimum, DWORD *output_maximum)
{
    TRACE("iface %p, input_minimum %p, input_maximum %p, output_minimum %p, output_maximum %p.\n",
            iface, input_minimum, input_maximum, output_minimum, output_maximum);
    *input_minimum = *input_maximum = *output_minimum = *output_maximum = 1;
    return S_OK;
}

static HRESULT WINAPI video_processor_GetStreamCount(IMFTransform *iface, DWORD *inputs, DWORD *outputs)
{
    TRACE("iface %p, inputs %p, outputs %p.\n", iface, inputs, outputs);
    *inputs = *outputs = 1;
    return S_OK;
}

static HRESULT WINAPI video_processor_GetStreamIDs(IMFTransform *iface, DWORD input_size, DWORD *inputs,
        DWORD output_size, DWORD *outputs)
{
    TRACE("iface %p, input_size %lu, inputs %p, output_size %lu, outputs %p.\n", iface,
            input_size, inputs, output_size, outputs);
    return E_NOTIMPL;
}

static HRESULT WINAPI video_processor_GetInputStreamInfo(IMFTransform *iface, DWORD id, MFT_INPUT_STREAM_INFO *info)
{
    struct video_processor *impl = impl_from_IMFTransform(iface);

    TRACE("iface %p, id %#lx, info %p.\n", iface, id, info);

    if (id)
        return MF_E_INVALIDSTREAMNUMBER;

    *info = impl->input_info;
    return S_OK;
}

static HRESULT WINAPI video_processor_GetOutputStreamInfo(IMFTransform *iface, DWORD id, MFT_OUTPUT_STREAM_INFO *info)
{
    struct video_processor *impl = impl_from_IMFTransform(iface);

    TRACE("iface %p, id %#lx, info %p.\n", iface, id, info);

    if (id)
        return MF_E_INVALIDSTREAMNUMBER;

    *info = impl->output_info;
    return S_OK;
}

static HRESULT WINAPI video_processor_GetAttributes(IMFTransform *iface, IMFAttributes **attributes)
{
    struct video_processor *impl = impl_from_IMFTransform(iface);

    TRACE("iface %p, attributes %p\n", iface, attributes);

    if (!attributes)
        return E_POINTER;

    IMFAttributes_AddRef((*attributes = impl->attributes));
    return S_OK;
}

static HRESULT WINAPI video_processor_GetInputStreamAttributes(IMFTransform *iface, DWORD id, IMFAttributes **attributes)
{
    TRACE("iface %p, id %#lx, attributes %p.\n", iface, id, attributes);
    return E_NOTIMPL;
}

static HRESULT WINAPI video_processor_GetOutputStreamAttributes(IMFTransform *iface, DWORD id, IMFAttributes **attributes)
{
    struct video_processor *impl = impl_from_IMFTransform(iface);

    TRACE("iface %p, id %#lx, attributes %p\n", iface, id, attributes);

    if (!attributes)
        return E_POINTER;
    if (id)
        return MF_E_INVALIDSTREAMNUMBER;

    IMFAttributes_AddRef((*attributes = impl->output_attributes));
    return S_OK;
}

static HRESULT WINAPI video_processor_DeleteInputStream(IMFTransform *iface, DWORD id)
{
    TRACE("iface %p, id %#lx.\n", iface, id);
    return E_NOTIMPL;
}

static HRESULT WINAPI video_processor_AddInputStreams(IMFTransform *iface, DWORD streams, DWORD *ids)
{
    TRACE("iface %p, streams %lu, ids %p.\n", iface, streams, ids);
    return E_NOTIMPL;
}

static HRESULT WINAPI video_processor_GetInputAvailableType(IMFTransform *iface, DWORD id, DWORD index,
        IMFMediaType **type)
{
    IMFMediaType *media_type;
    const GUID *subtype;
    HRESULT hr;

    TRACE("iface %p, id %#lx, index %#lx, type %p.\n", iface, id, index, type);

    *type = NULL;

    if (index >= ARRAY_SIZE(input_types))
        return MF_E_NO_MORE_TYPES;
    subtype = input_types[index];

    if (FAILED(hr = MFCreateMediaType(&media_type)))
        return hr;

    if (FAILED(hr = IMFMediaType_SetGUID(media_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video)))
        goto done;
    if (FAILED(hr = IMFMediaType_SetGUID(media_type, &MF_MT_SUBTYPE, subtype)))
        goto done;

    IMFMediaType_AddRef((*type = media_type));

done:
    IMFMediaType_Release(media_type);
    return hr;
}

static HRESULT WINAPI video_processor_GetOutputAvailableType(IMFTransform *iface, DWORD id, DWORD index,
        IMFMediaType **type)
{
    struct video_processor *impl = impl_from_IMFTransform(iface);
    IMFMediaType *media_type;
    UINT64 frame_size;
    GUID subtype;
    HRESULT hr;

    TRACE("iface %p, id %#lx, index %#lx, type %p.\n", iface, id, index, type);

    *type = NULL;

    if (!impl->input_type)
        return MF_E_NO_MORE_TYPES;

    if (FAILED(hr = IMFMediaType_GetGUID(impl->input_type, &MF_MT_SUBTYPE, &subtype))
            || FAILED(hr = IMFMediaType_GetUINT64(impl->input_type, &MF_MT_FRAME_SIZE, &frame_size)))
        return hr;

    if (index > ARRAY_SIZE(output_types))
        return MF_E_NO_MORE_TYPES;
    if (index > 0)
        subtype = *output_types[index - 1];

    if (FAILED(hr = MFCreateMediaType(&media_type)))
        return hr;

    if (FAILED(hr = IMFMediaType_SetGUID(media_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video)))
        goto done;
    if (FAILED(hr = IMFMediaType_SetGUID(media_type, &MF_MT_SUBTYPE, &subtype)))
        goto done;
    if (FAILED(hr = IMFMediaType_SetUINT64(media_type, &MF_MT_FRAME_SIZE, frame_size)))
        goto done;

    IMFMediaType_AddRef((*type = media_type));

done:
    IMFMediaType_Release(media_type);
    return hr;
}

static HRESULT WINAPI video_processor_SetInputType(IMFTransform *iface, DWORD id, IMFMediaType *type, DWORD flags)
{
    struct video_processor *impl = impl_from_IMFTransform(iface);
    GUID major, subtype;
    UINT64 frame_size;
    HRESULT hr;
    ULONG i;

    TRACE("iface %p, id %#lx, type %p, flags %#lx.\n", iface, id, type, flags);

    if (!type)
    {
        if (impl->input_type)
        {
            IMFMediaType_Release(impl->input_type);
            impl->input_type = NULL;
        }
        if (impl->wg_transform)
        {
            wg_transform_destroy(impl->wg_transform);
            impl->wg_transform = 0;
        }

        return S_OK;
    }

    if (FAILED(IMFMediaType_GetGUID(type, &MF_MT_MAJOR_TYPE, &major))
            || !IsEqualGUID(&major, &MFMediaType_Video))
        return E_INVALIDARG;
    if (FAILED(IMFMediaType_GetGUID(type, &MF_MT_SUBTYPE, &subtype)))
        return MF_E_INVALIDMEDIATYPE;
    if (FAILED(hr = IMFMediaType_GetUINT64(type, &MF_MT_FRAME_SIZE, &frame_size)))
        return hr;

    for (i = 0; i < ARRAY_SIZE(input_types); ++i)
        if (IsEqualGUID(&subtype, input_types[i]))
            break;
    if (i == ARRAY_SIZE(input_types))
        return MF_E_INVALIDMEDIATYPE;
    if (flags & MFT_SET_TYPE_TEST_ONLY)
        return S_OK;

    if (impl->input_type)
        IMFMediaType_Release(impl->input_type);
    IMFMediaType_AddRef((impl->input_type = type));

    if (impl->output_type && FAILED(hr = try_create_wg_transform(impl)))
    {
        IMFMediaType_Release(impl->input_type);
        impl->input_type = NULL;
    }

    if (FAILED(hr) || FAILED(MFCalculateImageSize(&subtype, frame_size >> 32, (UINT32)frame_size,
            (UINT32 *)&impl->input_info.cbSize)))
        impl->input_info.cbSize = 0;

    return hr;
}

static HRESULT WINAPI video_processor_SetOutputType(IMFTransform *iface, DWORD id, IMFMediaType *type, DWORD flags)
{
    struct video_processor *impl = impl_from_IMFTransform(iface);
    GUID major, subtype;
    UINT64 frame_size;
    HRESULT hr;
    ULONG i;

    TRACE("iface %p, id %#lx, type %p, flags %#lx.\n", iface, id, type, flags);

    if (!type)
    {
        if (impl->output_type)
        {
            IMFMediaType_Release(impl->output_type);
            impl->output_type = NULL;
        }
        if (impl->wg_transform)
        {
            wg_transform_destroy(impl->wg_transform);
            impl->wg_transform = 0;
        }

        return S_OK;
    }

    if (FAILED(IMFMediaType_GetGUID(type, &MF_MT_MAJOR_TYPE, &major))
            || !IsEqualGUID(&major, &MFMediaType_Video))
        return E_INVALIDARG;
    if (FAILED(IMFMediaType_GetGUID(type, &MF_MT_SUBTYPE, &subtype)))
        return MF_E_INVALIDMEDIATYPE;
    if (FAILED(hr = IMFMediaType_GetUINT64(type, &MF_MT_FRAME_SIZE, &frame_size)))
        return hr;

    for (i = 0; i < ARRAY_SIZE(output_types); ++i)
        if (IsEqualGUID(&subtype, output_types[i]))
            break;
    if (i == ARRAY_SIZE(output_types))
        return MF_E_INVALIDMEDIATYPE;
    if (flags & MFT_SET_TYPE_TEST_ONLY)
        return S_OK;

    if (FAILED(hr = video_processor_uninit_allocator(impl)))
        return hr;

    if (impl->output_type)
        IMFMediaType_Release(impl->output_type);
    IMFMediaType_AddRef((impl->output_type = type));

    if (impl->input_type && FAILED(hr = try_create_wg_transform(impl)))
    {
        IMFMediaType_Release(impl->output_type);
        impl->output_type = NULL;
    }

    if (FAILED(hr) || FAILED(MFCalculateImageSize(&subtype, frame_size >> 32, (UINT32)frame_size,
            (UINT32 *)&impl->output_info.cbSize)))
        impl->output_info.cbSize = 0;

    return hr;
}

static HRESULT WINAPI video_processor_GetInputCurrentType(IMFTransform *iface, DWORD id, IMFMediaType **type)
{
    struct video_processor *impl = impl_from_IMFTransform(iface);
    HRESULT hr;

    TRACE("iface %p, id %#lx, type %p.\n", iface, id, type);

    if (id != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (!impl->input_type)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    if (FAILED(hr = MFCreateMediaType(type)))
        return hr;

    if (FAILED(hr = IMFMediaType_CopyAllItems(impl->input_type, (IMFAttributes *)*type)))
        IMFMediaType_Release(*type);

    return hr;
}

static HRESULT WINAPI video_processor_GetOutputCurrentType(IMFTransform *iface, DWORD id, IMFMediaType **type)
{
    struct video_processor *impl = impl_from_IMFTransform(iface);
    HRESULT hr;

    TRACE("iface %p, id %#lx, type %p.\n", iface, id, type);

    if (id != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (!impl->output_type)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    if (FAILED(hr = MFCreateMediaType(type)))
        return hr;

    if (FAILED(hr = IMFMediaType_CopyAllItems(impl->output_type, (IMFAttributes *)*type)))
        IMFMediaType_Release(*type);

    return hr;
}

static HRESULT WINAPI video_processor_GetInputStatus(IMFTransform *iface, DWORD id, DWORD *flags)
{
    struct video_processor *impl = impl_from_IMFTransform(iface);

    FIXME("iface %p, id %#lx, flags %p stub!\n", iface, id, flags);

    if (!impl->input_type)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    *flags = MFT_INPUT_STATUS_ACCEPT_DATA;
    return S_OK;
}

static HRESULT WINAPI video_processor_GetOutputStatus(IMFTransform *iface, DWORD *flags)
{
    struct video_processor *impl = impl_from_IMFTransform(iface);

    FIXME("iface %p, flags %p stub!\n", iface, flags);

    if (!impl->output_type)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    return E_NOTIMPL;
}

static HRESULT WINAPI video_processor_SetOutputBounds(IMFTransform *iface, LONGLONG lower, LONGLONG upper)
{
    TRACE("iface %p, lower %I64d, upper %I64d.\n", iface, lower, upper);
    return E_NOTIMPL;
}

static HRESULT WINAPI video_processor_ProcessEvent(IMFTransform *iface, DWORD id, IMFMediaEvent *event)
{
    FIXME("iface %p, id %#lx, event %p stub!\n", iface, id, event);
    return E_NOTIMPL;
}

static HRESULT WINAPI video_processor_ProcessMessage(IMFTransform *iface, MFT_MESSAGE_TYPE message, ULONG_PTR param)
{
    struct video_processor *processor = impl_from_IMFTransform(iface);
    HRESULT hr;

    TRACE("iface %p, message %#x, param %Ix.\n", iface, message, param);

    switch (message)
    {
    case MFT_MESSAGE_SET_D3D_MANAGER:
        if (FAILED(hr = video_processor_uninit_allocator(processor)))
            return hr;

        if (processor->device_manager)
        {
            processor->output_info.dwFlags &= ~MFT_OUTPUT_STREAM_PROVIDES_SAMPLES;
            IUnknown_Release(processor->device_manager);
        }
        if ((processor->device_manager = (IUnknown *)param))
        {
            IUnknown_AddRef(processor->device_manager);
            processor->output_info.dwFlags |= MFT_OUTPUT_STREAM_PROVIDES_SAMPLES;
        }
        return S_OK;

    default:
        FIXME("Ignoring message %#x.\n", message);
        return S_OK;
    }
}

static HRESULT WINAPI video_processor_ProcessInput(IMFTransform *iface, DWORD id, IMFSample *sample, DWORD flags)
{
    struct video_processor *impl = impl_from_IMFTransform(iface);

    TRACE("iface %p, id %#lx, sample %p, flags %#lx.\n", iface, id, sample, flags);

    if (!impl->wg_transform)
        return MF_E_TRANSFORM_TYPE_NOT_SET;
    if (impl->input_sample)
        return MF_E_NOTACCEPTING;

    impl->input_sample = sample;
    IMFSample_AddRef(impl->input_sample);
    return S_OK;
}

static HRESULT WINAPI video_processor_ProcessOutput(IMFTransform *iface, DWORD flags, DWORD count,
        MFT_OUTPUT_DATA_BUFFER *samples, DWORD *status)
{
    struct video_processor *impl = impl_from_IMFTransform(iface);
    MFT_OUTPUT_STREAM_INFO info;
    IMFSample *input_sample, *output_sample;
    HRESULT hr;

    TRACE("iface %p, flags %#lx, count %lu, samples %p, status %p.\n", iface, flags, count, samples, status);

    if (count != 1)
        return E_INVALIDARG;

    if (!impl->wg_transform)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    samples->dwStatus = 0;
    if (FAILED(hr = IMFTransform_GetOutputStreamInfo(iface, 0, &info)))
        return hr;

    if (!(input_sample = impl->input_sample))
        return MF_E_TRANSFORM_NEED_MORE_INPUT;
    impl->input_sample = NULL;

    if (impl->output_info.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES)
    {
        if (FAILED(hr = video_processor_init_allocator(impl))
                || FAILED(hr = IMFVideoSampleAllocatorEx_AllocateSample(impl->allocator, &output_sample)))
        {
            IMFSample_Release(input_sample);
            return hr;
        }
    }
    else
    {
        if (!(output_sample = samples->pSample))
        {
            IMFSample_Release(input_sample);
            return E_INVALIDARG;
        }
        IMFSample_AddRef(output_sample);
    }

    if (FAILED(hr = video_processor_process_output_d3d11(impl, input_sample, output_sample)))
    {
        if (FAILED(hr = wg_transform_push_mf(impl->wg_transform, input_sample, impl->wg_sample_queue)))
            goto done;
        if (FAILED(hr = wg_transform_read_mf(impl->wg_transform, output_sample, info.cbSize, &samples->dwStatus)))
            goto done;
        wg_sample_queue_flush(impl->wg_sample_queue, false);
    }

    if (impl->output_info.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES)
    {
        samples->pSample = output_sample;
        IMFSample_AddRef(output_sample);
    }

done:
    IMFSample_Release(output_sample);
    IMFSample_Release(input_sample);
    return hr;
}

static const IMFTransformVtbl video_processor_vtbl =
{
    video_processor_QueryInterface,
    video_processor_AddRef,
    video_processor_Release,
    video_processor_GetStreamLimits,
    video_processor_GetStreamCount,
    video_processor_GetStreamIDs,
    video_processor_GetInputStreamInfo,
    video_processor_GetOutputStreamInfo,
    video_processor_GetAttributes,
    video_processor_GetInputStreamAttributes,
    video_processor_GetOutputStreamAttributes,
    video_processor_DeleteInputStream,
    video_processor_AddInputStreams,
    video_processor_GetInputAvailableType,
    video_processor_GetOutputAvailableType,
    video_processor_SetInputType,
    video_processor_SetOutputType,
    video_processor_GetInputCurrentType,
    video_processor_GetOutputCurrentType,
    video_processor_GetInputStatus,
    video_processor_GetOutputStatus,
    video_processor_SetOutputBounds,
    video_processor_ProcessEvent,
    video_processor_ProcessMessage,
    video_processor_ProcessInput,
    video_processor_ProcessOutput,
};

HRESULT video_processor_create(REFIID riid, void **ret)
{
    const MFVIDEOFORMAT input_format =
    {
        .dwSize = sizeof(MFVIDEOFORMAT),
        .videoInfo = {.dwWidth = 1920, .dwHeight = 1080},
        .guidFormat = MFVideoFormat_I420,
    };
    const MFVIDEOFORMAT output_format =
    {
        .dwSize = sizeof(MFVIDEOFORMAT),
        .videoInfo = {.dwWidth = 1920, .dwHeight = 1080},
        .guidFormat = MFVideoFormat_NV12,
    };
    struct video_processor *impl;
    HRESULT hr;

    TRACE("riid %s, ret %p.\n", debugstr_guid(riid), ret);

    if (FAILED(hr = check_video_transform_support(&input_format, &output_format)))
    {
        ERR_(winediag)("GStreamer doesn't support video conversion, please install appropriate plugins.\n");
        return hr;
    }

    if (!(impl = calloc(1, sizeof(*impl))))
        return E_OUTOFMEMORY;

    if (FAILED(hr = MFCreateAttributes(&impl->attributes, 0)))
        goto failed;
    if (FAILED(hr = IMFAttributes_SetUINT32(impl->attributes, &MF_SA_D3D11_AWARE, TRUE)))
        goto failed;
    /* native only has MF_SA_D3D_AWARE on Win7, but it is useful to have in mfreadwrite */
    if (FAILED(hr = IMFAttributes_SetUINT32(impl->attributes, &MF_SA_D3D_AWARE, TRUE)))
        goto failed;
    if (FAILED(hr = MFCreateAttributes(&impl->output_attributes, 0)))
        goto failed;
    if (FAILED(hr = wg_sample_queue_create(&impl->wg_sample_queue)))
        goto failed;

    impl->IMFTransform_iface.lpVtbl = &video_processor_vtbl;
    impl->refcount = 1;

    *ret = &impl->IMFTransform_iface;
    TRACE("Created %p\n", *ret);
    return S_OK;

failed:
    if (impl->output_attributes)
        IMFAttributes_Release(impl->output_attributes);
    if (impl->attributes)
        IMFAttributes_Release(impl->attributes);
    free(impl);
    return hr;
}
