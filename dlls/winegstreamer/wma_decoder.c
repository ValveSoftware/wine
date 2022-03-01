/* WMA Decoder Transform
 *
 * Copyright 2022 Rémi Bernon for CodeWeavers
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

#include "wine/debug.h"
#include "wine/heap.h"

WINE_DEFAULT_DEBUG_CHANNEL(mfplat);

struct wma_decoder
{
    IMFTransform IMFTransform_iface;
    LONG refcount;
};

static struct wma_decoder *impl_from_IMFTransform(IMFTransform *iface)
{
    return CONTAINING_RECORD(iface, struct wma_decoder, IMFTransform_iface);
}

static HRESULT WINAPI wma_decoder_QueryInterface(IMFTransform *iface, REFIID iid, void **out)
{
    struct wma_decoder *decoder = impl_from_IMFTransform(iface);

    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IMFTransform))
        *out = &decoder->IMFTransform_iface;
    else
    {
        *out = NULL;
        WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}

static ULONG WINAPI wma_decoder_AddRef(IMFTransform *iface)
{
    struct wma_decoder *decoder = impl_from_IMFTransform(iface);
    ULONG refcount = InterlockedIncrement(&decoder->refcount);

    TRACE("iface %p increasing refcount to %u.\n", decoder, refcount);

    return refcount;
}

static ULONG WINAPI wma_decoder_Release(IMFTransform *iface)
{
    struct wma_decoder *decoder = impl_from_IMFTransform(iface);
    ULONG refcount = InterlockedDecrement(&decoder->refcount);

    TRACE("iface %p decreasing refcount to %u.\n", decoder, refcount);

    if (!refcount)
        free(decoder);

    return refcount;
}

static HRESULT WINAPI wma_decoder_GetStreamLimits(IMFTransform *iface, DWORD *input_minimum,
        DWORD *input_maximum, DWORD *output_minimum, DWORD *output_maximum)
{
    FIXME("iface %p, input_minimum %p, input_maximum %p, output_minimum %p, output_maximum %p stub!\n",
            iface, input_minimum, input_maximum, output_minimum, output_maximum);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_GetStreamCount(IMFTransform *iface, DWORD *inputs, DWORD *outputs)
{
    FIXME("iface %p, inputs %p, outputs %p stub!\n", iface, inputs, outputs);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_GetStreamIDs(IMFTransform *iface, DWORD input_size, DWORD *inputs,
        DWORD output_size, DWORD *outputs)
{
    FIXME("iface %p, input_size %u, inputs %p, output_size %u, outputs %p stub!\n", iface,
            input_size, inputs, output_size, outputs);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_GetInputStreamInfo(IMFTransform *iface, DWORD id, MFT_INPUT_STREAM_INFO *info)
{
    FIXME("iface %p, id %u, info %p stub!\n", iface, id, info);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_GetOutputStreamInfo(IMFTransform *iface, DWORD id, MFT_OUTPUT_STREAM_INFO *info)
{
    FIXME("iface %p, id %u, info %p stub!\n", iface, id, info);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_GetAttributes(IMFTransform *iface, IMFAttributes **attributes)
{
    FIXME("iface %p, attributes %p stub!\n", iface, attributes);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_GetInputStreamAttributes(IMFTransform *iface, DWORD id, IMFAttributes **attributes)
{
    FIXME("iface %p, id %u, attributes %p stub!\n", iface, id, attributes);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_GetOutputStreamAttributes(IMFTransform *iface, DWORD id, IMFAttributes **attributes)
{
    FIXME("iface %p, id %u, attributes %p stub!\n", iface, id, attributes);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_DeleteInputStream(IMFTransform *iface, DWORD id)
{
    FIXME("iface %p, id %u stub!\n", iface, id);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_AddInputStreams(IMFTransform *iface, DWORD streams, DWORD *ids)
{
    FIXME("iface %p, streams %u, ids %p stub!\n", iface, streams, ids);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_GetInputAvailableType(IMFTransform *iface, DWORD id, DWORD index,
        IMFMediaType **type)
{
    FIXME("iface %p, id %u, index %u, type %p stub!\n", iface, id, index, type);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_GetOutputAvailableType(IMFTransform *iface, DWORD id, DWORD index,
        IMFMediaType **type)
{
    FIXME("iface %p, id %u, index %u, type %p stub!\n", iface, id, index, type);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_SetInputType(IMFTransform *iface, DWORD id, IMFMediaType *type, DWORD flags)
{
    FIXME("iface %p, id %u, type %p, flags %#x stub!\n", iface, id, type, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_SetOutputType(IMFTransform *iface, DWORD id, IMFMediaType *type, DWORD flags)
{
    FIXME("iface %p, id %u, type %p, flags %#x stub!\n", iface, id, type, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_GetInputCurrentType(IMFTransform *iface, DWORD id, IMFMediaType **type)
{
    FIXME("iface %p, id %u, type %p stub!\n", iface, id, type);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_GetOutputCurrentType(IMFTransform *iface, DWORD id, IMFMediaType **type)
{
    FIXME("iface %p, id %u, type %p stub!\n", iface, id, type);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_GetInputStatus(IMFTransform *iface, DWORD id, DWORD *flags)
{
    FIXME("iface %p, id %u, flags %p stub!\n", iface, id, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_GetOutputStatus(IMFTransform *iface, DWORD *flags)
{
    FIXME("iface %p, flags %p stub!\n", iface, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_SetOutputBounds(IMFTransform *iface, LONGLONG lower, LONGLONG upper)
{
    FIXME("iface %p, lower %s, upper %s stub!\n", iface, wine_dbgstr_longlong(lower),
            wine_dbgstr_longlong(upper));
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_ProcessEvent(IMFTransform *iface, DWORD id, IMFMediaEvent *event)
{
    FIXME("iface %p, id %u, event %p stub!\n", iface, id, event);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_ProcessMessage(IMFTransform *iface, MFT_MESSAGE_TYPE message, ULONG_PTR param)
{
    FIXME("iface %p, message %#x, param %p stub!\n", iface, message, (void *)param);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_ProcessInput(IMFTransform *iface, DWORD id, IMFSample *sample, DWORD flags)
{
    FIXME("iface %p, id %u, sample %p, flags %#x stub!\n", iface, id, sample, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI wma_decoder_ProcessOutput(IMFTransform *iface, DWORD flags, DWORD count,
        MFT_OUTPUT_DATA_BUFFER *samples, DWORD *status)
{
    FIXME("iface %p, flags %#x, count %u, samples %p, status %p stub!\n", iface, flags, count, samples, status);
    return E_NOTIMPL;
}

static const IMFTransformVtbl wma_decoder_vtbl =
{
    wma_decoder_QueryInterface,
    wma_decoder_AddRef,
    wma_decoder_Release,
    wma_decoder_GetStreamLimits,
    wma_decoder_GetStreamCount,
    wma_decoder_GetStreamIDs,
    wma_decoder_GetInputStreamInfo,
    wma_decoder_GetOutputStreamInfo,
    wma_decoder_GetAttributes,
    wma_decoder_GetInputStreamAttributes,
    wma_decoder_GetOutputStreamAttributes,
    wma_decoder_DeleteInputStream,
    wma_decoder_AddInputStreams,
    wma_decoder_GetInputAvailableType,
    wma_decoder_GetOutputAvailableType,
    wma_decoder_SetInputType,
    wma_decoder_SetOutputType,
    wma_decoder_GetInputCurrentType,
    wma_decoder_GetOutputCurrentType,
    wma_decoder_GetInputStatus,
    wma_decoder_GetOutputStatus,
    wma_decoder_SetOutputBounds,
    wma_decoder_ProcessEvent,
    wma_decoder_ProcessMessage,
    wma_decoder_ProcessInput,
    wma_decoder_ProcessOutput,
};

HRESULT wma_decoder_create(REFIID riid, void **ret)
{
    struct wma_decoder *decoder;

    TRACE("riid %s, ret %p.\n", debugstr_guid(riid), ret);

    if (!(decoder = calloc(1, sizeof(*decoder))))
        return E_OUTOFMEMORY;

    decoder->IMFTransform_iface.lpVtbl = &wma_decoder_vtbl;
    decoder->refcount = 1;

    *ret = &decoder->IMFTransform_iface;
    TRACE("Created decoder %p\n", *ret);
    return S_OK;
}
