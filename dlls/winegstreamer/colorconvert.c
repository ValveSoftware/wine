/* GStreamer Color Converter
 *
 * Copyright 2020 Derek Lesho
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

#include "config.h"

#include "gst_private.h"

#include "mfapi.h"
#include "mferror.h"
#include "mfidl.h"

#include "wine/debug.h"
#include "wine/heap.h"

WINE_DEFAULT_DEBUG_CHANNEL(mfplat);

static const GUID *raw_types[] = {
    &MFVideoFormat_RGB24,
    &MFVideoFormat_RGB32,
    &MFVideoFormat_RGB555,
    &MFVideoFormat_RGB8,
    &MFVideoFormat_AYUV,
    &MFVideoFormat_I420,
    &MFVideoFormat_IYUV,
    &MFVideoFormat_NV11,
    &MFVideoFormat_NV12,
    &MFVideoFormat_UYVY,
    &MFVideoFormat_v216,
    &MFVideoFormat_v410,
    &MFVideoFormat_YUY2,
    &MFVideoFormat_YVYU,
    &MFVideoFormat_YVYU,
};

struct color_converter
{
    IMFTransform IMFTransform_iface;
    LONG refcount;
    IMFMediaType *input_type;
    IMFMediaType *output_type;
    CRITICAL_SECTION cs;
    BOOL inflight;
    GstElement *container, *appsrc, *appsink;
};

static struct color_converter *impl_color_converter_from_IMFTransform(IMFTransform *iface)
{
    return CONTAINING_RECORD(iface, struct color_converter, IMFTransform_iface);
}

static HRESULT WINAPI color_converter_QueryInterface(IMFTransform *iface, REFIID riid, void **obj)
{
    TRACE("%p, %s, %p.\n", iface, debugstr_guid(riid), obj);

    if (IsEqualIID(riid, &IID_IMFTransform) ||
            IsEqualIID(riid, &IID_IUnknown))
    {
        *obj = iface;
        IMFTransform_AddRef(iface);
        return S_OK;
    }

    WARN("Unsupported %s.\n", debugstr_guid(riid));
    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI color_converter_AddRef(IMFTransform *iface)
{
    struct color_converter *transform = impl_color_converter_from_IMFTransform(iface);
    ULONG refcount = InterlockedIncrement(&transform->refcount);

    TRACE("%p, refcount %u.\n", iface, refcount);

    return refcount;
}

static ULONG WINAPI color_converter_Release(IMFTransform *iface)
{
    struct color_converter *transform = impl_color_converter_from_IMFTransform(iface);
    ULONG refcount = InterlockedDecrement(&transform->refcount);

    TRACE("%p, refcount %u.\n", iface, refcount);

    if (!refcount)
    {
        transform->cs.DebugInfo->Spare[0] = 0;
        DeleteCriticalSection(&transform->cs);
        gst_object_unref(transform->container);
        heap_free(transform);
    }

    return refcount;
}

static HRESULT WINAPI color_converter_GetStreamLimits(IMFTransform *iface, DWORD *input_minimum, DWORD *input_maximum,
        DWORD *output_minimum, DWORD *output_maximum)
{
    TRACE("%p, %p, %p, %p, %p.\n", iface, input_minimum, input_maximum, output_minimum, output_maximum);

    *input_minimum = *input_maximum = *output_minimum = *output_maximum = 1;

    return S_OK;
}

static HRESULT WINAPI color_converter_GetStreamCount(IMFTransform *iface, DWORD *inputs, DWORD *outputs)
{
    TRACE("%p, %p, %p.\n", iface, inputs, outputs);

    *inputs = *outputs = 1;

    return S_OK;
}

static HRESULT WINAPI color_converter_GetStreamIDs(IMFTransform *iface, DWORD input_size, DWORD *inputs,
        DWORD output_size, DWORD *outputs)
{
    TRACE("%p %u %p %u %p.\n", iface, input_size, inputs, output_size, outputs);

    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_GetInputStreamInfo(IMFTransform *iface, DWORD id, MFT_INPUT_STREAM_INFO *info)
{
    FIXME("%p %u %p.\n", iface, id, info);

    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_GetOutputStreamInfo(IMFTransform *iface, DWORD id, MFT_OUTPUT_STREAM_INFO *info)
{
    FIXME("%p %u %p.\n", iface, id, info);

    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_GetAttributes(IMFTransform *iface, IMFAttributes **attributes)
{
    FIXME("%p, %p.\n", iface, attributes);

    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_GetInputStreamAttributes(IMFTransform *iface, DWORD id,
        IMFAttributes **attributes)
{
    FIXME("%p, %u, %p.\n", iface, id, attributes);

    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_GetOutputStreamAttributes(IMFTransform *iface, DWORD id,
        IMFAttributes **attributes)
{
    FIXME("%p, %u, %p.\n", iface, id, attributes);

    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_DeleteInputStream(IMFTransform *iface, DWORD id)
{
    TRACE("%p, %u.\n", iface, id);

    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_AddInputStreams(IMFTransform *iface, DWORD streams, DWORD *ids)
{
    TRACE("%p, %u, %p.\n", iface, streams, ids);

    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_GetInputAvailableType(IMFTransform *iface, DWORD id, DWORD index,
        IMFMediaType **type)
{
    IMFMediaType *ret;
    HRESULT hr;

    TRACE("%p, %u, %u, %p.\n", iface, id, index, type);

    if (id != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (index >= ARRAY_SIZE(raw_types))
        return MF_E_NO_MORE_TYPES;

    if (FAILED(hr = MFCreateMediaType(&ret)))
        return hr;

    if (FAILED(hr = IMFMediaType_SetGUID(ret, &MF_MT_MAJOR_TYPE, &MFMediaType_Video)))
    {
        IMFMediaType_Release(ret);
        return hr;
    }

    if (FAILED(hr = IMFMediaType_SetGUID(ret, &MF_MT_SUBTYPE, raw_types[index])))
    {
        IMFMediaType_Release(ret);
        return hr;
    }

    *type = ret;

    return S_OK;
}

static HRESULT WINAPI color_converter_GetOutputAvailableType(IMFTransform *iface, DWORD id, DWORD index,
        IMFMediaType **type)
{
    IMFMediaType *output_type;
    HRESULT hr;

    struct color_converter *converter = impl_color_converter_from_IMFTransform(iface);

    TRACE("%p, %u, %u, %p.\n", iface, id, index, type);

    if (id != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (index >= ARRAY_SIZE(raw_types))
        return MF_E_NO_MORE_TYPES;

    if (FAILED(hr = MFCreateMediaType(&output_type)))
        return hr;

    EnterCriticalSection(&converter->cs);

    if (!(converter->input_type))
    {
        LeaveCriticalSection(&converter->cs);
        return MF_E_TRANSFORM_TYPE_NOT_SET;
    }

    IMFMediaType_CopyAllItems(converter->input_type, (IMFAttributes *)output_type);

    LeaveCriticalSection(&converter->cs);

    if (FAILED(hr = IMFMediaType_SetGUID(output_type, &MF_MT_SUBTYPE, raw_types[index])))
    {
        IMFMediaType_Release(output_type);
        return hr;
    }

    *type = output_type;

    return S_OK;
}

static HRESULT WINAPI color_converter_SetInputType(IMFTransform *iface, DWORD id, IMFMediaType *type, DWORD flags)
{
    GUID major_type, subtype;
    GstCaps *input_caps;
    unsigned int i;
    HRESULT hr;

    struct color_converter *converter = impl_color_converter_from_IMFTransform(iface);

    TRACE("%p, %u, %p, %#x.\n", iface, id, type, flags);

    if (id != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (!type)
    {
        if (flags & MFT_SET_TYPE_TEST_ONLY)
            return S_OK;

        EnterCriticalSection(&converter->cs);

        converter->inflight = FALSE;
        gst_element_set_state(converter->container, GST_STATE_READY);

        if (converter->input_type)
        {
            IMFMediaType_Release(converter->input_type);
            converter->input_type = NULL;
        }

        LeaveCriticalSection(&converter->cs);

        return S_OK;
    }

    if (FAILED(IMFMediaType_GetGUID(type, &MF_MT_MAJOR_TYPE, &major_type)))
        return MF_E_INVALIDTYPE;
    if (FAILED(IMFMediaType_GetGUID(type, &MF_MT_SUBTYPE, &subtype)))
        return MF_E_INVALIDTYPE;

    if (!(IsEqualGUID(&major_type, &MFMediaType_Video)))
        return MF_E_INVALIDTYPE;

    for (i = 0; i < ARRAY_SIZE(raw_types); i++)
    {
        if (IsEqualGUID(&subtype, raw_types[i]))
            break;
    }

    if (i == ARRAY_SIZE(raw_types))
        return MF_E_INVALIDTYPE;

    if (!(input_caps = caps_from_mf_media_type(type)))
        return MF_E_INVALIDTYPE;

    if (flags & MFT_SET_TYPE_TEST_ONLY)
    {
        gst_caps_unref(input_caps);
        return S_OK;
    }

    EnterCriticalSection(&converter->cs);

    hr = S_OK;
    converter->inflight = FALSE;
    gst_element_set_state(converter->container, GST_STATE_READY);

    if (!converter->input_type)
        hr = MFCreateMediaType(&converter->input_type);

    if (SUCCEEDED(hr))
        hr = IMFMediaType_CopyAllItems(type, (IMFAttributes *) converter->input_type);

    g_object_set(converter->appsrc, "caps", input_caps, NULL);
    gst_caps_unref(input_caps);

    if (FAILED(hr))
    {
        IMFMediaType_Release(converter->input_type);
        converter->input_type = NULL;
    }

    if (converter->input_type && converter->output_type)
        gst_element_set_state(converter->container, GST_STATE_PLAYING);

    LeaveCriticalSection(&converter->cs);

    return hr;
}

static HRESULT WINAPI color_converter_SetOutputType(IMFTransform *iface, DWORD id, IMFMediaType *type, DWORD flags)
{
    GUID major_type, subtype;
    GstCaps *output_caps;
    unsigned int i;
    HRESULT hr;

    struct color_converter *converter = impl_color_converter_from_IMFTransform(iface);

    TRACE("%p, %u, %p, %#x.\n", iface, id, type, flags);

    if (id != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (!type)
    {
        if (flags & MFT_SET_TYPE_TEST_ONLY)
            return S_OK;

        EnterCriticalSection(&converter->cs);

        converter->inflight = FALSE;
        gst_element_set_state(converter->container, GST_STATE_READY);

        if (converter->output_type)
        {
            IMFMediaType_Release(converter->output_type);
            converter->output_type = NULL;
        }

        LeaveCriticalSection(&converter->cs);

        return S_OK;
    }

    if (FAILED(IMFMediaType_GetGUID(type, &MF_MT_MAJOR_TYPE, &major_type)))
        return MF_E_INVALIDTYPE;
    if (FAILED(IMFMediaType_GetGUID(type, &MF_MT_SUBTYPE, &subtype)))
        return MF_E_INVALIDTYPE;

    if (!(IsEqualGUID(&major_type, &MFMediaType_Video)))
        return MF_E_INVALIDTYPE;

    for (i = 0; i < ARRAY_SIZE(raw_types); i++)
    {
        if (IsEqualGUID(&subtype, raw_types[i]))
            break;
    }

    if (i == ARRAY_SIZE(raw_types))
        return MF_E_INVALIDTYPE;

    if (!(output_caps = caps_from_mf_media_type(type)))
        return MF_E_INVALIDTYPE;

    if (flags & MFT_SET_TYPE_TEST_ONLY)
    {
        gst_caps_unref(output_caps);
        return S_OK;
    }

    EnterCriticalSection(&converter->cs);

    hr = S_OK;
    converter->inflight = FALSE;
    gst_element_set_state(converter->container, GST_STATE_READY);

    if (!converter->output_type)
        hr = MFCreateMediaType(&converter->output_type);

    if (SUCCEEDED(hr))
        hr = IMFMediaType_CopyAllItems(type, (IMFAttributes *) converter->output_type);

    g_object_set(converter->appsink, "caps", output_caps, NULL);
    gst_caps_unref(output_caps);

    if (FAILED(hr))
    {
        IMFMediaType_Release(converter->output_type);
        converter->output_type = NULL;
    }

    if (converter->input_type && converter->output_type)
        gst_element_set_state(converter->container, GST_STATE_PLAYING);

    LeaveCriticalSection(&converter->cs);

    return hr;
}

static HRESULT WINAPI color_converter_GetInputCurrentType(IMFTransform *iface, DWORD id, IMFMediaType **type)
{
    FIXME("%p, %u, %p.\n", iface, id, type);

    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_GetOutputCurrentType(IMFTransform *iface, DWORD id, IMFMediaType **type)
{
    FIXME("%p, %u, %p.\n", iface, id, type);

    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_GetInputStatus(IMFTransform *iface, DWORD id, DWORD *flags)
{
    FIXME("%p, %u, %p.\n", iface, id, flags);

    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_GetOutputStatus(IMFTransform *iface, DWORD *flags)
{
    FIXME("%p, %p.\n", iface, flags);

    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_SetOutputBounds(IMFTransform *iface, LONGLONG lower, LONGLONG upper)
{
    FIXME("%p, %s, %s.\n", iface, wine_dbgstr_longlong(lower), wine_dbgstr_longlong(upper));

    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_ProcessEvent(IMFTransform *iface, DWORD id, IMFMediaEvent *event)
{
    TRACE("%p, %u, %p.\n", iface, id, event);

    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_ProcessMessage(IMFTransform *iface, MFT_MESSAGE_TYPE message, ULONG_PTR param)
{
    TRACE("%p, %u %lu.\n", iface, message, param);

    switch(message)
    {
        case MFT_MESSAGE_NOTIFY_BEGIN_STREAMING:
            return S_OK;
        default:
            FIXME("Unhandled message type %x.\n", message);
            return E_NOTIMPL;
    }
}

static HRESULT WINAPI color_converter_ProcessInput(IMFTransform *iface, DWORD id, IMFSample *sample, DWORD flags)
{
    GstBuffer *gst_buffer;
    int ret;

    struct color_converter *converter = impl_color_converter_from_IMFTransform(iface);

    TRACE("%p, %u, %p, %#x.\n", iface, id, sample, flags);

    if (flags)
        WARN("Unsupported flags %#x\n", flags);

    if (id != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    EnterCriticalSection(&converter->cs);

    if (!converter->input_type || !converter->output_type)
    {
        LeaveCriticalSection(&converter->cs);
        return MF_E_TRANSFORM_TYPE_NOT_SET;
    }

    if (converter->inflight)
    {
        LeaveCriticalSection(&converter->cs);
        return MF_E_NOTACCEPTING;
    }

    if (!(gst_buffer = gst_buffer_from_mf_sample(sample)))
    {
        LeaveCriticalSection(&converter->cs);
        return E_FAIL;
    }

    g_signal_emit_by_name(converter->appsrc, "push-buffer", gst_buffer, &ret);
    gst_buffer_unref(gst_buffer);
    if (ret != GST_FLOW_OK)
    {
        ERR("Couldn't push buffer, (%s)\n", gst_flow_get_name(ret));
        LeaveCriticalSection(&converter->cs);
        return E_FAIL;
    }

    converter->inflight = TRUE;
    LeaveCriticalSection(&converter->cs);

    return S_OK;
}

static HRESULT WINAPI color_converter_ProcessOutput(IMFTransform *iface, DWORD flags, DWORD count,
        MFT_OUTPUT_DATA_BUFFER *samples, DWORD *status)
{
    GstSample *sample;

    struct color_converter *converter = impl_color_converter_from_IMFTransform(iface);

    TRACE("%p, %#x, %u, %p, %p.\n", iface, flags, count, samples, status);

    if (flags)
        WARN("Unsupported flags %#x\n", flags);

    if (!count)
        return S_OK;

    if (count != 1)
        return MF_E_INVALIDSTREAMNUMBER;

    if (samples[0].dwStreamID != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    EnterCriticalSection(&converter->cs);

    if (!converter->input_type || !converter->output_type)
    {
        LeaveCriticalSection(&converter->cs);
        return MF_E_TRANSFORM_TYPE_NOT_SET;
    }

    if (!converter->inflight)
    {
        LeaveCriticalSection(&converter->cs);
        return MF_E_TRANSFORM_NEED_MORE_INPUT;
    }

    g_signal_emit_by_name(converter->appsink, "pull-sample", &sample);

    converter->inflight = FALSE;

    samples[0].pSample = mf_sample_from_gst_buffer(gst_sample_get_buffer(sample));
    gst_sample_unref(sample);
    samples[0].dwStatus = S_OK;
    samples[0].pEvents = NULL;
    *status = 0;

    LeaveCriticalSection(&converter->cs);

    return S_OK;

    return E_NOTIMPL;
}

static const IMFTransformVtbl color_converter_vtbl =
{
    color_converter_QueryInterface,
    color_converter_AddRef,
    color_converter_Release,
    color_converter_GetStreamLimits,
    color_converter_GetStreamCount,
    color_converter_GetStreamIDs,
    color_converter_GetInputStreamInfo,
    color_converter_GetOutputStreamInfo,
    color_converter_GetAttributes,
    color_converter_GetInputStreamAttributes,
    color_converter_GetOutputStreamAttributes,
    color_converter_DeleteInputStream,
    color_converter_AddInputStreams,
    color_converter_GetInputAvailableType,
    color_converter_GetOutputAvailableType,
    color_converter_SetInputType,
    color_converter_SetOutputType,
    color_converter_GetInputCurrentType,
    color_converter_GetOutputCurrentType,
    color_converter_GetInputStatus,
    color_converter_GetOutputStatus,
    color_converter_SetOutputBounds,
    color_converter_ProcessEvent,
    color_converter_ProcessMessage,
    color_converter_ProcessInput,
    color_converter_ProcessOutput,
};

HRESULT color_converter_create(REFIID riid, void **ret)
{
    struct color_converter *object;
    GstElement *videoconvert;

    TRACE("%s %p\n", debugstr_guid(riid), ret);

    if (!(object = heap_alloc_zero(sizeof(*object))))
        return E_OUTOFMEMORY;

    object->IMFTransform_iface.lpVtbl = &color_converter_vtbl;
    object->refcount = 1;

    InitializeCriticalSection(&object->cs);
    object->cs.DebugInfo->Spare[0] = (DWORD_PTR)(__FILE__ ": color_converter_lock");

    object->container = gst_bin_new(NULL);

    if (!(object->appsrc = gst_element_factory_make("appsrc", NULL)))
    {
        ERR("Failed to create appsrc, are %u-bit Gstreamer \"base\" plugins installed?\n",
                8 * (int)sizeof(void *));
        IMFTransform_Release(&object->IMFTransform_iface);
        return E_FAIL;
    }
    gst_bin_add(GST_BIN(object->container), object->appsrc);

    if (!(videoconvert = gst_element_factory_make("videoconvert", NULL)))
    {
        ERR("Failed to create videoconvert, are %u-bit Gstreamer \"base\" plugins installed?\n",
                8 * (int)sizeof(void *));
        IMFTransform_Release(&object->IMFTransform_iface);
        return E_FAIL;
    }
    gst_bin_add(GST_BIN(object->container), videoconvert);

    if (!(object->appsink = gst_element_factory_make("appsink", NULL)))
    {
        ERR("Failed to create appsink, are %u-bit Gstreamer \"base\" plugins installed?\n",
                8 * (int)sizeof(void *));
        IMFTransform_Release(&object->IMFTransform_iface);
        return E_FAIL;
    }
    gst_bin_add(GST_BIN(object->container), object->appsink);

    if (!gst_element_link(object->appsrc, videoconvert))
    {
        ERR("Failed to link appsrc to videoconvert\n");
        IMFTransform_Release(&object->IMFTransform_iface);
        return E_FAIL;
    }

    if (!gst_element_link(videoconvert, object->appsink))
    {
        ERR("Failed to link videoconvert to appsink\n");
        IMFTransform_Release(&object->IMFTransform_iface);
        return E_FAIL;
    }

    *ret = &object->IMFTransform_iface;
    return S_OK;
}
