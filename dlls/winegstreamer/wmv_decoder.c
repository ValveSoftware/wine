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
#include "mediaerr.h"
#include "dmort.h"

#include "initguid.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(mfplat);

DEFINE_MEDIATYPE_GUID(MEDIASUBTYPE_VC1S,MAKEFOURCC('V','C','1','S'));

static enum wg_video_format const video_formats[] =
{
    WG_VIDEO_FORMAT_BGRx,
};

struct wmv_decoder
{
    IUnknown IUnknown_inner;
    IMFTransform IMFTransform_iface;
    IMediaObject IMediaObject_iface;
    IPropertyBag IPropertyBag_iface;
    IPropertyStore IPropertyStore_iface;
    IUnknown *outer;
    LONG refcount;

    struct wg_format input_format;
    struct wg_format output_format;
    DMO_OUTPUT_DATA_BUFFER output;
};

static inline struct wmv_decoder *impl_from_IUnknown(IUnknown *iface)
{
    return CONTAINING_RECORD(iface, struct wmv_decoder, IUnknown_inner);
}

static HRESULT WINAPI unknown_QueryInterface(IUnknown *iface, REFIID iid, void **out)
{
    struct wmv_decoder *impl = impl_from_IUnknown(iface);

    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IUnknown))
        *out = &impl->IUnknown_inner;
    else if (IsEqualGUID(iid, &IID_IMFTransform))
        *out = &impl->IMFTransform_iface;
    else if (IsEqualGUID(iid, &IID_IMediaObject))
        *out = &impl->IMediaObject_iface;
    else if (IsEqualIID(iid, &IID_IPropertyBag))
        *out = &impl->IPropertyBag_iface;
    else if (IsEqualIID(iid, &IID_IPropertyStore))
        *out = &impl->IPropertyStore_iface;
    else
    {
        *out = NULL;
        WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}

static ULONG WINAPI unknown_AddRef(IUnknown *iface)
{
    struct wmv_decoder *impl = impl_from_IUnknown(iface);
    ULONG refcount = InterlockedIncrement(&impl->refcount);

    TRACE("iface %p increasing refcount to %lu.\n", iface, refcount);

    return refcount;
}

static ULONG WINAPI unknown_Release(IUnknown *iface)
{
    struct wmv_decoder *impl = impl_from_IUnknown(iface);
    ULONG refcount = InterlockedDecrement(&impl->refcount);

    TRACE("iface %p decreasing refcount to %lu.\n", iface, refcount);

    if (!refcount)
        free(impl);

    return refcount;
}

static const IUnknownVtbl unknown_vtbl =
{
    unknown_QueryInterface,
    unknown_AddRef,
    unknown_Release,
};

static struct wmv_decoder *impl_from_IMFTransform(IMFTransform *iface)
{
    return CONTAINING_RECORD(iface, struct wmv_decoder, IMFTransform_iface);
}

static HRESULT WINAPI transform_QueryInterface(IMFTransform *iface, REFIID iid, void **out)
{
    return IUnknown_QueryInterface(impl_from_IMFTransform(iface)->outer, iid, out);
}

static ULONG WINAPI transform_AddRef(IMFTransform *iface)
{
    return IUnknown_AddRef(impl_from_IMFTransform(iface)->outer);
}

static ULONG WINAPI transform_Release(IMFTransform *iface)
{
    return IUnknown_Release(impl_from_IMFTransform(iface)->outer);
}

