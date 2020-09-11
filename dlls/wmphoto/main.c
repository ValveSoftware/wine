/*
 * Copyright 2017 Vincent Povirk for CodeWeavers
 * Copyright 2020 Rémi Bernon for CodeWeavers
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
#include "wine/port.h"

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "objbase.h"
#include "rpcproxy.h"
#include "wincodecsdk.h"

#include "wine/debug.h"
#include "wine/unicode.h"

#ifdef SONAME_LIBJXRGLUE
#undef ERR
#define ERR JXR_ERR
#include <JXRGlue.h>
#undef ERR
#define ERR WINE_ERR
#endif

WINE_DEFAULT_DEBUG_CHANNEL(wincodecs);

#ifdef SONAME_LIBJXRGLUE

static void *libjxrglue;
static typeof(PKImageDecode_Create_WMP) *pPKImageDecode_Create_WMP;

static inline const char *debug_wic_rect(const WICRect *rect)
{
    if (!rect) return "(null)";
    return wine_dbg_sprintf("(%u,%u)-(%u,%u)", rect->X, rect->Y, rect->Width, rect->Height);
}

static const struct
{
    const WICPixelFormatGUID *format;
    int bpp;
} pixel_format_bpp[] =
{
    {&GUID_WICPixelFormat128bppRGBAFixedPoint, 128},
    {&GUID_WICPixelFormat128bppRGBAFloat, 128},
    {&GUID_WICPixelFormat128bppRGBFloat, 128},
    {&GUID_WICPixelFormat16bppBGR555, 16},
    {&GUID_WICPixelFormat16bppBGR565, 16},
    {&GUID_WICPixelFormat16bppGray, 16},
    {&GUID_WICPixelFormat16bppGrayFixedPoint, 16},
    {&GUID_WICPixelFormat16bppGrayHalf, 16},
    {&GUID_WICPixelFormat24bppBGR, 24},
    {&GUID_WICPixelFormat24bppRGB, 24},
    {&GUID_WICPixelFormat32bppBGR, 32},
    {&GUID_WICPixelFormat32bppBGR101010, 32},
    {&GUID_WICPixelFormat32bppBGRA, 32},
    {&GUID_WICPixelFormat32bppCMYK, 32},
    {&GUID_WICPixelFormat32bppGrayFixedPoint, 32},
    {&GUID_WICPixelFormat32bppGrayFloat, 32},
    {&GUID_WICPixelFormat32bppRGBE, 32},
    {&GUID_WICPixelFormat40bppCMYKAlpha, 40},
    {&GUID_WICPixelFormat48bppRGB, 48},
    {&GUID_WICPixelFormat48bppRGBFixedPoint, 48},
    {&GUID_WICPixelFormat48bppRGBHalf, 48},
    {&GUID_WICPixelFormat64bppCMYK, 64},
    {&GUID_WICPixelFormat64bppRGBA, 64},
    {&GUID_WICPixelFormat64bppRGBAFixedPoint, 64},
    {&GUID_WICPixelFormat64bppRGBAHalf, 64},
    {&GUID_WICPixelFormat80bppCMYKAlpha, 80},
    {&GUID_WICPixelFormat8bppGray, 8},
    {&GUID_WICPixelFormat96bppRGBFixedPoint, 96},
    {&GUID_WICPixelFormatBlackWhite, 1},
};

static inline int pixel_format_get_bpp(const WICPixelFormatGUID *format)
{
    int i;
    for (i = 0; i < ARRAY_SIZE(pixel_format_bpp); ++i)
        if (IsEqualGUID(format, pixel_format_bpp[i].format)) return pixel_format_bpp[i].bpp;
    return 0;
}

struct wmp_decoder
{
    IWICBitmapDecoder IWICBitmapDecoder_iface;
    IWICBitmapFrameDecode IWICBitmapFrameDecode_iface;
    IWICMetadataBlockReader IWICMetadataBlockReader_iface;
    struct WMPStream wmp_stream;
    LONG ref;
    IStream *stream;
    PKImageDecode *decoder;
    void *image_data;
    CRITICAL_SECTION lock;
};

static inline struct wmp_decoder *impl_from_IWICBitmapDecoder(IWICBitmapDecoder *iface)
{
    return CONTAINING_RECORD(iface, struct wmp_decoder, IWICBitmapDecoder_iface);
}

static inline struct wmp_decoder *impl_from_IWICBitmapFrameDecode(IWICBitmapFrameDecode *iface)
{
    return CONTAINING_RECORD(iface, struct wmp_decoder, IWICBitmapFrameDecode_iface);
}

static inline struct wmp_decoder *impl_from_IWICMetadataBlockReader(IWICMetadataBlockReader *iface)
{
    return CONTAINING_RECORD(iface, struct wmp_decoder, IWICMetadataBlockReader_iface);
}

static inline struct wmp_decoder *impl_from_WMPStream(struct WMPStream *iface)
{
    return CONTAINING_RECORD(iface, struct wmp_decoder, wmp_stream);
}

static JXR_ERR wmp_stream_Close(struct WMPStream **piface)
{
    TRACE("iface %p.\n", piface);
    return WMP_errSuccess;
}

static Bool wmp_stream_EOS(struct WMPStream *iface)
{
    FIXME("iface %p, stub!\n", iface);
    return FALSE;
}

static JXR_ERR wmp_stream_Read(struct WMPStream *iface, void *buf, size_t len)
{
    struct wmp_decoder *This = impl_from_WMPStream(iface);
    HRESULT hr;
    ULONG count;

    TRACE("iface %p, buf %p, len %zx.\n", iface, buf, len);

    EnterCriticalSection(&This->lock);
    hr = IStream_Read(This->stream, buf, len, &count);
    LeaveCriticalSection(&This->lock);

    return FAILED(hr) ? WMP_errFail : WMP_errSuccess;
}

static JXR_ERR wmp_stream_Write(struct WMPStream *iface, const void *buf, size_t len)
{
    struct wmp_decoder *This = impl_from_WMPStream(iface);
    HRESULT hr;
    ULONG count;

    TRACE("iface %p, buf %p, len %zx.\n", iface, buf, len);

    EnterCriticalSection(&This->lock);
    hr = IStream_Write(This->stream, buf, len, &count);
    LeaveCriticalSection(&This->lock);

    return FAILED(hr) ? WMP_errFail : WMP_errSuccess;
}

static JXR_ERR wmp_stream_SetPos(struct WMPStream *iface, size_t pos)
{
    struct wmp_decoder *This = impl_from_WMPStream(iface);
    LARGE_INTEGER move;
    HRESULT hr;

    TRACE("iface %p, pos %zx.\n", iface, pos);

    move.QuadPart = pos;
    EnterCriticalSection(&This->lock);
    hr = IStream_Seek(This->stream, move, STREAM_SEEK_SET, NULL);
    LeaveCriticalSection(&This->lock);

    return FAILED(hr) ? WMP_errFail : WMP_errSuccess;
}

static JXR_ERR wmp_stream_GetPos(struct WMPStream *iface, size_t *pos)
{
    struct wmp_decoder *This = impl_from_WMPStream(iface);
    ULARGE_INTEGER curr;
    LARGE_INTEGER move;
    HRESULT hr;

    TRACE("iface %p, pos %p.\n", iface, pos);

    move.QuadPart = 0;
    EnterCriticalSection(&This->lock);
    hr = IStream_Seek(This->stream, move, STREAM_SEEK_CUR, &curr);
    LeaveCriticalSection(&This->lock);
    *pos = curr.QuadPart;

    return FAILED(hr) ? WMP_errFail : WMP_errSuccess;
}

static HRESULT WINAPI wmp_decoder_QueryInterface(IWICBitmapDecoder *iface, REFIID iid, void **out)
{
    struct wmp_decoder *This = impl_from_IWICBitmapDecoder(iface);

    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (!out) return E_INVALIDARG;

    *out = NULL;
    if (!IsEqualIID(&IID_IUnknown, iid) && !IsEqualIID(&IID_IWICBitmapDecoder, iid))
        return E_NOINTERFACE;

    *out = &This->IWICBitmapDecoder_iface;
    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}

static ULONG WINAPI wmp_decoder_AddRef(IWICBitmapDecoder *iface)
{
    struct wmp_decoder *This = impl_from_IWICBitmapDecoder(iface);
    ULONG ref = InterlockedIncrement(&This->ref);
    TRACE("iface %p -> ref %u.\n", iface, ref);
    return ref;
}

static ULONG WINAPI wmp_decoder_Release(IWICBitmapDecoder *iface)
{
    struct wmp_decoder *This = impl_from_IWICBitmapDecoder(iface);
    ULONG ref = InterlockedDecrement(&This->ref);
    TRACE("iface %p -> ref %u.\n", iface, ref);

    if (ref == 0)
    {
        This->lock.DebugInfo->Spare[0] = 0;
        DeleteCriticalSection(&This->lock);
        if (This->image_data) HeapFree(GetProcessHeap(), 0, This->image_data);
        if (This->decoder) This->decoder->Release(&This->decoder);
        if (This->stream) IStream_Release(This->stream);
        HeapFree(GetProcessHeap(), 0, This);
    }

    return ref;
}

static HRESULT WINAPI wmp_decoder_QueryCapability(IWICBitmapDecoder *iface, IStream *stream, DWORD *capability)
{
    FIXME("iface %p, stream %p, capability %p, stub!\n", iface, stream, capability);
    return E_NOTIMPL;
}

static HRESULT WINAPI wmp_decoder_Initialize(IWICBitmapDecoder *iface, IStream *stream, WICDecodeOptions options)
{
    struct wmp_decoder *This = impl_from_IWICBitmapDecoder(iface);
    LARGE_INTEGER seek;
    HRESULT hr = E_FAIL;

    TRACE("iface %p, stream %p, options %u.\n", iface, stream, options);

    EnterCriticalSection(&This->lock);

    seek.QuadPart = 0;
    IStream_Seek(stream, seek, STREAM_SEEK_SET, NULL);
    IStream_AddRef(stream);
    This->stream = stream;

    if (!pPKImageDecode_Create_WMP(&This->decoder) && !This->decoder->Initialize(This->decoder, &This->wmp_stream))
        hr = S_OK;

    LeaveCriticalSection(&This->lock);
    return hr;
}

static HRESULT WINAPI wmp_decoder_GetContainerFormat(IWICBitmapDecoder *iface, GUID *format)
{
    memcpy(format, &GUID_ContainerFormatWmp, sizeof(GUID));
    return S_OK;
}

static HRESULT WINAPI wmp_decoder_GetDecoderInfo(IWICBitmapDecoder *iface, IWICBitmapDecoderInfo **info)
{
    FIXME("iface %p, info %p, stub!\n", iface, info);
    return E_NOTIMPL;
}

static HRESULT WINAPI wmp_decoder_CopyPalette(IWICBitmapDecoder *iface, IWICPalette *palette)
{
    TRACE("iface %p, palette %p.\n", iface, palette);
    return WINCODEC_ERR_PALETTEUNAVAILABLE;
}

static HRESULT WINAPI wmp_decoder_GetMetadataQueryReader(IWICBitmapDecoder *iface, IWICMetadataQueryReader **reader)
{
    FIXME("iface %p, reader %p, stub!\n", iface, reader);
    if (!reader) return E_INVALIDARG;
    *reader = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI wmp_decoder_GetPreview(IWICBitmapDecoder *iface, IWICBitmapSource **source)
{
    FIXME("iface %p, source %p, stub!\n", iface, source);
    return E_NOTIMPL;
}

static HRESULT WINAPI wmp_decoder_GetColorContexts(IWICBitmapDecoder *iface, UINT maxcount,
                                                   IWICColorContext **contexts, UINT *count)
{
    FIXME("iface %p, maxcount %u, contexts %p, count %p, stub!\n", iface, maxcount, contexts, count);
    return E_NOTIMPL;
}

static HRESULT WINAPI wmp_decoder_GetThumbnail(IWICBitmapDecoder *iface, IWICBitmapSource **thumbnail)
{
    FIXME("iface %p, thumbnail %p, stub!\n", iface, thumbnail);
    return E_NOTIMPL;
}

static HRESULT WINAPI wmp_decoder_GetFrameCount(IWICBitmapDecoder *iface, UINT *count)
{
    struct wmp_decoder *This = impl_from_IWICBitmapDecoder(iface);
    HRESULT hr = E_FAIL;

    TRACE("iface %p, count %p.\n", iface, count);
    if (!count) return E_INVALIDARG;

    EnterCriticalSection(&This->lock);
    if (!This->decoder->GetFrameCount(This->decoder, count)) hr = S_OK;
    LeaveCriticalSection(&This->lock);

    if (SUCCEEDED(hr) && *count > 1) FIXME("unsupported frame count %u.\n", *count);
    *count = 1;
    return hr;
}

static HRESULT WINAPI wmp_decoder_GetFrame(IWICBitmapDecoder *iface, UINT index, IWICBitmapFrameDecode **frame)
{
    struct wmp_decoder *This = impl_from_IWICBitmapDecoder(iface);
    TRACE("iface %p, index %u, frame %p.\n", iface, index, frame);

    if (index != 0) return E_INVALIDARG;

    IWICBitmapDecoder_AddRef(iface);
    *frame = &This->IWICBitmapFrameDecode_iface;
    return S_OK;
}

static const IWICBitmapDecoderVtbl wmp_decoder_vtbl =
{
    wmp_decoder_QueryInterface,
    wmp_decoder_AddRef,
    wmp_decoder_Release,
    wmp_decoder_QueryCapability,
    wmp_decoder_Initialize,
    wmp_decoder_GetContainerFormat,
    wmp_decoder_GetDecoderInfo,
    wmp_decoder_CopyPalette,
    wmp_decoder_GetMetadataQueryReader,
    wmp_decoder_GetPreview,
    wmp_decoder_GetColorContexts,
    wmp_decoder_GetThumbnail,
    wmp_decoder_GetFrameCount,
    wmp_decoder_GetFrame
};

static HRESULT WINAPI wmp_decoder_frame_QueryInterface(IWICBitmapFrameDecode *iface, REFIID iid, void **out)
{
    struct wmp_decoder *This = impl_from_IWICBitmapFrameDecode(iface);

    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (!out) return E_INVALIDARG;

    *out = NULL;
    if (!IsEqualIID(&IID_IUnknown, iid) && !IsEqualIID(&IID_IWICBitmapSource, iid) &&
        !IsEqualIID(&IID_IWICBitmapFrameDecode, iid))
        return E_NOINTERFACE;

    *out = &This->IWICBitmapFrameDecode_iface;
    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}

static ULONG WINAPI wmp_decoder_frame_AddRef(IWICBitmapFrameDecode *iface)
{
    struct wmp_decoder *This = impl_from_IWICBitmapFrameDecode(iface);
    return IWICBitmapDecoder_AddRef(&This->IWICBitmapDecoder_iface);
}

static ULONG WINAPI wmp_decoder_frame_Release(IWICBitmapFrameDecode *iface)
{
    struct wmp_decoder *This = impl_from_IWICBitmapFrameDecode(iface);
    return IWICBitmapDecoder_Release(&This->IWICBitmapDecoder_iface);
}

static HRESULT WINAPI wmp_decoder_frame_GetSize(IWICBitmapFrameDecode *iface, UINT *width, UINT *height)
{
    struct wmp_decoder *This = impl_from_IWICBitmapFrameDecode(iface);
    HRESULT hr = E_FAIL;
    INT32 w, h;

    TRACE("iface %p, width %p, height %p.\n", iface, width, height);

    EnterCriticalSection(&This->lock);
    if (!This->decoder->GetSize(This->decoder, &w, &h)) hr = S_OK;
    LeaveCriticalSection(&This->lock);

    if (SUCCEEDED(hr))
    {
        *width = w;
        *height = h;
    }

    return hr;
}

static HRESULT WINAPI wmp_decoder_frame_GetPixelFormat(IWICBitmapFrameDecode *iface, WICPixelFormatGUID *format)
{
    struct wmp_decoder *This = impl_from_IWICBitmapFrameDecode(iface);
    HRESULT hr = E_FAIL;

    TRACE("iface %p, format %p.\n", iface, format);

    EnterCriticalSection(&This->lock);
    if (!This->decoder->GetPixelFormat(This->decoder, format)) hr = S_OK;
    LeaveCriticalSection(&This->lock);

    return hr;
}

static HRESULT WINAPI wmp_decoder_frame_GetResolution(IWICBitmapFrameDecode *iface, double *dpix, double *dpiy)
{
    struct wmp_decoder *This = impl_from_IWICBitmapFrameDecode(iface);
    HRESULT hr = E_FAIL;
    float resx, resy;

    TRACE("iface %p, dpix %p, dpiy %p.\n", iface, dpix, dpiy);

    EnterCriticalSection(&This->lock);
    if (!This->decoder->GetResolution(This->decoder, &resx, &resy)) hr = S_OK;
    LeaveCriticalSection(&This->lock);

    if (SUCCEEDED(hr))
    {
        *dpix = resx;
        *dpiy = resy;
    }

    return hr;
}

static HRESULT WINAPI wmp_decoder_frame_CopyPalette(IWICBitmapFrameDecode *iface, IWICPalette *palette)
{
    TRACE("iface %p, palette %p.\n", iface, palette);
    return WINCODEC_ERR_PALETTEUNAVAILABLE;
}

static HRESULT WINAPI wmp_decoder_frame_CopyPixels(IWICBitmapFrameDecode *iface, const WICRect *rect,
                                                   UINT stride, UINT bufsize, BYTE *buffer)
{
    struct wmp_decoder *This = impl_from_IWICBitmapFrameDecode(iface);
    WICPixelFormatGUID format;
    PKRect src_rect = {0, 0, 0, 0};
    SIZE_T y, bpp, size, width, src_stride;
    BYTE *dst, *src;
    JXR_ERR err;

    TRACE("iface %p, rect %p, stride %u, bufsize %u, buffer %p.\n", iface, debug_wic_rect(rect),
          stride, bufsize, buffer);

    EnterCriticalSection(&This->lock);
    if (This->decoder->GetPixelFormat(This->decoder, &format)) goto failed;
    if (This->decoder->GetSize(This->decoder, &src_rect.Width, &src_rect.Height)) goto failed;
    if (!(bpp = pixel_format_get_bpp(&format))) goto failed;
    src_stride = (src_rect.Width * bpp + 7) / 8;

    if (!This->image_data)
    {
        size = src_rect.Height * src_stride;
        if (!(This->image_data = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size))) goto failed;
        if ((err = This->decoder->Copy(This->decoder, &src_rect, This->image_data, src_stride)))
            goto failed;
    }

    if (rect)
    {
        /* FIXME: not sure what to do here, should we shift bits? it's unlikely to happen anyway */
        if ((bpp % 8) && (rect->X % 8))
            FIXME("unsupported copy with non-aligned format.\n");
        src_rect.X = rect->X;
        src_rect.Y = rect->Y;
        src_rect.Width = rect->Width;
        src_rect.Height = rect->Height;
    }

    dst = buffer;
    src = This->image_data;
    src += (src_rect.X * bpp) / 8;
    width = (src_rect.Width * bpp + 7) / 8;
    for (y = src_rect.Y; y < src_rect.Y + src_rect.Height; ++y)
    {
        if (dst + width > buffer + bufsize) goto failed;
        memcpy(dst, src + y * src_stride, width);
        dst += stride;
    }

    LeaveCriticalSection(&This->lock);
    return S_OK;

