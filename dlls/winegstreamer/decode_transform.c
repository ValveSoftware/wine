/* GStreamer Decoder Transform
 *
 * Copyright 2021 Derek Lesho
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

#include "wine/debug.h"
#include "wine/heap.h"

WINE_DEFAULT_DEBUG_CHANNEL(mfplat);

const GUID *h264_input_types[] = {&MFVideoFormat_H264};
/* NV12 comes first https://docs.microsoft.com/en-us/windows/win32/medfound/mft-decoder-expose-output-types-in-native-order . thanks to @vitorhnn */
const GUID *h264_output_types[] = {&MFVideoFormat_NV12, &MFVideoFormat_I420, &MFVideoFormat_IYUV, &MFVideoFormat_YUY2, &MFVideoFormat_YV12};

static struct decoder_desc
{
    const GUID *major_type;
    const GUID **input_types;
    unsigned int input_types_count;
    const GUID **output_types;
    unsigned int output_types_count;
} decoder_descs[] =
{
    { /* DECODER_TYPE_H264 */
        &MFMediaType_Video,
        h264_input_types,
        ARRAY_SIZE(h264_input_types),
        h264_output_types,
        ARRAY_SIZE(h264_output_types),
    },
};

struct mf_decoder
{
    IMFTransform IMFTransform_iface;
    LONG refcount;
    enum decoder_type type;
    IMFMediaType *input_type, *output_type;
    CRITICAL_SECTION cs;
    BOOL video;
};

static struct mf_decoder *impl_mf_decoder_from_IMFTransform(IMFTransform *iface)
{
    return CONTAINING_RECORD(iface, struct mf_decoder, IMFTransform_iface);
}