static HRESULT WINAPI transform_GetStreamLimits(IMFTransform *iface, DWORD *input_minimum,
        DWORD *input_maximum, DWORD *output_minimum, DWORD *output_maximum)
{
    FIXME("iface %p, input_minimum %p, input_maximum %p, output_minimum %p, output_maximum %p stub!\n",
            iface, input_minimum, input_maximum, output_minimum, output_maximum);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_GetStreamCount(IMFTransform *iface, DWORD *inputs, DWORD *outputs)
{
    FIXME("iface %p, inputs %p, outputs %p stub!\n", iface, inputs, outputs);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_GetStreamIDs(IMFTransform *iface, DWORD input_size, DWORD *inputs,
        DWORD output_size, DWORD *outputs)
{
    FIXME("iface %p, input_size %lu, inputs %p, output_size %lu, outputs %p stub!\n", iface,
            input_size, inputs, output_size, outputs);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_GetInputStreamInfo(IMFTransform *iface, DWORD id, MFT_INPUT_STREAM_INFO *info)
{
    FIXME("iface %p, id %#lx, info %p stub!\n", iface, id, info);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_GetOutputStreamInfo(IMFTransform *iface, DWORD id, MFT_OUTPUT_STREAM_INFO *info)
{
    FIXME("iface %p, id %#lx, info %p stub!\n", iface, id, info);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_GetAttributes(IMFTransform *iface, IMFAttributes **attributes)
{
    FIXME("iface %p, attributes %p stub!\n", iface, attributes);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_GetInputStreamAttributes(IMFTransform *iface, DWORD id, IMFAttributes **attributes)
{
    FIXME("iface %p, id %#lx, attributes %p stub!\n", iface, id, attributes);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_GetOutputStreamAttributes(IMFTransform *iface, DWORD id, IMFAttributes **attributes)
{
    FIXME("iface %p, id %#lx, attributes %p stub!\n", iface, id, attributes);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_DeleteInputStream(IMFTransform *iface, DWORD id)
{
    FIXME("iface %p, id %#lx stub!\n", iface, id);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_AddInputStreams(IMFTransform *iface, DWORD streams, DWORD *ids)
{
    FIXME("iface %p, streams %lu, ids %p stub!\n", iface, streams, ids);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_GetInputAvailableType(IMFTransform *iface, DWORD id, DWORD index,
        IMFMediaType **type)
{
    FIXME("iface %p, id %#lx, index %lu, type %p stub!\n", iface, id, index, type);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_GetOutputAvailableType(IMFTransform *iface, DWORD id, DWORD index,
        IMFMediaType **type)
{
    FIXME("iface %p, id %#lx, index %lu, type %p stub!\n", iface, id, index, type);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_SetInputType(IMFTransform *iface, DWORD id, IMFMediaType *type, DWORD flags)
{
    FIXME("iface %p, id %#lx, type %p, flags %#lx stub!\n", iface, id, type, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_SetOutputType(IMFTransform *iface, DWORD id, IMFMediaType *type, DWORD flags)
{
    FIXME("iface %p, id %#lx, type %p, flags %#lx stub!\n", iface, id, type, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_GetInputCurrentType(IMFTransform *iface, DWORD id, IMFMediaType **type)
{
    FIXME("iface %p, id %#lx, type %p stub!\n", iface, id, type);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_GetOutputCurrentType(IMFTransform *iface, DWORD id, IMFMediaType **type)
{
    FIXME("iface %p, id %#lx, type %p stub!\n", iface, id, type);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_GetInputStatus(IMFTransform *iface, DWORD id, DWORD *flags)
{
    FIXME("iface %p, id %#lx, flags %p stub!\n", iface, id, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_GetOutputStatus(IMFTransform *iface, DWORD *flags)
{
    FIXME("iface %p, flags %p stub!\n", iface, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_SetOutputBounds(IMFTransform *iface, LONGLONG lower, LONGLONG upper)
{
    FIXME("iface %p, lower %I64d, upper %I64d stub!\n", iface, lower, upper);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_ProcessEvent(IMFTransform *iface, DWORD id, IMFMediaEvent *event)
{
    FIXME("iface %p, id %#lx, event %p stub!\n", iface, id, event);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_ProcessMessage(IMFTransform *iface, MFT_MESSAGE_TYPE message, ULONG_PTR param)
{
    FIXME("iface %p, message %#x, param %#Ix stub!\n", iface, message, param);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_ProcessInput(IMFTransform *iface, DWORD id, IMFSample *sample, DWORD flags)
{
    FIXME("iface %p, id %#lx, sample %p, flags %#lx stub!\n", iface, id, sample, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI transform_ProcessOutput(IMFTransform *iface, DWORD flags, DWORD count,
        MFT_OUTPUT_DATA_BUFFER *samples, DWORD *status)
{
    FIXME("iface %p, flags %#lx, count %lu, samples %p, status %p stub!\n", iface, flags, count, samples, status);
    return E_NOTIMPL;
}

static const IMFTransformVtbl transform_vtbl =
{
    transform_QueryInterface,
    transform_AddRef,
    transform_Release,
    transform_GetStreamLimits,
    transform_GetStreamCount,
    transform_GetStreamIDs,
    transform_GetInputStreamInfo,
    transform_GetOutputStreamInfo,
    transform_GetAttributes,
    transform_GetInputStreamAttributes,
    transform_GetOutputStreamAttributes,
    transform_DeleteInputStream,
    transform_AddInputStreams,
    transform_GetInputAvailableType,
    transform_GetOutputAvailableType,
    transform_SetInputType,
    transform_SetOutputType,
    transform_GetInputCurrentType,
    transform_GetOutputCurrentType,
    transform_GetInputStatus,
    transform_GetOutputStatus,
    transform_SetOutputBounds,
    transform_ProcessEvent,
    transform_ProcessMessage,
    transform_ProcessInput,
    transform_ProcessOutput,
};

static inline struct wmv_decoder *impl_from_IMediaObject(IMediaObject *iface)
{
    return CONTAINING_RECORD(iface, struct wmv_decoder, IMediaObject_iface);
}

static HRESULT WINAPI media_object_QueryInterface(IMediaObject *iface, REFIID iid, void **obj)
{
    return IUnknown_QueryInterface(impl_from_IMediaObject(iface)->outer, iid, obj);
}

static ULONG WINAPI media_object_AddRef(IMediaObject *iface)
{
    return IUnknown_AddRef(impl_from_IMediaObject(iface)->outer);
}

static ULONG WINAPI media_object_Release(IMediaObject *iface)
{
    return IUnknown_Release(impl_from_IMediaObject(iface)->outer);
}

static HRESULT WINAPI media_object_GetStreamCount(IMediaObject *iface, DWORD *input, DWORD *output)
{
    FIXME("iface %p, input %p, output %p semi-stub!\n", iface, input, output);
    *input = *output = 1;
    return S_OK;
}

static HRESULT WINAPI media_object_GetInputStreamInfo(IMediaObject *iface, DWORD index, DWORD *flags)
{
    FIXME("iface %p, index %lu, flags %p stub!\n", iface, index, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI media_object_GetOutputStreamInfo(IMediaObject *iface, DWORD index, DWORD *flags)
{
    FIXME("iface %p, index %lu, flags %p stub!\n", iface, index, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI media_object_GetInputType(IMediaObject *iface, DWORD index, DWORD type_index,
        DMO_MEDIA_TYPE *type)
{
    struct wmv_decoder *impl = impl_from_IMediaObject(iface);
    struct wg_format format = impl->input_format;

    FIXME("iface %p, index %lu, type_index %lu, type %p semi-stub!\n", iface, index, type_index, type);

    if (type_index >= ARRAY_SIZE(video_formats))
        return VFW_E_NO_TYPES;

    format.major_type = WG_MAJOR_TYPE_VIDEO;
    format.u.video.format = video_formats[index];
    if (!format.u.video.width)
        format.u.video.width = 1920;
    if (!format.u.video.height)
        format.u.video.height = 1080;
    if (!format.u.video.fps_d)
        format.u.video.fps_d = 1;
    if (!format.u.video.fps_n)
        format.u.video.fps_n = 1;

    if (!amt_from_wg_format((AM_MEDIA_TYPE *)type, &format, false))
        return VFW_E_NO_TYPES;

    return S_OK;
}

static HRESULT WINAPI media_object_GetOutputType(IMediaObject *iface, DWORD index, DWORD type_index,
        DMO_MEDIA_TYPE *type)
{
    struct wmv_decoder *impl = impl_from_IMediaObject(iface);
    struct wg_format format = impl->output_format;

    FIXME("iface %p, index %lu, type_index %lu, type %p semi-stub!\n", iface, index, type_index, type);

    if (type_index >= ARRAY_SIZE(video_formats))
        return VFW_E_NO_TYPES;

    format.major_type = WG_MAJOR_TYPE_VIDEO;
    format.u.video.format = video_formats[index];
    if (!format.u.video.width)
        format.u.video.width = 1920;
    if (!format.u.video.height)
        format.u.video.height = 1080;
    if (!format.u.video.fps_d)
        format.u.video.fps_d = 1;
    if (!format.u.video.fps_n)
        format.u.video.fps_n = 1;

    if (!amt_from_wg_format((AM_MEDIA_TYPE *)type, &format, false))
        return VFW_E_NO_TYPES;

    return S_OK;
}

static HRESULT WINAPI media_object_SetInputType(IMediaObject *iface, DWORD index,
        const DMO_MEDIA_TYPE *type, DWORD flags)
{
    struct wmv_decoder *impl = impl_from_IMediaObject(iface);
    struct wg_format wg_format;
    DWORD i;

    FIXME("iface %p, index %lu, type %p, flags %#lx semi-stub!\n", iface, index, type, flags);

    if (flags & DMO_SET_TYPEF_CLEAR)
    {
        memset(&impl->output_format, 0, sizeof(impl->output_format));
        return S_OK;
    }

    if (!amt_to_wg_format((const AM_MEDIA_TYPE *)type, &wg_format))
        return VFW_E_INVALIDMEDIATYPE;

    if (wg_format.major_type != WG_MAJOR_TYPE_VIDEO)
        return VFW_E_INVALIDMEDIATYPE;
    for (i = 0; i < ARRAY_SIZE(video_formats); ++i)
        if (wg_format.u.video.format == video_formats[i])
            break;
    if (i == ARRAY_SIZE(video_formats))
        return VFW_E_INVALIDMEDIATYPE;

    if (!(flags & DMO_SET_TYPEF_TEST_ONLY))
        impl->input_format = wg_format;

    return S_OK;
}

static HRESULT WINAPI media_object_SetOutputType(IMediaObject *iface, DWORD index,
        const DMO_MEDIA_TYPE *type, DWORD flags)
{
    struct wmv_decoder *impl = impl_from_IMediaObject(iface);
    struct wg_format wg_format;
    DWORD i;

    FIXME("iface %p, index %lu, type %p, flags %#lx semi-stub!\n", iface, index, type, flags);

    if (flags & DMO_SET_TYPEF_CLEAR)
    {
        memset(&impl->output_format, 0, sizeof(impl->output_format));
        return S_OK;
    }

    if (!amt_to_wg_format((const AM_MEDIA_TYPE *)type, &wg_format))
        return VFW_E_INVALIDMEDIATYPE;

    if (wg_format.major_type != WG_MAJOR_TYPE_VIDEO)
        return VFW_E_INVALIDMEDIATYPE;
    for (i = 0; i < ARRAY_SIZE(video_formats); ++i)
        if (wg_format.u.video.format == video_formats[i])
            break;
    if (i == ARRAY_SIZE(video_formats))
        return VFW_E_INVALIDMEDIATYPE;

    if (!(flags & DMO_SET_TYPEF_TEST_ONLY))
        impl->output_format = wg_format;

    return S_OK;
}

static HRESULT WINAPI media_object_GetInputCurrentType(IMediaObject *iface, DWORD index, DMO_MEDIA_TYPE *type)
{
    FIXME("iface %p, index %lu, type %p stub!\n", iface, index, type);
    return E_NOTIMPL;
}

static HRESULT WINAPI media_object_GetOutputCurrentType(IMediaObject *iface, DWORD index, DMO_MEDIA_TYPE *type)
{
    FIXME("iface %p, index %lu, type %p stub!\n", iface, index, type);
    return E_NOTIMPL;
}

static HRESULT WINAPI media_object_GetInputSizeInfo(IMediaObject *iface, DWORD index, DWORD *size,
        DWORD *lookahead, DWORD *alignment)
{
    FIXME("iface %p, index %lu, size %p, lookahead %p, alignment %p stub!\n", iface, index, size,
            lookahead, alignment);
    return E_NOTIMPL;
}

static HRESULT WINAPI media_object_GetOutputSizeInfo(IMediaObject *iface, DWORD index, DWORD *size, DWORD *alignment)
{
    struct wmv_decoder *impl = impl_from_IMediaObject(iface);
    AM_MEDIA_TYPE mt;

    TRACE("iface %p, index %lu, size %p, alignment %p semi-stub!\n", iface, index, size, alignment);

    if (!amt_from_wg_format(&mt, &impl->output_format, false))
        return VFW_E_INVALIDMEDIATYPE;

    if (!IsEqualGUID(&mt.formattype, &FORMAT_VideoInfo))
        *size = mt.lSampleSize;
    else
    {
        VIDEOINFOHEADER *format = (VIDEOINFOHEADER *)mt.pbFormat;
        *size = format->bmiHeader.biSizeImage;
    }
    FreeMediaType(&mt);

    *alignment = 1;
    return S_OK;
}

static HRESULT WINAPI media_object_GetInputMaxLatency(IMediaObject *iface, DWORD index, REFERENCE_TIME *latency)
{
    FIXME("iface %p, index %lu, latency %p stub!\n", iface, index, latency);
    return E_NOTIMPL;
}

static HRESULT WINAPI media_object_SetInputMaxLatency(IMediaObject *iface, DWORD index, REFERENCE_TIME latency)
{
    FIXME("iface %p, index %lu, latency %s stub!\n", iface, index, wine_dbgstr_longlong(latency));
    return E_NOTIMPL;
}

static HRESULT WINAPI media_object_Flush(IMediaObject *iface)
{
    struct wmv_decoder *impl = impl_from_IMediaObject(iface);

    TRACE("iface %p.\n", iface);

    if (impl->output.pBuffer) IMediaBuffer_Release(impl->output.pBuffer);
    impl->output.pBuffer = NULL;

    return S_OK;
}

static HRESULT WINAPI media_object_Discontinuity(IMediaObject *iface, DWORD index)
{
    FIXME("iface %p, index %lu semi-stub!\n", iface, index);
    return S_OK;
}

static HRESULT WINAPI media_object_AllocateStreamingResources(IMediaObject *iface)
{
    FIXME("iface %p stub!\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI media_object_FreeStreamingResources(IMediaObject *iface)
{
    FIXME("iface %p stub!\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI media_object_GetInputStatus(IMediaObject *iface, DWORD index, DWORD *flags)
{
    FIXME("iface %p, index %lu, flags %p stub!\n", iface, index, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI media_object_ProcessInput(IMediaObject *iface, DWORD index,
        IMediaBuffer *buffer, DWORD flags, REFERENCE_TIME timestamp, REFERENCE_TIME timelength)
{
    struct wmv_decoder *impl = impl_from_IMediaObject(iface);

    TRACE("iface %p, index %lu, buffer %p, flags %#lx, timestamp %s, timelength %s.\n", iface,
            index, buffer, flags, wine_dbgstr_longlong(timestamp), wine_dbgstr_longlong(timelength));

    if (impl->output.pBuffer) return DMO_E_NOTACCEPTING;

    IMediaBuffer_AddRef((impl->output.pBuffer = buffer));
    impl->output.dwStatus = flags;
    impl->output.rtTimestamp = timestamp;
    impl->output.rtTimelength = timelength;

    return S_OK;
}

static HRESULT WINAPI media_object_ProcessOutput(IMediaObject *iface, DWORD flags, DWORD count,
        DMO_OUTPUT_DATA_BUFFER *buffers, DWORD *status)
{
    struct wmv_decoder *impl = impl_from_IMediaObject(iface);
    BYTE *src_data, *dst_data;
    DWORD src_len, dst_len;
    static int once;

    TRACE("iface %p, flags %#lx, count %lu, buffers %p, status %p.\n", iface, flags, count, buffers, status);
    
    if (!impl->output.pBuffer) return DMO_E_NO_MORE_ITEMS;

    IMediaBuffer_GetBufferAndLength(impl->output.pBuffer, &src_data, &src_len);
    IMediaBuffer_GetBufferAndLength(buffers[0].pBuffer, &dst_data, &dst_len);
    IMediaBuffer_GetMaxLength(buffers[0].pBuffer, &dst_len);
    if (dst_len != src_len && !once++) FIXME("video conversion not implemented!\n");
    memcpy(dst_data, src_data, min(dst_len, src_len));
    IMediaBuffer_SetLength(buffers[0].pBuffer, min(dst_len, src_len));

    *status = 0;
    buffers[0].dwStatus = impl->output.dwStatus;
    buffers[0].rtTimestamp = impl->output.rtTimelength;
    buffers[0].rtTimelength = impl->output.rtTimelength;

    IMediaBuffer_Release(impl->output.pBuffer);
    impl->output.pBuffer = NULL;

    return S_OK;
}

static HRESULT WINAPI media_object_Lock(IMediaObject *iface, LONG lock)
{
    FIXME("iface %p, lock %ld stub!\n", iface, lock);
    return E_NOTIMPL;
}

static const IMediaObjectVtbl media_object_vtbl =
{
    media_object_QueryInterface,
    media_object_AddRef,
    media_object_Release,
    media_object_GetStreamCount,
    media_object_GetInputStreamInfo,
    media_object_GetOutputStreamInfo,
    media_object_GetInputType,
    media_object_GetOutputType,
    media_object_SetInputType,
    media_object_SetOutputType,
    media_object_GetInputCurrentType,
    media_object_GetOutputCurrentType,
    media_object_GetInputSizeInfo,
    media_object_GetOutputSizeInfo,
    media_object_GetInputMaxLatency,
    media_object_SetInputMaxLatency,
    media_object_Flush,
    media_object_Discontinuity,
    media_object_AllocateStreamingResources,
    media_object_FreeStreamingResources,
    media_object_GetInputStatus,
    media_object_ProcessInput,
    media_object_ProcessOutput,
    media_object_Lock,
};

static inline struct wmv_decoder *impl_from_IPropertyBag(IPropertyBag *iface)
{
    return CONTAINING_RECORD(iface, struct wmv_decoder, IPropertyBag_iface);
}

static HRESULT WINAPI property_bag_QueryInterface(IPropertyBag *iface, REFIID iid, void **out)
{
    return IUnknown_QueryInterface(impl_from_IPropertyBag(iface)->outer, iid, out);
}

static ULONG WINAPI property_bag_AddRef(IPropertyBag *iface)
{
    return IUnknown_AddRef(impl_from_IPropertyBag(iface)->outer);
}

static ULONG WINAPI property_bag_Release(IPropertyBag *iface)
{
    return IUnknown_Release(impl_from_IPropertyBag(iface)->outer);
}

static HRESULT WINAPI property_bag_Read(IPropertyBag *iface, const WCHAR *prop_name, VARIANT *value,
        IErrorLog *error_log)
{
    FIXME("iface %p, prop_name %s, value %p, error_log %p stub!\n", iface, debugstr_w(prop_name), value, error_log);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_bag_Write(IPropertyBag *iface, const WCHAR *prop_name, VARIANT *value)
{
    FIXME("iface %p, prop_name %s, value %p stub!\n", iface, debugstr_w(prop_name), value);
    return S_OK;
}

static const IPropertyBagVtbl property_bag_vtbl =
{
    property_bag_QueryInterface,
    property_bag_AddRef,
    property_bag_Release,
    property_bag_Read,
    property_bag_Write,
};

static inline struct wmv_decoder *impl_from_IPropertyStore(IPropertyStore *iface)
{
    return CONTAINING_RECORD(iface, struct wmv_decoder, IPropertyStore_iface);
}

static HRESULT WINAPI property_store_QueryInterface(IPropertyStore *iface, REFIID iid, void **out)
{
    return IUnknown_QueryInterface(impl_from_IPropertyStore(iface)->outer, iid, out);
}

static ULONG WINAPI property_store_AddRef(IPropertyStore *iface)
{
    return IUnknown_AddRef(impl_from_IPropertyStore(iface)->outer);
}

static ULONG WINAPI property_store_Release(IPropertyStore *iface)
{
    return IUnknown_Release(impl_from_IPropertyStore(iface)->outer);
}

static HRESULT WINAPI property_store_GetCount(IPropertyStore *iface, DWORD *count)
{
    FIXME("iface %p, count %p stub!\n", iface, count);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_store_GetAt(IPropertyStore *iface, DWORD index, PROPERTYKEY *key)
{
    FIXME("iface %p, index %lu, key %p stub!\n", iface, index, key);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_store_GetValue(IPropertyStore *iface, REFPROPERTYKEY key, PROPVARIANT *value)
{
    FIXME("iface %p, key %p, value %p stub!\n", iface, key, value);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_store_SetValue(IPropertyStore *iface, REFPROPERTYKEY key, REFPROPVARIANT value)
{
    FIXME("iface %p, key %p, value %p stub!\n", iface, key, value);
    return E_NOTIMPL;
}

static HRESULT WINAPI property_store_Commit(IPropertyStore *iface)
{
    FIXME("iface %p stub!\n", iface);
    return E_NOTIMPL;
}

static const IPropertyStoreVtbl property_store_vtbl =
{
    property_store_QueryInterface,
    property_store_AddRef,
    property_store_Release,
    property_store_GetCount,
    property_store_GetAt,
    property_store_GetValue,
    property_store_SetValue,
    property_store_Commit,
};

HRESULT wmv_decoder_create(IUnknown *outer, IUnknown **out)
{
    struct wmv_decoder *impl;

    TRACE("outer %p, out %p.\n", outer, out);

    if (!(impl = calloc(1, sizeof(*impl))))
        return E_OUTOFMEMORY;

    impl->IUnknown_inner.lpVtbl = &unknown_vtbl;
    impl->IMFTransform_iface.lpVtbl = &transform_vtbl;
    impl->IMediaObject_iface.lpVtbl = &media_object_vtbl;
    impl->IPropertyBag_iface.lpVtbl = &property_bag_vtbl;
    impl->IPropertyStore_iface.lpVtbl = &property_store_vtbl;
    impl->refcount = 1;
    impl->outer = outer ? outer : &impl->IUnknown_inner;

    *out = &impl->IUnknown_inner;
    TRACE("Created %p\n", *out);
    return S_OK;
}