failed:
    LeaveCriticalSection(&This->lock);
    return E_FAIL;
}

static HRESULT WINAPI wmp_decoder_frame_GetMetadataQueryReader(IWICBitmapFrameDecode *iface,
                                                               IWICMetadataQueryReader **reader)
{
    FIXME("iface %p, reader %p, stub!\n", iface, reader);
    if (!reader) return E_INVALIDARG;
    return E_NOTIMPL;
}

static HRESULT WINAPI wmp_decoder_frame_GetColorContexts(IWICBitmapFrameDecode *iface, UINT maxcount,
                                                         IWICColorContext **contexts, UINT *count)
{
    FIXME("iface %p, maxcount %u, contexts %p, count %p, stub\n", iface, maxcount, contexts, count);
    return E_NOTIMPL;
}

static HRESULT WINAPI wmp_decoder_frame_GetThumbnail(IWICBitmapFrameDecode *iface, IWICBitmapSource **thumbnail)
{
    FIXME("iface %p, thumbnail %p, stub!\n", iface, thumbnail);
    return WINCODEC_ERR_CODECNOTHUMBNAIL;
}

static const IWICBitmapFrameDecodeVtbl wmp_decoder_frame_vtbl =
{
    wmp_decoder_frame_QueryInterface,
    wmp_decoder_frame_AddRef,
    wmp_decoder_frame_Release,
    wmp_decoder_frame_GetSize,
    wmp_decoder_frame_GetPixelFormat,
    wmp_decoder_frame_GetResolution,
    wmp_decoder_frame_CopyPalette,
    wmp_decoder_frame_CopyPixels,
    wmp_decoder_frame_GetMetadataQueryReader,
    wmp_decoder_frame_GetColorContexts,
    wmp_decoder_frame_GetThumbnail
};