static HRESULT WINAPI mf_decoder_QueryInterface (IMFTransform *iface, REFIID riid, void **out)
{
    TRACE("%p, %s, %p.\n", iface, debugstr_guid(riid), out);

    if (IsEqualIID(riid, &IID_IMFTransform) ||
            IsEqualIID(riid, &IID_IUnknown))
    {
        *out = iface;
        IMFTransform_AddRef(iface);
        return S_OK;
    }

    WARN("Unsupported %s.\n", debugstr_guid(riid));
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI mf_decoder_AddRef(IMFTransform *iface)
{
    struct mf_decoder *decoder = impl_mf_decoder_from_IMFTransform(iface);
    ULONG refcount = InterlockedIncrement(&decoder->refcount);

    TRACE("%p, refcount %u.\n", iface, refcount);

    return refcount;
}

static ULONG WINAPI mf_decoder_Release(IMFTransform *iface)
{
    struct mf_decoder *decoder = impl_mf_decoder_from_IMFTransform(iface);
    ULONG refcount = InterlockedDecrement(&decoder->refcount);

    TRACE("%p, refcount %u.\n", iface, refcount);

    if (!refcount)
    {
        if (decoder->input_type)
        {
            IMFMediaType_Release(decoder->input_type);
            decoder->input_type = NULL;
        }

        if (decoder->output_type)
        {
            IMFMediaType_Release(decoder->output_type);
            decoder->output_type = NULL;
        }

        DeleteCriticalSection(&decoder->cs);

        heap_free(decoder);
    }

    return refcount;
}

static HRESULT WINAPI mf_decoder_GetStreamLimits(IMFTransform *iface, DWORD *input_minimum, DWORD *input_maximum,
        DWORD *output_minimum, DWORD *output_maximum)
{
    TRACE("%p, %p, %p, %p, %p.\n", iface, input_minimum, input_maximum, output_minimum, output_maximum);

    *input_minimum = *input_maximum = *output_minimum = *output_maximum = 1;

    return S_OK;
}

static HRESULT WINAPI mf_decoder_GetStreamCount(IMFTransform *iface, DWORD *inputs, DWORD *outputs)
{
    TRACE("%p %p %p.\n", iface, inputs, outputs);

    *inputs = *outputs = 1;

    return S_OK;
}

static HRESULT WINAPI mf_decoder_GetStreamIDs(IMFTransform *iface, DWORD input_size, DWORD *inputs,
        DWORD output_size, DWORD *outputs)
{
    TRACE("%p %u %p %u %p.\n", iface, input_size, inputs, output_size, outputs);

    return E_NOTIMPL;
}

static HRESULT WINAPI mf_decoder_GetInputStreamInfo(IMFTransform *iface, DWORD id, MFT_INPUT_STREAM_INFO *info)
{
    struct mf_decoder *decoder = impl_mf_decoder_from_IMFTransform(iface);

    TRACE("%p %u %p\n", decoder, id, info);

    if (id != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    info->dwFlags = MFT_INPUT_STREAM_WHOLE_SAMPLES | MFT_INPUT_STREAM_DOES_NOT_ADDREF;
    info->cbAlignment = 0;
    info->cbSize = 0;
    /* TODO: retrieve following fields from gstreamer */
    info->hnsMaxLatency = 0;
    info->cbMaxLookahead = 0;
    return S_OK;
}

static HRESULT WINAPI mf_decoder_GetOutputStreamInfo(IMFTransform *iface, DWORD id, MFT_OUTPUT_STREAM_INFO *info)
{
    struct mf_decoder *decoder = impl_mf_decoder_from_IMFTransform(iface);
    MFT_OUTPUT_STREAM_INFO stream_info = {};
    GUID output_subtype;
    UINT64 framesize;

    TRACE("%p %u %p\n", decoder, id, info);

    if (id != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    EnterCriticalSection(&decoder->cs);

    if (!decoder->output_type)
    {
        LeaveCriticalSection(&decoder->cs);
        return MF_E_TRANSFORM_TYPE_NOT_SET;
    }

    if (decoder->video)
    {
        stream_info.dwFlags = MFT_OUTPUT_STREAM_WHOLE_SAMPLES | MFT_OUTPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER |
                              MFT_OUTPUT_STREAM_FIXED_SAMPLE_SIZE | MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES;
        stream_info.cbSize = 0;
        if (SUCCEEDED(IMFMediaType_GetGUID(decoder->output_type, &MF_MT_SUBTYPE, &output_subtype)) &&
            SUCCEEDED(IMFMediaType_GetUINT64(decoder->output_type, &MF_MT_FRAME_SIZE, &framesize)))
        {
            MFCalculateImageSize(&output_subtype, framesize >> 32, (UINT32) framesize, &stream_info.cbSize);
        }
        if (!stream_info.cbSize)
            ERR("Failed to get desired output buffer size\n");
    }
    else
    {
        stream_info.dwFlags = MFT_OUTPUT_STREAM_FIXED_SAMPLE_SIZE | MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES;
        stream_info.cbSize = 4;
    }
    stream_info.cbAlignment = 0;

    LeaveCriticalSection(&decoder->cs);

    *info = stream_info;

    return S_OK;
}

static HRESULT WINAPI mf_decoder_GetAttributes(IMFTransform *iface, IMFAttributes **attributes)
{
    FIXME("%p, %p.\n", iface, attributes);

    return E_NOTIMPL;
}

static HRESULT WINAPI mf_decoder_GetInputStreamAttributes(IMFTransform *iface, DWORD id,
        IMFAttributes **attributes)
{
    FIXME("%p, %u, %p.\n", iface, id, attributes);

    return E_NOTIMPL;
}

static HRESULT WINAPI mf_decoder_GetOutputStreamAttributes(IMFTransform *iface, DWORD id,
        IMFAttributes **attributes)
{
    FIXME("%p, %u, %p.\n", iface, id, attributes);

    return E_NOTIMPL;
}

static HRESULT WINAPI mf_decoder_DeleteInputStream(IMFTransform *iface, DWORD id)
{
    TRACE("%p, %u.\n", iface, id);

    return E_NOTIMPL;
}

static HRESULT WINAPI mf_decoder_AddInputStreams(IMFTransform *iface, DWORD streams, DWORD *ids)
{
    TRACE("%p, %u, %p.\n", iface, streams, ids);

    return E_NOTIMPL;
}

static HRESULT WINAPI mf_decoder_GetInputAvailableType(IMFTransform *iface, DWORD id, DWORD index,
        IMFMediaType **type)
{
    struct mf_decoder *decoder = impl_mf_decoder_from_IMFTransform(iface);
    IMFMediaType *input_type;
    HRESULT hr;

    TRACE("%p, %u, %u, %p\n", decoder, id, index, type);

    if (id != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (index >= decoder_descs[decoder->type].input_types_count)
        return MF_E_NO_MORE_TYPES;

    if (FAILED(hr = MFCreateMediaType(&input_type)))
        return hr;

    if (FAILED(hr = IMFMediaType_SetGUID(input_type, &MF_MT_MAJOR_TYPE, decoder_descs[decoder->type].major_type)))
    {
        IMFMediaType_Release(input_type);
        return hr;
    }

    if (FAILED(hr = IMFMediaType_SetGUID(input_type, &MF_MT_SUBTYPE, decoder_descs[decoder->type].input_types[index])))
    {
        IMFMediaType_Release(input_type);
        return hr;
    }

    *type = input_type;

    return S_OK;
}

static HRESULT WINAPI mf_decoder_GetOutputAvailableType(IMFTransform *iface, DWORD id, DWORD index,
        IMFMediaType **type)
{
    struct mf_decoder *decoder = impl_mf_decoder_from_IMFTransform(iface);
    IMFMediaType *output_type;
    HRESULT hr;

    TRACE("%p, %u, %u, %p\n", decoder, id, index, type);

    if (id != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (index >= decoder_descs[decoder->type].output_types_count)
        return MF_E_NO_MORE_TYPES;

    if (FAILED(hr = MFCreateMediaType(&output_type)))
        return hr;

    if (FAILED(hr = IMFMediaType_SetGUID(output_type, &MF_MT_MAJOR_TYPE, decoder_descs[decoder->type].major_type)))
    {
        IMFMediaType_Release(output_type);
        return hr;
    }

    if (FAILED(hr = IMFMediaType_SetGUID(output_type, &MF_MT_SUBTYPE, decoder_descs[decoder->type].output_types[index])))
    {
        IMFMediaType_Release(output_type);
        return hr;
    }

    *type = output_type;

    return S_OK;
}

static HRESULT WINAPI mf_decoder_SetInputType(IMFTransform *iface, DWORD id, IMFMediaType *type, DWORD flags)
{
    struct mf_decoder *decoder = impl_mf_decoder_from_IMFTransform(iface);
    struct wg_format input_format;
    GUID major_type, subtype;
    unsigned int i;
    HRESULT hr;

    TRACE("%p, %u, %p, %#x.\n", decoder, id, type, flags);

    if (id != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (!type)
    {
        if (flags & MFT_SET_TYPE_TEST_ONLY)
            return S_OK;

        EnterCriticalSection(&decoder->cs);

        if (decoder->input_type)
        {
            IMFMediaType_Release(decoder->input_type);
            decoder->input_type = NULL;
        }

        LeaveCriticalSection(&decoder->cs);

        return S_OK;
    }

    if (FAILED(IMFMediaType_GetGUID(type, &MF_MT_MAJOR_TYPE, &major_type)))
        return MF_E_INVALIDTYPE;
    if (FAILED(IMFMediaType_GetGUID(type, &MF_MT_SUBTYPE, &subtype)))
        return MF_E_INVALIDTYPE;

    if (!(IsEqualGUID(&major_type, decoder_descs[decoder->type].major_type)))
        return MF_E_INVALIDTYPE;

    for (i = 0; i < decoder_descs[decoder->type].input_types_count; i++)
    {
        if (IsEqualGUID(&subtype, decoder_descs[decoder->type].input_types[i]))
            break;
        if (i == decoder_descs[decoder->type].input_types_count)
            return MF_E_INVALIDTYPE;
    }

    mf_media_type_to_wg_format(type, &input_format);
    if (!input_format.major_type)
        return MF_E_INVALIDTYPE;

    if (flags & MFT_SET_TYPE_TEST_ONLY)
        return S_OK;

    EnterCriticalSection(&decoder->cs);

    hr = S_OK;

    if (!decoder->input_type)
        hr = MFCreateMediaType(&decoder->input_type);

    if (SUCCEEDED(hr) && FAILED(hr = IMFMediaType_CopyAllItems(type, (IMFAttributes*) decoder->input_type)))
    {
        IMFMediaType_Release(decoder->input_type);
        decoder->input_type = NULL;
    }

    LeaveCriticalSection(&decoder->cs);
    return hr;
}

static HRESULT WINAPI mf_decoder_SetOutputType(IMFTransform *iface, DWORD id, IMFMediaType *type, DWORD flags)
{
    struct mf_decoder *decoder = impl_mf_decoder_from_IMFTransform(iface);
    struct wg_format output_format;
    GUID major_type, subtype;
    HRESULT hr;
    unsigned int i;

    TRACE("%p, %u, %p, %#x.\n", decoder, id, type, flags);

    if (id != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (!type)
    {
        if (flags & MFT_SET_TYPE_TEST_ONLY)
            return S_OK;

        EnterCriticalSection(&decoder->cs);

        if (decoder->output_type)
        {
            IMFMediaType_Release(decoder->output_type);
            decoder->output_type = NULL;
        }

        LeaveCriticalSection(&decoder->cs);

        return S_OK;
    }

    if (FAILED(IMFMediaType_GetGUID(type, &MF_MT_MAJOR_TYPE, &major_type)))
        return MF_E_INVALIDTYPE;
    if (FAILED(IMFMediaType_GetGUID(type, &MF_MT_SUBTYPE, &subtype)))
        return MF_E_INVALIDTYPE;

    if (!(IsEqualGUID(&major_type, decoder_descs[decoder->type].major_type)))
        return MF_E_INVALIDTYPE;

    for (i = 0; i < decoder_descs[decoder->type].output_types_count; i++)
    {
        if (IsEqualGUID(&subtype, decoder_descs[decoder->type].output_types[i]))
            break;
        if (i == decoder_descs[decoder->type].output_types_count)
            return MF_E_INVALIDTYPE;
    }

    mf_media_type_to_wg_format(type, &output_format);
    if (!output_format.major_type)
        return MF_E_INVALIDTYPE;

    if (flags & MFT_SET_TYPE_TEST_ONLY)
        return S_OK;

    EnterCriticalSection(&decoder->cs);

    hr = S_OK;

    if (!decoder->output_type)
        hr = MFCreateMediaType(&decoder->output_type);

    if (SUCCEEDED(hr) && FAILED(hr = IMFMediaType_CopyAllItems(type, (IMFAttributes*) decoder->output_type)))
    {
        IMFMediaType_Release(decoder->output_type);
        decoder->output_type = NULL;
    }

    LeaveCriticalSection(&decoder->cs);
    return hr;
}

static HRESULT WINAPI mf_decoder_GetInputCurrentType(IMFTransform *iface, DWORD id, IMFMediaType **type)
{
    FIXME("%p, %u, %p.\n", iface, id, type);

    return E_NOTIMPL;
}

static HRESULT WINAPI mf_decoder_GetOutputCurrentType(IMFTransform *iface, DWORD id, IMFMediaType **type)
{
    FIXME("%p, %u, %p.\n", iface, id, type);

    return E_NOTIMPL;
}

static HRESULT WINAPI mf_decoder_GetInputStatus(IMFTransform *iface, DWORD id, DWORD *flags)
{
    FIXME("%p, %u, %p\n", iface, id, flags);

    return E_NOTIMPL;
}

static HRESULT WINAPI mf_decoder_GetOutputStatus(IMFTransform *iface, DWORD *flags)
{
    FIXME("%p, %p.\n", iface, flags);

    return E_NOTIMPL;
}

static HRESULT WINAPI mf_decoder_SetOutputBounds(IMFTransform *iface, LONGLONG lower, LONGLONG upper)
{
    FIXME("%p, %s, %s.\n", iface, wine_dbgstr_longlong(lower), wine_dbgstr_longlong(upper));

    return E_NOTIMPL;
}

static HRESULT WINAPI mf_decoder_ProcessEvent(IMFTransform *iface, DWORD id, IMFMediaEvent *event)
{
    FIXME("%p, %u, %p.\n", iface, id, event);

    return E_NOTIMPL;
}

static HRESULT WINAPI mf_decoder_ProcessMessage(IMFTransform *iface, MFT_MESSAGE_TYPE message, ULONG_PTR param)
{
    FIXME("%p, %u %lu.\n", iface, message, param);

    return E_NOTIMPL;
}

static HRESULT WINAPI mf_decoder_ProcessInput(IMFTransform *iface, DWORD id, IMFSample *sample, DWORD flags)
{
    FIXME("%p, %u, %p, %#x.\n", iface, id, sample, flags);

    return E_NOTIMPL;
}

static HRESULT WINAPI mf_decoder_ProcessOutput(IMFTransform *iface, DWORD flags, DWORD count,
        MFT_OUTPUT_DATA_BUFFER *samples, DWORD *status)
{
    FIXME("%p, %#x, %u, %p, %p.\n", iface, flags, count, samples, status);

    return E_NOTIMPL;
}

static const IMFTransformVtbl mf_decoder_vtbl =
{
    mf_decoder_QueryInterface,
    mf_decoder_AddRef,
    mf_decoder_Release,
    mf_decoder_GetStreamLimits,
    mf_decoder_GetStreamCount,
    mf_decoder_GetStreamIDs,
    mf_decoder_GetInputStreamInfo,
    mf_decoder_GetOutputStreamInfo,
    mf_decoder_GetAttributes,
    mf_decoder_GetInputStreamAttributes,
    mf_decoder_GetOutputStreamAttributes,
    mf_decoder_DeleteInputStream,
    mf_decoder_AddInputStreams,
    mf_decoder_GetInputAvailableType,
    mf_decoder_GetOutputAvailableType,
    mf_decoder_SetInputType,
    mf_decoder_SetOutputType,
    mf_decoder_GetInputCurrentType,
    mf_decoder_GetOutputCurrentType,
    mf_decoder_GetInputStatus,
    mf_decoder_GetOutputStatus,
    mf_decoder_SetOutputBounds,
    mf_decoder_ProcessEvent,
    mf_decoder_ProcessMessage,
    mf_decoder_ProcessInput,
    mf_decoder_ProcessOutput,
};

HRESULT decode_transform_create(REFIID riid, void **obj, enum decoder_type type)
{
    struct mf_decoder *object;

    TRACE("%s, %p %u.\n", debugstr_guid(riid), obj, type);

    if (!(object = heap_alloc_zero(sizeof(*object))))
        return E_OUTOFMEMORY;

    object->IMFTransform_iface.lpVtbl = &mf_decoder_vtbl;
    object->refcount = 1;

    object->type = type;
    object->video = decoder_descs[type].major_type == &MFMediaType_Video;

    InitializeCriticalSection(&object->cs);

    *obj = &object->IMFTransform_iface;
    return S_OK;
}