static HRESULT WINAPI wmp_decoder_block_QueryInterface(IWICMetadataBlockReader *iface, REFIID iid, void **ppv)
{
    struct wmp_decoder *This = impl_from_IWICMetadataBlockReader(iface);
    return IWICBitmapFrameDecode_QueryInterface(&This->IWICBitmapFrameDecode_iface, iid, ppv);
}

static ULONG WINAPI wmp_decoder_block_AddRef(IWICMetadataBlockReader *iface)
{
    struct wmp_decoder *This = impl_from_IWICMetadataBlockReader(iface);
    return IWICBitmapDecoder_AddRef(&This->IWICBitmapDecoder_iface);
}

static ULONG WINAPI wmp_decoder_block_Release(IWICMetadataBlockReader *iface)
{
    struct wmp_decoder *This = impl_from_IWICMetadataBlockReader(iface);
    return IWICBitmapDecoder_Release(&This->IWICBitmapDecoder_iface);
}

static HRESULT WINAPI wmp_decoder_block_GetContainerFormat(IWICMetadataBlockReader *iface, GUID *format)
{
    if (!format) return E_INVALIDARG;
    memcpy(format, &GUID_ContainerFormatWmp, sizeof(GUID));
    return S_OK;
}

static HRESULT WINAPI wmp_decoder_block_GetCount(IWICMetadataBlockReader *iface, UINT *count)
{
    FIXME("iface %p, count %p, stub!\n", iface, count);
    if (!count) return E_INVALIDARG;
    *count = 0;
    return S_OK;
}

static HRESULT WINAPI wmp_decoder_block_GetReaderByIndex(IWICMetadataBlockReader *iface, UINT index,
                                                         IWICMetadataReader **reader)
{
    FIXME("iface %p, index %u, reader %p, stub!\n", iface, index, reader);
    return E_NOTIMPL;
}

static HRESULT WINAPI wmp_decoder_block_GetEnumerator(IWICMetadataBlockReader *iface, IEnumUnknown **enumerator)
{
    FIXME("iface %p, metadata %p, stub!\n", iface, enumerator);
    return E_NOTIMPL;
}

static const IWICMetadataBlockReaderVtbl wmp_decoder_block_vtbl =
{
    wmp_decoder_block_QueryInterface,
    wmp_decoder_block_AddRef,
    wmp_decoder_block_Release,
    wmp_decoder_block_GetContainerFormat,
    wmp_decoder_block_GetCount,
    wmp_decoder_block_GetReaderByIndex,
    wmp_decoder_block_GetEnumerator,
};

static HRESULT wmp_decoder_create(IUnknown *outer, IUnknown **out)
{
    struct wmp_decoder *This;

    TRACE("outer %p, out %p.\n", outer, out);

    *out = NULL;
    This = HeapAlloc(GetProcessHeap(), 0, sizeof(struct wmp_decoder));
    if (!This) return E_OUTOFMEMORY;

    This->IWICBitmapDecoder_iface.lpVtbl = &wmp_decoder_vtbl;
    This->IWICBitmapFrameDecode_iface.lpVtbl = &wmp_decoder_frame_vtbl;
    This->IWICMetadataBlockReader_iface.lpVtbl = &wmp_decoder_block_vtbl;
    This->wmp_stream.Close = wmp_stream_Close;
    This->wmp_stream.EOS = wmp_stream_EOS;
    This->wmp_stream.Read = wmp_stream_Read;
    This->wmp_stream.Write = wmp_stream_Write;
    This->wmp_stream.SetPos = wmp_stream_SetPos;
    This->wmp_stream.GetPos = wmp_stream_GetPos;
    This->ref = 1;
    This->stream = NULL;
    This->decoder = NULL;
    This->image_data = NULL;
    InitializeCriticalSection(&This->lock);
    This->lock.DebugInfo->Spare[0] = (DWORD_PTR)(__FILE__ ": wmp_decoder.lock");

    *out = (IUnknown *)&This->IWICBitmapDecoder_iface;
    return S_OK;
}

#else /* !defined(SONAME_LIBJXRGLUE) */

static HRESULT wmp_decoder_create(IUnknown *outer, IUnknown **out)
{
    ERR("JPEG-XR support not compiled in!\n");
    return E_NOTIMPL;
}

#endif

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
    FIXME("%s not implemented.\n", debugstr_guid(iid));
    return E_NOINTERFACE;
}

static ULONG WINAPI class_factory_AddRef(IClassFactory *iface) { return 2; }

static ULONG WINAPI class_factory_Release(IClassFactory *iface) { return 1; }

static HRESULT WINAPI class_factory_CreateInstance(IClassFactory *iface, IUnknown *outer, REFIID iid, void **out)
{
    struct class_factory *factory = impl_from_IClassFactory(iface);
    IUnknown *unk;
    HRESULT hr;

    TRACE("iface %p, outer %p, iid %s, out %p.\n", iface, outer, debugstr_guid(iid), out);

    if (outer && !IsEqualGUID(iid, &IID_IUnknown)) return E_NOINTERFACE;

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
    FIXME("iface %p, lock %d, stub!\n", iface, lock);
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

static struct class_factory wmp_decoder_cf = {{&class_factory_vtbl}, wmp_decoder_create};

static HINSTANCE WMPHOTO_hInstance;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        WMPHOTO_hInstance = hinstDLL;
        DisableThreadLibraryCalls(hinstDLL);
#ifdef SONAME_LIBJXRGLUE
        if (!(libjxrglue = dlopen(SONAME_LIBJXRGLUE, RTLD_NOW)))
        {
            ERR("unable to load %s!\n", SONAME_LIBJXRGLUE);
            return FALSE;
        }

        if (!(pPKImageDecode_Create_WMP = dlsym(libjxrglue, "PKImageDecode_Create_WMP")))
        {
            ERR("unable to find PKImageDecode_Create_WMP in %s!\n", SONAME_LIBJXRGLUE);
            return FALSE;
        }
#endif
        break;
    case DLL_WINE_PREATTACH:
        return FALSE; /* prefer native version */
    case DLL_PROCESS_DETACH:
#ifdef SONAME_LIBJXRGLUE
        dlclose(libjxrglue);
#endif
        return TRUE;
    }

    return TRUE;
}

HRESULT WINAPI DllCanUnloadNow(void)
{
    return S_FALSE;
}

HRESULT WINAPI DllGetClassObject(REFCLSID clsid, REFIID iid, LPVOID *out)
{
    struct class_factory *factory;

    TRACE("clsid %s, iid %s, out %p.\n", debugstr_guid(clsid), debugstr_guid(iid), out);

    if (IsEqualGUID(clsid, &CLSID_WICWmpDecoder)) factory = &wmp_decoder_cf;
    else
    {
        FIXME("%s not implemented.\n", debugstr_guid(clsid));
        return CLASS_E_CLASSNOTAVAILABLE;
    }

    return IClassFactory_QueryInterface(&factory->IClassFactory_iface, iid, out);
}

HRESULT WINAPI DllRegisterServer(void)
{
    return __wine_register_resources( WMPHOTO_hInstance );
}

HRESULT WINAPI DllUnregisterServer(void)
{
    return __wine_unregister_resources( WMPHOTO_hInstance );
}
