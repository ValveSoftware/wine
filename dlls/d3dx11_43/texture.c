/*
 * Copyright 2016 Andrey Gusev
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

#include "wine/debug.h"

#define COBJMACROS

#include "initguid.h"
#include "d3d11.h"
#include "d3dx11.h"
#include "d3dcompiler.h"
#include "dxhelpers.h"

WINE_DEFAULT_DEBUG_CHANNEL(d3dx);

HRESULT WINAPI D3DX11SaveTextureToFileW(ID3D11DeviceContext *context, ID3D11Resource *texture,
        D3DX11_IMAGE_FILE_FORMAT format, const WCHAR *filename)
{
    FIXME("context %p, texture %p, format %u, filename %s stub!\n",
            context, texture, format, debugstr_w(filename));

    return E_NOTIMPL;
}

HRESULT WINAPI D3DX11SaveTextureToFileA(ID3D11DeviceContext *context, ID3D11Resource *texture,
        D3DX11_IMAGE_FILE_FORMAT format, const char *filename)
{
    FIXME("context %p, texture %p, format %u, filename %s stub!\n",
            context, texture, format, debugstr_a(filename));

    return E_NOTIMPL;
}

HRESULT WINAPI D3DX11SaveTextureToMemory(ID3D11DeviceContext *context, ID3D11Resource *texture,
        D3DX11_IMAGE_FILE_FORMAT format, ID3D10Blob **buffer, UINT flags)
{
    FIXME("context %p, texture %p, format %u, buffer %p, flags %#x stub!\n",
            context, texture, format, buffer, flags);

    return E_NOTIMPL;
}

HRESULT WINAPI D3DX11GetImageInfoFromFileA(const char *src_file, ID3DX11ThreadPump *pump, D3DX11_IMAGE_INFO *info,
        HRESULT *result)
{
    WCHAR *buffer;
    int str_len;
    HRESULT hr;

    TRACE("src_file %s, pump %p, info %p, result %p.\n", debugstr_a(src_file), pump, info, result);

    if (!src_file)
        return E_FAIL;

    str_len = MultiByteToWideChar(CP_ACP, 0, src_file, -1, NULL, 0);
    if (!str_len)
        return HRESULT_FROM_WIN32(GetLastError());

    buffer = malloc(str_len * sizeof(*buffer));
    if (!buffer)
        return E_OUTOFMEMORY;

    MultiByteToWideChar(CP_ACP, 0, src_file, -1, buffer, str_len);
    hr = D3DX11GetImageInfoFromFileW(buffer, pump, info, result);

    free(buffer);

    return hr;
}

HRESULT WINAPI D3DX11GetImageInfoFromFileW(const WCHAR *src_file, ID3DX11ThreadPump *pump, D3DX11_IMAGE_INFO *info,
        HRESULT *result)
{
    void *buffer = NULL;
    DWORD size = 0;
    HRESULT hr;

    TRACE("src_file %s, pump %p, info %p, result %p.\n", debugstr_w(src_file), pump, info, result);

    if (!src_file)
        return E_FAIL;

    if (pump)
    {
        ID3DX11DataProcessor *processor;
        ID3DX11DataLoader *loader;

        if (FAILED((hr = D3DX11CreateAsyncFileLoaderW(src_file, &loader))))
            return hr;
        if (FAILED((hr = D3DX11CreateAsyncTextureInfoProcessor(info, &processor))))
        {
            ID3DX11DataLoader_Destroy(loader);
            return hr;
        }
        hr = ID3DX11ThreadPump_AddWorkItem(pump, loader, processor, result, NULL);
        if (FAILED(hr))
        {
            ID3DX11DataLoader_Destroy(loader);
            ID3DX11DataProcessor_Destroy(processor);
        }
        return hr;
    }

    if (SUCCEEDED((hr = load_file(src_file, &buffer, &size))))
    {
        hr = get_image_info(buffer, size, info);
        free(buffer);
    }
    if (result)
        *result = hr;
    return hr;
}

HRESULT WINAPI D3DX11GetImageInfoFromResourceA(HMODULE module, const char *resource, ID3DX11ThreadPump *pump,
        D3DX11_IMAGE_INFO *info, HRESULT *result)
{
    void *buffer;
    HRESULT hr;
    DWORD size;

    TRACE("module %p, resource %s, pump %p, info %p, result %p.\n",
            module, debugstr_a(resource), pump, info, result);

    if (pump)
    {
        ID3DX11DataProcessor *processor;
        ID3DX11DataLoader *loader;

        if (FAILED((hr = D3DX11CreateAsyncResourceLoaderA(module, resource, &loader))))
            return hr;
        if (FAILED((hr = D3DX11CreateAsyncTextureInfoProcessor(info, &processor))))
        {
            ID3DX11DataLoader_Destroy(loader);
            return hr;
        }
        if (FAILED((hr = ID3DX11ThreadPump_AddWorkItem(pump, loader, processor, result, NULL))))
        {
            ID3DX11DataLoader_Destroy(loader);
            ID3DX11DataProcessor_Destroy(processor);
        }
        return hr;
    }

    if (FAILED((hr = load_resourceA(module, resource, &buffer, &size))))
        return hr;
    hr = get_image_info(buffer, size, info);
    if (result)
        *result = hr;
    return hr;
}

HRESULT WINAPI D3DX11GetImageInfoFromResourceW(HMODULE module, const WCHAR *resource, ID3DX11ThreadPump *pump,
        D3DX11_IMAGE_INFO *info, HRESULT *result)
{
    void *buffer;
    HRESULT hr;
    DWORD size;

    TRACE("module %p, resource %s, pump %p, info %p, result %p.\n",
            module, debugstr_w(resource), pump, info, result);

    if (pump)
    {
        ID3DX11DataProcessor *processor;
        ID3DX11DataLoader *loader;

        if (FAILED((hr = D3DX11CreateAsyncResourceLoaderW(module, resource, &loader))))
            return hr;
        if (FAILED((hr = D3DX11CreateAsyncTextureInfoProcessor(info, &processor))))
        {
            ID3DX11DataLoader_Destroy(loader);
            return hr;
        }
        if (FAILED((hr = ID3DX11ThreadPump_AddWorkItem(pump, loader, processor, result, NULL))))
        {
            ID3DX11DataLoader_Destroy(loader);
            ID3DX11DataProcessor_Destroy(processor);
        }
        return hr;
    }

    if (FAILED((hr = load_resourceW(module, resource, &buffer, &size))))
        return hr;
    hr = get_image_info(buffer, size, info);
    if (result)
        *result = hr;
    return hr;
}

static HRESULT d3dx11_image_info_from_d3dx_image(D3DX11_IMAGE_INFO *info, struct d3dx_image *image)
{
    DXGI_FORMAT format;
    HRESULT hr = S_OK;

    memset(info, 0, sizeof(*info));
    if (image->image_file_format == D3DX_IMAGE_FILE_FORMAT_DDS)
        format = dxgi_format_from_dds_d3dx_pixel_format_id(image->format);
    else if (image->image_file_format == D3DX_IMAGE_FILE_FORMAT_DDS_DXT10)
        format = dxgi_format_from_d3dx_pixel_format_id(image->format);
    else
        format = DXGI_FORMAT_R8G8B8A8_UNORM;

    if (format == DXGI_FORMAT_UNKNOWN)
    {
        WARN("Tried to load DDS file with unsupported format %#x.\n", image->format);
        return E_FAIL;
    }

    if (image->image_file_format == D3DX_IMAGE_FILE_FORMAT_DDS_DXT10)
        info->ImageFileFormat = D3DX11_IFF_DDS;
    else
        info->ImageFileFormat = (D3DX11_IMAGE_FILE_FORMAT)image->image_file_format;
    if (info->ImageFileFormat == D3DX11_IFF_FORCE_DWORD)
    {
        ERR("Unsupported d3dx image file.\n");
        return E_FAIL;
    }

    info->Width = image->size.width;
    info->Height = image->size.height;
    info->Depth = image->size.depth;
    info->ArraySize = image->layer_count;
    info->MipLevels = image->mip_levels;
    info->Format = format;
    switch (image->resource_type)
    {
    case D3DX_RESOURCE_TYPE_TEXTURE_1D:
        info->ResourceDimension = D3D11_RESOURCE_DIMENSION_TEXTURE1D;
        break;

    case D3DX_RESOURCE_TYPE_TEXTURE_2D:
        info->ResourceDimension = D3D11_RESOURCE_DIMENSION_TEXTURE2D;
        break;

    case D3DX_RESOURCE_TYPE_CUBE_TEXTURE:
        info->ResourceDimension = D3D11_RESOURCE_DIMENSION_TEXTURE2D;
        info->MiscFlags |= D3D11_RESOURCE_MISC_TEXTURECUBE;
        break;

    case D3DX_RESOURCE_TYPE_TEXTURE_3D:
        info->ResourceDimension = D3D11_RESOURCE_DIMENSION_TEXTURE3D;
        break;

    default:
        ERR("Unhandled resource type %d.\n", image->resource_type);
        hr = E_FAIL;
        break;
    }

    return hr;
}

HRESULT get_image_info(const void *data, SIZE_T size, D3DX11_IMAGE_INFO *img_info)
{
    struct d3dx_image image;
    HRESULT hr;

    if (!data || !size)
        return E_FAIL;

    hr = d3dx_image_init(data, size, &image, 0, D3DX_IMAGE_INFO_ONLY | D3DX_IMAGE_SUPPORT_DXT10);
    if (SUCCEEDED(hr))
        hr = d3dx11_image_info_from_d3dx_image(img_info, &image);

    if (FAILED(hr))
    {
        WARN("Invalid or unsupported image file, hr %#lx.\n", hr);
        return E_FAIL;
    }

    return S_OK;
}

HRESULT WINAPI D3DX11GetImageInfoFromMemory(const void *src_data, SIZE_T src_data_size, ID3DX11ThreadPump *pump,
        D3DX11_IMAGE_INFO *img_info, HRESULT *hresult)
{
    HRESULT hr;

    TRACE("src_data %p, src_data_size %Iu, pump %p, img_info %p, hresult %p.\n",
            src_data, src_data_size, pump, img_info, hresult);

    if (!src_data)
        return E_FAIL;

    if (pump)
    {
        ID3DX11DataProcessor *processor;
        ID3DX11DataLoader *loader;

        if (FAILED((hr = D3DX11CreateAsyncMemoryLoader(src_data, src_data_size, &loader))))
            return hr;
        if (FAILED((hr = D3DX11CreateAsyncTextureInfoProcessor(img_info, &processor))))
        {
            ID3DX11DataLoader_Destroy(loader);
            return hr;
        }
        if (FAILED((hr = ID3DX11ThreadPump_AddWorkItem(pump, loader, processor, hresult, NULL))))
        {
            ID3DX11DataLoader_Destroy(loader);
            ID3DX11DataProcessor_Destroy(processor);
        }
        return hr;
    }

    hr = get_image_info(src_data, src_data_size, img_info);
    if (hresult)
        *hresult = hr;
    return hr;
}

void init_load_info(const D3DX11_IMAGE_LOAD_INFO *load_info, D3DX11_IMAGE_LOAD_INFO *out)
{
    if (load_info)
    {
        *out = *load_info;
        return;
    }

    out->Width = D3DX11_DEFAULT;
    out->Height = D3DX11_DEFAULT;
    out->Depth = D3DX11_DEFAULT;
    out->FirstMipLevel = D3DX11_DEFAULT;
    out->MipLevels = D3DX11_DEFAULT;
    out->Usage = D3DX11_DEFAULT;
    out->BindFlags = D3DX11_DEFAULT;
    out->CpuAccessFlags = D3DX11_DEFAULT;
    out->MiscFlags = D3DX11_DEFAULT;
    out->Format = D3DX11_DEFAULT;
    out->Filter = D3DX11_DEFAULT;
    out->MipFilter = D3DX11_DEFAULT;
    out->pSrcInfo = NULL;
}

#define D3DX_FILTER_INVALID_BITS 0xff80fff8
static inline HRESULT d3dx_validate_filter(uint32_t filter)
{
    if ((filter & D3DX_FILTER_INVALID_BITS) || !(filter & 0x7) || ((filter & 0x7) > D3DX11_FILTER_BOX))
        return D3DERR_INVALIDCALL;

    return S_OK;
}

static HRESULT d3dx_handle_filter(uint32_t *filter)
{
    if (*filter == D3DX11_DEFAULT || !(*filter))
        *filter = D3DX11_FILTER_TRIANGLE | D3DX11_FILTER_DITHER;

    return d3dx_validate_filter(*filter);
}

static HRESULT d3dx_create_subresource_data_for_texture(uint32_t width, uint32_t height, uint32_t depth,
        uint32_t mip_levels, uint32_t layer_count, const struct pixel_format_desc *fmt_desc,
        D3D11_SUBRESOURCE_DATA **out_sub_rsrc_data, uint8_t **pixel_data)
{
    uint8_t *sub_rsrc_data = NULL, *pixels_ptr;
    uint32_t i, j, pixels_size, pixels_offset;
    D3D11_SUBRESOURCE_DATA *sub_rsrcs = NULL;
    HRESULT hr = S_OK;

    *pixel_data = NULL;
    *out_sub_rsrc_data = NULL;

    pixels_offset = (sizeof(*sub_rsrcs) * mip_levels * layer_count);
    pixels_size = d3dx_calculate_layer_pixels_size(fmt_desc->format, width, height, depth, mip_levels) * layer_count;
    if (!(sub_rsrc_data = malloc(pixels_size + pixels_offset)))
        return E_FAIL;

    sub_rsrcs = (D3D11_SUBRESOURCE_DATA *)sub_rsrc_data;
    pixels_ptr = sub_rsrc_data + pixels_offset;
    for (i = 0; i < layer_count; ++i)
    {
        struct volume size = { width, height, depth };

        for (j = 0; j < mip_levels; ++j)
        {
            uint32_t row_pitch, slice_pitch;

            hr = d3dx_calculate_pixels_size(fmt_desc->format, size.width, size.height, &row_pitch, &slice_pitch);
            if (FAILED(hr))
                break;

            sub_rsrcs[i * mip_levels + j].pSysMem = pixels_ptr;
            sub_rsrcs[i * mip_levels + j].SysMemPitch = row_pitch;
            sub_rsrcs[i * mip_levels + j].SysMemSlicePitch = slice_pitch;

            pixels_ptr += slice_pitch * size.depth;
            d3dx_get_next_mip_level_size(&size);
        }
    }

    if (SUCCEEDED(hr))
    {
        *pixel_data = sub_rsrc_data + pixels_offset;
        *out_sub_rsrc_data = sub_rsrcs;
        sub_rsrc_data = NULL;
    }

    free(sub_rsrc_data);
    return hr;
}

HRESULT load_texture_data(const void *data, SIZE_T size, D3DX11_IMAGE_LOAD_INFO *load_info,
        D3D11_SUBRESOURCE_DATA **resource_data)
{
    const struct pixel_format_desc *fmt_desc, *src_desc;
    uint32_t i, j, loaded_mip_levels, max_mip_levels;
    D3D11_SUBRESOURCE_DATA *sub_rsrcs = NULL;
    D3DX11_IMAGE_INFO img_info;
    struct d3dx_image image;
    uint8_t *pixels_ptr;
    HRESULT hr;

    if (!data || !size)
        return E_FAIL;

    if (FAILED(hr = d3dx_handle_filter(&load_info->Filter)))
    {
        FIXME("Invalid filter argument.\n");
        return hr;
    }

    hr = d3dx_image_init(data, size, &image, 0, D3DX_IMAGE_SUPPORT_DXT10);
    if (FAILED(hr))
        return E_FAIL;

    hr = d3dx11_image_info_from_d3dx_image(&img_info, &image);
    if (FAILED(hr))
    {
        WARN("Invalid or unsupported image file, hr %#lx.\n", hr);
        hr = E_FAIL;
        goto end;
    }

    if ((!(img_info.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE) || img_info.ArraySize != 6)
            && img_info.ArraySize != 1)
    {
        FIXME("img_info.ArraySize = %u not supported.\n", img_info.ArraySize);
        hr = E_NOTIMPL;
        goto end;
    }

    if (load_info->FirstMipLevel == D3DX11_DEFAULT || (load_info->FirstMipLevel >= img_info.MipLevels))
        load_info->FirstMipLevel = 0;
    if (load_info->Format == D3DX11_DEFAULT || load_info->Format == DXGI_FORMAT_FROM_FILE)
        load_info->Format = img_info.Format;
    fmt_desc = get_d3dx_pixel_format_info(d3dx_pixel_format_id_from_dxgi_format(load_info->Format));
    if (fmt_desc->format == D3DX_PIXEL_FORMAT_COUNT)
    {
        FIXME("Unknown DXGI format supplied, %#x.\n", load_info->Format);
        hr = E_NOTIMPL;
        goto end;
    }

    /* Potentially round up width/height to align with block size. */
    if (!load_info->Width || load_info->Width == D3DX11_FROM_FILE || load_info->Width == D3DX11_DEFAULT)
        load_info->Width = (img_info.Width + fmt_desc->block_width - 1) & ~(fmt_desc->block_width - 1);
    if (!load_info->Height || load_info->Height == D3DX11_FROM_FILE || load_info->Height == D3DX11_DEFAULT)
        load_info->Height = (img_info.Height + fmt_desc->block_height - 1) & ~(fmt_desc->block_height - 1);

    if (!load_info->Depth || load_info->Depth == D3DX11_FROM_FILE || load_info->Depth == D3DX11_DEFAULT)
        load_info->Depth = img_info.Depth;
    if ((load_info->Depth > 1) && (img_info.ResourceDimension != D3D11_RESOURCE_DIMENSION_TEXTURE3D))
    {
        hr = E_FAIL;
        goto end;
    }

    max_mip_levels = d3dx_get_max_mip_levels_for_size(load_info->Width, load_info->Height, load_info->Depth);
    if (!load_info->MipLevels || load_info->MipLevels == D3DX11_DEFAULT || load_info->MipLevels == D3DX11_FROM_FILE)
        load_info->MipLevels = (load_info->MipLevels == D3DX11_FROM_FILE) ? img_info.MipLevels : max_mip_levels;
    load_info->MipLevels = min(max_mip_levels, load_info->MipLevels);

    hr = d3dx_create_subresource_data_for_texture(load_info->Width, load_info->Height, load_info->Depth,
            load_info->MipLevels, img_info.ArraySize, fmt_desc, &sub_rsrcs, &pixels_ptr);
    if (FAILED(hr))
        goto end;

    src_desc = get_d3dx_pixel_format_info(image.format);
    loaded_mip_levels = min((img_info.MipLevels - load_info->FirstMipLevel), load_info->MipLevels);
    for (i = 0; i < img_info.ArraySize; ++i)
    {
        struct volume dst_size = { load_info->Width, load_info->Height, load_info->Depth };

        for (j = 0; j < loaded_mip_levels; ++j)
        {
            D3D11_SUBRESOURCE_DATA *sub_rsrc = &sub_rsrcs[i * load_info->MipLevels + j];
            const RECT unaligned_rect = { 0, 0, dst_size.width, dst_size.height };
            struct d3dx_pixels src_pixels, dst_pixels;

            hr = d3dx_image_get_pixels(&image, i, j + load_info->FirstMipLevel, &src_pixels);
            if (FAILED(hr))
                goto end;

            set_d3dx_pixels(&dst_pixels, sub_rsrc->pSysMem, sub_rsrc->SysMemPitch, sub_rsrc->SysMemSlicePitch, NULL,
                    dst_size.width, dst_size.height, dst_size.depth, &unaligned_rect);

            hr = d3dx_load_pixels_from_pixels(&dst_pixels, fmt_desc, &src_pixels, src_desc, load_info->Filter, 0);
            if (FAILED(hr))
                goto end;

            d3dx_get_next_mip_level_size(&dst_size);
        }
    }

    if (loaded_mip_levels < load_info->MipLevels)
    {
        struct volume base_level_size = { load_info->Width, load_info->Height, load_info->Depth };

        if (FAILED(hr = d3dx_handle_filter(&load_info->MipFilter)))
        {
            FIXME("Invalid mip filter argument.\n");
            goto end;
        }

        d3dx_get_mip_level_size(&base_level_size, loaded_mip_levels - 1);
        for (i = 0; i < img_info.ArraySize; ++i)
        {
            struct volume src_size, dst_size;

            src_size = dst_size = base_level_size;
            for (j = (loaded_mip_levels - 1); j < (load_info->MipLevels - 1); ++j)
            {
                D3D11_SUBRESOURCE_DATA *dst_data = &sub_rsrcs[i * load_info->MipLevels + j + 1];
                D3D11_SUBRESOURCE_DATA *src_data = &sub_rsrcs[i * load_info->MipLevels + j];
                const RECT src_unaligned_rect = { 0, 0, src_size.width, src_size.height };
                struct d3dx_pixels src_pixels, dst_pixels;
                RECT dst_unaligned_rect;

                d3dx_get_next_mip_level_size(&dst_size);
                SetRect(&dst_unaligned_rect, 0, 0, dst_size.width, dst_size.height);
                set_d3dx_pixels(&dst_pixels, dst_data->pSysMem, dst_data->SysMemPitch, dst_data->SysMemSlicePitch, NULL,
                        dst_size.width, dst_size.height, dst_size.depth, &dst_unaligned_rect);
                set_d3dx_pixels(&src_pixels, src_data->pSysMem, src_data->SysMemPitch, src_data->SysMemSlicePitch, NULL,
                        src_size.width, src_size.height, src_size.depth, &src_unaligned_rect);

                hr = d3dx_load_pixels_from_pixels(&dst_pixels, fmt_desc, &src_pixels, fmt_desc, load_info->MipFilter, 0);
                if (FAILED(hr))
                    goto end;

                src_size = dst_size;
            }
        }
    }

    if (load_info->pSrcInfo)
       *load_info->pSrcInfo = img_info;
    load_info->Usage = (load_info->Usage == D3DX11_DEFAULT) ? D3D11_USAGE_DEFAULT : load_info->Usage;
    load_info->BindFlags = (load_info->BindFlags == D3DX11_DEFAULT) ? D3D11_BIND_SHADER_RESOURCE : load_info->BindFlags;
    load_info->CpuAccessFlags = (load_info->CpuAccessFlags == D3DX11_DEFAULT) ? 0 : load_info->CpuAccessFlags;
    load_info->MiscFlags = (load_info->MiscFlags == D3DX11_DEFAULT) ? 0 : load_info->MiscFlags;
    load_info->MiscFlags |= img_info.MiscFlags;
    *resource_data = sub_rsrcs;
    sub_rsrcs = NULL;

end:
    d3dx_image_cleanup(&image);
    free(sub_rsrcs);
    return hr;
}

HRESULT create_d3d_texture(ID3D11Device *device, D3DX11_IMAGE_LOAD_INFO *load_info,
        D3D11_SUBRESOURCE_DATA *resource_data, ID3D11Resource **texture)
{
    HRESULT hr;

    *texture = NULL;
    switch (load_info->pSrcInfo->ResourceDimension)
    {
    case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
    {
        D3D11_TEXTURE2D_DESC texture_2d_desc = { 0 };
        ID3D11Texture2D *texture_2d;

        texture_2d_desc.Width = load_info->Width;
        texture_2d_desc.Height = load_info->Height;
        texture_2d_desc.MipLevels = load_info->MipLevels;
        texture_2d_desc.ArraySize = load_info->pSrcInfo->ArraySize;
        texture_2d_desc.Format = load_info->Format;
        texture_2d_desc.SampleDesc.Count = 1;
        texture_2d_desc.Usage = load_info->Usage;
        texture_2d_desc.BindFlags = load_info->BindFlags;
        texture_2d_desc.CPUAccessFlags = load_info->CpuAccessFlags;
        texture_2d_desc.MiscFlags = load_info->MiscFlags;

        if (FAILED(hr = ID3D11Device_CreateTexture2D(device, &texture_2d_desc, resource_data, &texture_2d)))
            return hr;
        *texture = (ID3D11Resource *)texture_2d;
        break;
    }

    case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
    {
        D3D11_TEXTURE3D_DESC texture_3d_desc = { 0 };
        ID3D11Texture3D *texture_3d;

        texture_3d_desc.Width = load_info->Width;
        texture_3d_desc.Height = load_info->Height;
        texture_3d_desc.Depth = load_info->Depth;
        texture_3d_desc.MipLevels = load_info->MipLevels;
        texture_3d_desc.Format = load_info->Format;
        texture_3d_desc.Usage = load_info->Usage;
        texture_3d_desc.BindFlags = load_info->BindFlags;
        texture_3d_desc.CPUAccessFlags = load_info->CpuAccessFlags;
        texture_3d_desc.MiscFlags = load_info->MiscFlags;

        if (FAILED(hr = ID3D11Device_CreateTexture3D(device, &texture_3d_desc, resource_data, &texture_3d)))
            return hr;
        *texture = (ID3D11Resource *)texture_3d;
        break;
    }

    default:
        FIXME("Unhandled resource dimension %d.\n", load_info->pSrcInfo->ResourceDimension);
        return E_NOTIMPL;
    }

    return S_OK;
}

static HRESULT create_texture(ID3D11Device *device, const void *data, SIZE_T size,
        D3DX11_IMAGE_LOAD_INFO *load_info, ID3D11Resource **texture)
{
    D3D11_SUBRESOURCE_DATA *resource_data;
    D3DX11_IMAGE_LOAD_INFO load_info_copy;
    D3DX11_IMAGE_INFO img_info;
    HRESULT hr;

    init_load_info(load_info, &load_info_copy);
    if (load_info_copy.pSrcInfo == NULL)
        load_info_copy.pSrcInfo = &img_info;

    if (FAILED((hr = load_texture_data(data, size, &load_info_copy, &resource_data))))
        return hr;
    hr = create_d3d_texture(device, &load_info_copy, resource_data, texture);
    free(resource_data);
    return hr;
}

HRESULT WINAPI D3DX11CreateTextureFromMemory(ID3D11Device *device, const void *src_data, SIZE_T src_data_size,
        D3DX11_IMAGE_LOAD_INFO *load_info, ID3DX11ThreadPump *pump, ID3D11Resource **texture, HRESULT *hresult)
{
    HRESULT hr;

    TRACE("device %p, src_data %p, src_data_size %Iu, load_info %p, pump %p, texture %p, hresult %p.\n",
            device, src_data, src_data_size, load_info, pump, texture, hresult);

    if (!device)
        return E_INVALIDARG;
    if (!src_data)
        return E_FAIL;

    if (pump)
    {
        ID3DX11DataProcessor *processor;
        ID3DX11DataLoader *loader;

        if (FAILED((hr = D3DX11CreateAsyncMemoryLoader(src_data, src_data_size, &loader))))
            return hr;
        if (FAILED((hr = D3DX11CreateAsyncTextureProcessor(device, load_info, &processor))))
        {
            ID3DX11DataLoader_Destroy(loader);
            return hr;
        }
        if (FAILED((hr = ID3DX11ThreadPump_AddWorkItem(pump, loader, processor, hresult, (void **)texture))))
        {
            ID3DX11DataLoader_Destroy(loader);
            ID3DX11DataProcessor_Destroy(processor);
        }
        return hr;
    }

    hr = create_texture(device, src_data, src_data_size, load_info, texture);
    if (hresult)
        *hresult = hr;
    return hr;
}

HRESULT WINAPI D3DX11CreateTextureFromFileA(ID3D11Device *device, const char *src_file,
        D3DX11_IMAGE_LOAD_INFO *load_info, ID3DX11ThreadPump *pump, ID3D11Resource **texture,
        HRESULT *hresult)
{
    int32_t str_len;
    WCHAR *buffer;
    HRESULT hr;

    TRACE("device %p, src_file %s, load_info %p, pump %p, texture %p, hresult %p.\n",
            device, debugstr_a(src_file), load_info, pump, texture, hresult);

    if (!device)
        return E_INVALIDARG;
    if (!src_file)
        return E_FAIL;

    if (!(str_len = MultiByteToWideChar(CP_ACP, 0, src_file, -1, NULL, 0)))
        return HRESULT_FROM_WIN32(GetLastError());

    if (!(buffer = malloc(str_len * sizeof(*buffer))))
        return E_OUTOFMEMORY;

    MultiByteToWideChar(CP_ACP, 0, src_file, -1, buffer, str_len);
    hr = D3DX11CreateTextureFromFileW(device, buffer, load_info, pump, texture, hresult);

    free(buffer);

    return hr;
}

HRESULT WINAPI D3DX11CreateTextureFromFileW(ID3D11Device *device, const WCHAR *src_file,
        D3DX11_IMAGE_LOAD_INFO *load_info, ID3DX11ThreadPump *pump, ID3D11Resource **texture,
        HRESULT *hresult)
{
    void *buffer = NULL;
    DWORD size = 0;
    HRESULT hr;

    TRACE("device %p, src_file %s, load_info %p, pump %p, texture %p, hresult %p.\n",
            device, debugstr_w(src_file), load_info, pump, texture, hresult);

    if (!device)
        return E_INVALIDARG;
    if (!src_file)
        return E_FAIL;

    if (pump)
    {
        ID3DX11DataProcessor *processor;
        ID3DX11DataLoader *loader;

        if (FAILED((hr = D3DX11CreateAsyncFileLoaderW(src_file, &loader))))
            return hr;
        if (FAILED((hr = D3DX11CreateAsyncTextureProcessor(device, load_info, &processor))))
        {
            ID3DX11DataLoader_Destroy(loader);
            return hr;
        }
        if (FAILED((hr = ID3DX11ThreadPump_AddWorkItem(pump, loader, processor, hresult, (void **)texture))))
        {
            ID3DX11DataLoader_Destroy(loader);
            ID3DX11DataProcessor_Destroy(processor);
        }
        return hr;
    }

    if (SUCCEEDED((hr = load_file(src_file, &buffer, &size))))
    {
        hr = create_texture(device, buffer, size, load_info, texture);
        free(buffer);
    }
    if (hresult)
        *hresult = hr;
    return hr;
}

HRESULT WINAPI D3DX11CreateTextureFromResourceA(ID3D11Device *device, HMODULE module, const char *resource,
        D3DX11_IMAGE_LOAD_INFO *load_info, ID3DX11ThreadPump *pump, ID3D11Resource **texture, HRESULT *hresult)
{
    void *buffer;
    DWORD size;
    HRESULT hr;

    TRACE("device %p, module %p, resource %s, load_info %p, pump %p, texture %p, hresult %p.\n",
            device, module, debugstr_a(resource), load_info, pump, texture, hresult);

    if (!device)
        return E_INVALIDARG;

    if (pump)
    {
        ID3DX11DataProcessor *processor;
        ID3DX11DataLoader *loader;

        if (FAILED((hr = D3DX11CreateAsyncResourceLoaderA(module, resource, &loader))))
            return hr;
        if (FAILED((hr = D3DX11CreateAsyncTextureProcessor(device, load_info, &processor))))
        {
            ID3DX11DataLoader_Destroy(loader);
            return hr;
        }
        if (FAILED((hr = ID3DX11ThreadPump_AddWorkItem(pump, loader, processor, hresult, (void **)texture))))
        {
            ID3DX11DataLoader_Destroy(loader);
            ID3DX11DataProcessor_Destroy(processor);
        }
        return hr;
    }

    if (FAILED((hr = load_resourceA(module, resource, &buffer, &size))))
        return hr;
    hr = create_texture(device, buffer, size, load_info, texture);
    if (hresult)
        *hresult = hr;
    return hr;
}

HRESULT WINAPI D3DX11CreateTextureFromResourceW(ID3D11Device *device, HMODULE module, const WCHAR *resource,
        D3DX11_IMAGE_LOAD_INFO *load_info, ID3DX11ThreadPump *pump, ID3D11Resource **texture, HRESULT *hresult)
{
    void *buffer;
    DWORD size;
    HRESULT hr;

    TRACE("device %p, module %p, resource %s, load_info %p, pump %p, texture %p, hresult %p.\n",
            device, module, debugstr_w(resource), load_info, pump, texture, hresult);

    if (!device)
        return E_INVALIDARG;

    if (pump)
    {
        ID3DX11DataProcessor *processor;
        ID3DX11DataLoader *loader;

        if (FAILED((hr = D3DX11CreateAsyncResourceLoaderW(module, resource, &loader))))
            return hr;
        if (FAILED((hr = D3DX11CreateAsyncTextureProcessor(device, load_info, &processor))))
        {
            ID3DX11DataLoader_Destroy(loader);
            return hr;
        }
        if (FAILED((hr = ID3DX11ThreadPump_AddWorkItem(pump, loader, processor, hresult, (void **)texture))))
        {
            ID3DX11DataLoader_Destroy(loader);
            ID3DX11DataProcessor_Destroy(processor);
        }
        return hr;
    }

    if (FAILED((hr = load_resourceW(module, resource, &buffer, &size))))
        return hr;
    hr = create_texture(device, buffer, size, load_info, texture);
    if (hresult)
        *hresult = hr;
    return hr;
}

/*
 * D3DX11CreateShaderResourceView variants.
 */
HRESULT WINAPI D3DX11CreateShaderResourceViewFromFileA(ID3D11Device *device, const char *src_file,
        D3DX11_IMAGE_LOAD_INFO *load_info, ID3DX11ThreadPump *pump, ID3D11ShaderResourceView **srv, HRESULT *hresult)
{
    WCHAR *buffer;
    int str_len;
    HRESULT hr;

    TRACE("device %p, src_file %s, load_info %p, pump %p, srv %p, hresult %p.\n",
            device, debugstr_a(src_file), load_info, pump, srv, hresult);

    if (!device)
        return E_INVALIDARG;
    if (!src_file)
        return E_FAIL;

    if (!(str_len = MultiByteToWideChar(CP_ACP, 0, src_file, -1, NULL, 0)))
        return HRESULT_FROM_WIN32(GetLastError());

    if (!(buffer = malloc(str_len * sizeof(*buffer))))
        return E_OUTOFMEMORY;

    MultiByteToWideChar(CP_ACP, 0, src_file, -1, buffer, str_len);
    hr = D3DX11CreateShaderResourceViewFromFileW(device, buffer, load_info, pump, srv, hresult);

    free(buffer);

    return hr;
}

HRESULT WINAPI D3DX11CreateShaderResourceViewFromFileW(ID3D11Device *device, const WCHAR *src_file,
        D3DX11_IMAGE_LOAD_INFO *load_info, ID3DX11ThreadPump *pump, ID3D11ShaderResourceView **srv, HRESULT *hresult)
{
    ID3D11Resource *texture;
    void *buffer = NULL;
    DWORD size = 0;
    HRESULT hr;

    TRACE("device %p, src_file %s, load_info %p, pump %p, srv %p, hresult %p.\n",
            device, debugstr_w(src_file), load_info, pump, srv, hresult);

    if (!device)
        return E_INVALIDARG;
    if (!src_file)
        return E_FAIL;

    if (pump)
    {
        ID3DX11DataProcessor *processor;
        ID3DX11DataLoader *loader;

        if (FAILED((hr = D3DX11CreateAsyncFileLoaderW(src_file, &loader))))
            return hr;
        if (FAILED((hr = D3DX11CreateAsyncShaderResourceViewProcessor(device, load_info, &processor))))
        {
            ID3DX11DataLoader_Destroy(loader);
            return hr;
        }
        if (FAILED((hr = ID3DX11ThreadPump_AddWorkItem(pump, loader, processor, hresult, (void **)srv))))
        {
            ID3DX11DataLoader_Destroy(loader);
            ID3DX11DataProcessor_Destroy(processor);
        }
        return hr;
    }

    if (SUCCEEDED((hr = load_file(src_file, &buffer, &size))))
    {
        hr = create_texture(device, buffer, size, load_info, &texture);
        if (SUCCEEDED(hr))
        {
            hr = ID3D11Device_CreateShaderResourceView(device, texture, NULL, srv);
            ID3D11Resource_Release(texture);
        }
        free(buffer);
    }
    if (hresult)
        *hresult = hr;
    return hr;
}

HRESULT WINAPI D3DX11CreateShaderResourceViewFromResourceA(ID3D11Device *device, HMODULE module, const char *resource,
        D3DX11_IMAGE_LOAD_INFO *load_info, ID3DX11ThreadPump *pump, ID3D11ShaderResourceView **srv, HRESULT *hresult)
{
    ID3D11Resource *texture;
    void *buffer;
    DWORD size;
    HRESULT hr;

    TRACE("device %p, module %p, resource %s, load_info %p, pump %p, srv %p, hresult %p.\n",
            device, module, debugstr_a(resource), load_info, pump, srv, hresult);

    if (!device)
        return E_INVALIDARG;

    if (pump)
    {
        ID3DX11DataProcessor *processor;
        ID3DX11DataLoader *loader;

        if (FAILED((hr = D3DX11CreateAsyncResourceLoaderA(module, resource, &loader))))
            return hr;
        if (FAILED((hr = D3DX11CreateAsyncShaderResourceViewProcessor(device, load_info, &processor))))
        {
            ID3DX11DataLoader_Destroy(loader);
            return hr;
        }
        if (FAILED((hr = ID3DX11ThreadPump_AddWorkItem(pump, loader, processor, hresult, (void **)srv))))
        {
            ID3DX11DataLoader_Destroy(loader);
            ID3DX11DataProcessor_Destroy(processor);
        }
        return hr;
    }

    if (FAILED((hr = load_resourceA(module, resource, &buffer, &size))))
        return hr;
    hr = create_texture(device, buffer, size, load_info, &texture);
    if (SUCCEEDED(hr))
    {
        hr = ID3D11Device_CreateShaderResourceView(device, texture, NULL, srv);
        ID3D11Resource_Release(texture);
    }
    if (hresult)
        *hresult = hr;
    return hr;
}

HRESULT WINAPI D3DX11CreateShaderResourceViewFromResourceW(ID3D11Device *device, HMODULE module, const WCHAR *resource,
        D3DX11_IMAGE_LOAD_INFO *load_info, ID3DX11ThreadPump *pump, ID3D11ShaderResourceView **srv, HRESULT *hresult)
{
    ID3D11Resource *texture;
    void *buffer;
    DWORD size;
    HRESULT hr;

    TRACE("device %p, module %p, resource %s, load_info %p, pump %p, srv %p, hresult %p.\n",
            device, module, debugstr_w(resource), load_info, pump, srv, hresult);

    if (!device)
        return E_INVALIDARG;

    if (pump)
    {
        ID3DX11DataProcessor *processor;
        ID3DX11DataLoader *loader;

        if (FAILED((hr = D3DX11CreateAsyncResourceLoaderW(module, resource, &loader))))
            return hr;
        if (FAILED((hr = D3DX11CreateAsyncShaderResourceViewProcessor(device, load_info, &processor))))
        {
            ID3DX11DataLoader_Destroy(loader);
            return hr;
        }
        if (FAILED((hr = ID3DX11ThreadPump_AddWorkItem(pump, loader, processor, hresult, (void **)srv))))
        {
            ID3DX11DataLoader_Destroy(loader);
            ID3DX11DataProcessor_Destroy(processor);
        }
        return hr;
    }

    if (FAILED((hr = load_resourceW(module, resource, &buffer, &size))))
        return hr;
    hr = create_texture(device, buffer, size, load_info, &texture);
    if (SUCCEEDED(hr))
    {
        hr = ID3D11Device_CreateShaderResourceView(device, texture, NULL, srv);
        ID3D11Resource_Release(texture);
    }
    if (hresult)
        *hresult = hr;
    return hr;
}

HRESULT WINAPI D3DX11CreateShaderResourceViewFromMemory(ID3D11Device *device, const void *src_data, SIZE_T src_data_size,
        D3DX11_IMAGE_LOAD_INFO *load_info, ID3DX11ThreadPump *pump, ID3D11ShaderResourceView **srv, HRESULT *hresult)
{
    ID3D11Resource *texture;
    HRESULT hr;

    TRACE("device %p, src_data %p, src_data_size %Iu, load_info %p, pump %p, srv %p, hresult %p.\n",
            device, src_data, src_data_size, load_info, pump, srv, hresult);

    if (!device)
        return E_INVALIDARG;
    if (!src_data)
        return E_FAIL;

    if (pump)
    {
        ID3DX11DataProcessor *processor;
        ID3DX11DataLoader *loader;

        if (FAILED((hr = D3DX11CreateAsyncMemoryLoader(src_data, src_data_size, &loader))))
            return hr;
        if (FAILED((hr = D3DX11CreateAsyncShaderResourceViewProcessor(device, load_info, &processor))))
        {
            ID3DX11DataLoader_Destroy(loader);
            return hr;
        }
        if (FAILED((hr = ID3DX11ThreadPump_AddWorkItem(pump, loader, processor, hresult, (void **)srv))))
        {
            ID3DX11DataLoader_Destroy(loader);
            ID3DX11DataProcessor_Destroy(processor);
        }
        return hr;
    }

    hr = create_texture(device, src_data, src_data_size, load_info, &texture);
    if (SUCCEEDED(hr))
    {
        hr = ID3D11Device_CreateShaderResourceView(device, texture, NULL, srv);
        ID3D11Resource_Release(texture);
    }
    if (hresult)
        *hresult = hr;
    return hr;
}

/*
 * D3DX11LoadTextureFromTexture implementation.
 */
struct d3d11_texture_resource {
    D3D11_RESOURCE_DIMENSION texture_dimension;
    union
    {
        ID3D11Resource  *tex_rsrc;
        ID3D11Texture2D *tex_2d;
        ID3D11Texture3D *tex_3d;
    } iface;
    struct volume size;
    uint32_t mip_levels;
    uint32_t layer_count;
};

struct d3d11_texture {
    ID3D11Device *device;
    ID3D11DeviceContext *device_context;
    struct d3d11_texture_resource texture;
    struct d3d11_texture_resource staging_texture;

    const struct pixel_format_desc *fmt_desc;
    D3D11_MAP map_flags;
    D3D11_BOX texture_box;

    uint32_t first_layer;
    uint32_t first_mip_level;
};

static void set_d3d11_box(D3D11_BOX *box, uint32_t left, uint32_t top, uint32_t right, uint32_t bottom, uint32_t front,
        uint32_t back)
{
    box->left = left;
    box->top = top;
    box->right = right;
    box->bottom = bottom;
    box->front = front;
    box->back = back;
}

static const char *debug_d3d11_box(const struct D3D11_BOX *box)
{
    if (!box)
        return "(null)";
    return wine_dbg_sprintf("(%ux%ux%u)-(%ux%ux%u)", box->left, box->top, box->front, box->right, box->bottom, box->back);
}

static void d3d11_box_get_mip_level(D3D11_BOX *box, uint32_t level)
{
    uint32_t i;

    for (i = 0; i < level; ++i)
    {
        set_d3d11_box(box, (box->left ? (box->left / 2) : 0), (box->top ? (box->top / 2) : 0),
                max(box->right / 2, 1), max(box->bottom / 2, 1),
                (box->front ? (box->front / 2) : 0), max(box->back / 2, 1));
    }
}

static HRESULT d3dx_d3d11_texture_init(ID3D11DeviceContext *context, ID3D11Resource *tex_rsrc, uint32_t first_layer,
        uint32_t first_mip_level, D3D11_MAP map_flags, D3D11_BOX *tex_box, struct d3d11_texture *texture)
{
    struct d3d11_texture_resource *staging_tex_rsrc = &texture->staging_texture;
    struct d3d11_texture_resource *src_tex_rsrc = &texture->texture;
    HRESULT hr;

    ID3D11Resource_GetDevice(tex_rsrc, &texture->device);
    if (!texture->device)
    {
        ERR("Failed to get device from texture resource.\n");
        return E_FAIL;
    }

    texture->map_flags = map_flags;
    ID3D11Resource_GetType(tex_rsrc, &src_tex_rsrc->texture_dimension);
    switch (src_tex_rsrc->texture_dimension)
    {
    case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
    {
        D3D11_TEXTURE2D_DESC desc;

        hr = ID3D11Resource_QueryInterface(tex_rsrc, &IID_ID3D11Texture2D, (void **)&src_tex_rsrc->iface.tex_2d);
        if (FAILED(hr))
            return hr;

        ID3D11Texture2D_GetDesc(src_tex_rsrc->iface.tex_2d, &desc);
        if (map_flags != D3D11_MAP_READ && (first_mip_level >= desc.MipLevels))
            return S_FALSE;

        texture->fmt_desc = get_d3dx_pixel_format_info(d3dx_pixel_format_id_from_dxgi_format(desc.Format));
        if (texture->fmt_desc->format == D3DX_PIXEL_FORMAT_COUNT)
        {
            FIXME("Unknown DXGI format supplied, %#x.\n", desc.Format);
            return E_NOTIMPL;
        }

        set_volume_struct(&src_tex_rsrc->size, desc.Width, desc.Height, 1);
        src_tex_rsrc->mip_levels = desc.MipLevels;
        src_tex_rsrc->layer_count = desc.ArraySize;

        texture->first_mip_level = min((desc.MipLevels - 1), first_mip_level);
        texture->first_layer = first_layer >= desc.ArraySize ? 0 : first_layer;

        staging_tex_rsrc->texture_dimension = src_tex_rsrc->texture_dimension;
        staging_tex_rsrc->size = src_tex_rsrc->size;
        d3dx_get_mip_level_size(&staging_tex_rsrc->size, texture->first_mip_level);
        staging_tex_rsrc->mip_levels = src_tex_rsrc->mip_levels - texture->first_mip_level;
        staging_tex_rsrc->layer_count = 1;

        /* Create the staging texture. */
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = desc.MiscFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        if (map_flags != D3D11_MAP_READ)
            desc.CPUAccessFlags |= D3D11_CPU_ACCESS_WRITE;
        desc.ArraySize = 1;
        desc.MipLevels = staging_tex_rsrc->mip_levels;
        desc.Width = staging_tex_rsrc->size.width;
        desc.Height = staging_tex_rsrc->size.height;

        hr = ID3D11Device_CreateTexture2D(texture->device, &desc, NULL, &staging_tex_rsrc->iface.tex_2d);
        if (FAILED(hr))
            return hr;
        break;
    }

    case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
    {
        D3D11_TEXTURE3D_DESC desc;

        hr = ID3D11Resource_QueryInterface(tex_rsrc, &IID_ID3D11Texture3D, (void **)&src_tex_rsrc->iface.tex_3d);
        if (FAILED(hr))
            return hr;

        ID3D11Texture3D_GetDesc(src_tex_rsrc->iface.tex_3d, &desc);
        if (map_flags != D3D11_MAP_READ && (first_mip_level >= desc.MipLevels))
            return S_FALSE;

        texture->fmt_desc = get_d3dx_pixel_format_info(d3dx_pixel_format_id_from_dxgi_format(desc.Format));
        if (texture->fmt_desc->format == D3DX_PIXEL_FORMAT_COUNT)
        {
            FIXME("Unknown DXGI format supplied, %#x.\n", desc.Format);
            return E_NOTIMPL;
        }

        set_volume_struct(&src_tex_rsrc->size, desc.Width, desc.Height, desc.Depth);
        src_tex_rsrc->mip_levels = desc.MipLevels;
        src_tex_rsrc->layer_count = 1;

        texture->first_mip_level = min((desc.MipLevels - 1), first_mip_level);
        if (first_layer)
            WARN("Specified a non zero FirstElement argument on a 3D texture.\n");
        texture->first_layer = 0;

        staging_tex_rsrc->texture_dimension = src_tex_rsrc->texture_dimension;
        staging_tex_rsrc->size = src_tex_rsrc->size;
        d3dx_get_mip_level_size(&staging_tex_rsrc->size, texture->first_mip_level);
        staging_tex_rsrc->mip_levels = src_tex_rsrc->mip_levels - texture->first_mip_level;
        staging_tex_rsrc->layer_count = 1;

        /* Create the staging texture. */
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = desc.MiscFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        if (map_flags != D3D11_MAP_READ)
            desc.CPUAccessFlags |= D3D11_CPU_ACCESS_WRITE;
        desc.MipLevels = staging_tex_rsrc->mip_levels;
        desc.Width = staging_tex_rsrc->size.width;
        desc.Height = staging_tex_rsrc->size.height;
        desc.Depth = staging_tex_rsrc->size.depth;

        hr = ID3D11Device_CreateTexture3D(texture->device, &desc, NULL, &staging_tex_rsrc->iface.tex_3d);
        if (FAILED(hr))
            return hr;
        break;
    }

    default:
        FIXME("Unhandled resource dimension %u.\n", src_tex_rsrc->texture_dimension);
        return E_NOTIMPL;
    }

    if (tex_box)
        texture->texture_box = *tex_box;
    else
        set_d3d11_box(&texture->texture_box, 0, 0, staging_tex_rsrc->size.width, staging_tex_rsrc->size.height, 0,
                staging_tex_rsrc->size.depth);
    texture->device_context = context;
    ID3D11DeviceContext_AddRef(context);

    return S_OK;
}

static void d3dx_d3d11_texture_release(struct d3d11_texture *texture)
{
    if (texture->device)
        ID3D11Device_Release(texture->device);
    if (texture->device_context)
        ID3D11DeviceContext_Release(texture->device_context);
    if (texture->texture.iface.tex_rsrc)
        ID3D11Resource_Release(texture->texture.iface.tex_rsrc);
    if (texture->staging_texture.iface.tex_rsrc)
        ID3D11Resource_Release(texture->staging_texture.iface.tex_rsrc);
}

static HRESULT d3dx_d3d11_texture_map(struct d3d11_texture *texture, uint32_t layer, uint32_t mip_level,
        struct d3dx_pixels *pixels)
{
    struct d3d11_texture_resource *staging_tex_rsrc = &texture->staging_texture;
    struct d3d11_texture_resource *src_tex_rsrc = &texture->texture;
    D3D11_BOX tmp_box = texture->texture_box;
    D3D11_MAPPED_SUBRESOURCE map = { 0 };
    uint32_t sub_rsrc_idx;
    HRESULT hr;

    d3d11_box_get_mip_level(&tmp_box, mip_level);
    sub_rsrc_idx = (src_tex_rsrc->mip_levels * (texture->first_layer + layer)) + (mip_level + texture->first_mip_level);
    ID3D11DeviceContext_CopySubresourceRegion(texture->device_context, staging_tex_rsrc->iface.tex_rsrc, mip_level, 0, 0, 0,
             src_tex_rsrc->iface.tex_rsrc, sub_rsrc_idx, NULL);
    hr = ID3D11DeviceContext_Map(texture->device_context, staging_tex_rsrc->iface.tex_rsrc, mip_level,
            texture->map_flags, 0, &map);
    if (FAILED(hr))
        return hr;

    TRACE("Mapping layer %u, mip level %u, box %s.\n", texture->first_layer + layer, texture->first_mip_level + mip_level,
            debug_d3d11_box(&tmp_box));
    return d3dx_pixels_init(map.pData, map.RowPitch, map.DepthPitch, NULL, texture->fmt_desc->format, tmp_box.left, tmp_box.top,
            tmp_box.right, tmp_box.bottom, tmp_box.front, tmp_box.back, pixels);
}

static void d3dx_d3d11_texture_unmap(struct d3d11_texture *texture, uint32_t layer, uint32_t mip_level)
{
    struct d3d11_texture_resource *staging_tex_rsrc = &texture->staging_texture;
    struct d3d11_texture_resource *src_tex_rsrc = &texture->texture;
    uint32_t sub_rsrc_idx;

    ID3D11DeviceContext_Unmap(texture->device_context, staging_tex_rsrc->iface.tex_rsrc, mip_level);
    if (texture->map_flags == D3D11_MAP_READ)
        return;

    sub_rsrc_idx = (src_tex_rsrc->mip_levels * (texture->first_layer + layer)) + (mip_level + texture->first_mip_level);
    ID3D11DeviceContext_CopySubresourceRegion(texture->device_context, src_tex_rsrc->iface.tex_rsrc, sub_rsrc_idx, 0, 0, 0,
            staging_tex_rsrc->iface.tex_rsrc, mip_level, NULL);
}

static const D3DX11_TEXTURE_LOAD_INFO default_load_info = { NULL, NULL, 0, 0, D3DX11_DEFAULT, 0, 0, D3DX11_DEFAULT,
                                                            D3DX11_DEFAULT, D3DX11_DEFAULT };
HRESULT WINAPI D3DX11LoadTextureFromTexture(ID3D11DeviceContext *context, ID3D11Resource *src_texture,
        D3DX11_TEXTURE_LOAD_INFO *load_info, ID3D11Resource *dst_texture)
{
    D3DX11_TEXTURE_LOAD_INFO info = (load_info) ? *load_info : default_load_info;
    struct d3d11_texture src_tex = { 0 };
    struct d3d11_texture dst_tex = { 0 };
    uint32_t i, j, loaded_mip_levels;
    HRESULT hr;

    TRACE("context %p, src_texture %p, load_info %p, dst_texture %p.\n", context, src_texture, load_info, dst_texture);

    if (!src_texture || !dst_texture)
        return E_INVALIDARG;

    if (!context)
        return D3DERR_INVALIDCALL;

    if (!info.Filter || FAILED(hr = d3dx_handle_filter(&info.Filter)))
    {
        FIXME("Invalid filter argument.\n");
        return D3DERR_INVALIDCALL;
    }

    hr = d3dx_d3d11_texture_init(context, src_texture, info.SrcFirstElement, info.SrcFirstMip, D3D11_MAP_READ, info.pSrcBox, &src_tex);
    if (FAILED(hr))
        goto end;

    hr = d3dx_d3d11_texture_init(context, dst_texture, info.DstFirstElement, info.DstFirstMip, D3D11_MAP_READ_WRITE, info.pDstBox, &dst_tex);
    if (hr == S_FALSE || FAILED(hr))
        goto end;

    if ((src_texture == dst_texture) && ((src_tex.first_layer == dst_tex.first_layer) &&
                (src_tex.first_mip_level == dst_tex.first_mip_level)))
    {
        hr = D3DERR_INVALIDCALL;
        goto end;
    }

    if (!info.NumMips || info.NumMips == D3DX11_DEFAULT)
        info.NumMips = dst_tex.staging_texture.mip_levels;
    info.NumMips = min(info.NumMips, dst_tex.staging_texture.mip_levels);
    if (!info.NumElements || info.NumElements == D3DX11_DEFAULT)
        info.NumElements = min(src_tex.texture.layer_count, dst_tex.texture.layer_count);
    info.NumElements = min(info.NumElements, min(src_tex.texture.layer_count, dst_tex.texture.layer_count));
    loaded_mip_levels = min(info.NumMips, src_tex.staging_texture.mip_levels);
    for (i = 0; i < info.NumElements; ++i)
    {
        for (j = 0; j < loaded_mip_levels; ++j)
        {
            struct d3dx_pixels src_pixels, dst_pixels;

            hr = d3dx_d3d11_texture_map(&src_tex, i, j, &src_pixels);
            if (FAILED(hr))
                goto end;

            hr = d3dx_d3d11_texture_map(&dst_tex, i, j, &dst_pixels);
            if (FAILED(hr))
            {
                d3dx_d3d11_texture_unmap(&src_tex, i, j);
                goto end;
            }

            hr = d3dx_load_pixels_from_pixels(&dst_pixels, dst_tex.fmt_desc, &src_pixels, src_tex.fmt_desc, info.Filter, 0);
            d3dx_d3d11_texture_unmap(&src_tex, i, j);
            d3dx_d3d11_texture_unmap(&dst_tex, i, j);
            if (FAILED(hr))
            {
                WARN("Failed with hr %#lx.\n", hr);
                goto end;
            }
        }
    }

    if (loaded_mip_levels < info.NumMips)
    {
        if (!info.MipFilter || FAILED(hr = d3dx_handle_filter(&info.MipFilter)))
        {
            FIXME("Invalid mip filter argument.\n");
            hr = D3DERR_INVALIDCALL;
            goto end;
        }

        for (i = 0; i < info.NumElements; ++i)
        {
            for (j = loaded_mip_levels; j < info.NumMips; ++j)
            {
                struct d3dx_pixels src_pixels, dst_pixels;

                hr = d3dx_d3d11_texture_map(&dst_tex, i, j - 1, &src_pixels);
                if (FAILED(hr))
                    break;

                hr = d3dx_d3d11_texture_map(&dst_tex, i, j, &dst_pixels);
                if (SUCCEEDED(hr))
                {
                    hr = d3dx_load_pixels_from_pixels(&dst_pixels, dst_tex.fmt_desc, &src_pixels, dst_tex.fmt_desc, info.MipFilter, 0);
                    d3dx_d3d11_texture_unmap(&dst_tex, i, j);
                }
                d3dx_d3d11_texture_unmap(&dst_tex, i, j - 1);
                if (FAILED(hr))
                    goto end;
            }
        }
    }

end:
    d3dx_d3d11_texture_release(&src_tex);
    d3dx_d3d11_texture_release(&dst_tex);
    return SUCCEEDED(hr) ? S_OK : hr;
}

static uint32_t d3d11_get_resource_mip_levels(ID3D11Resource *rsrc)
{
    D3D11_RESOURCE_DIMENSION rsrc_dim;
    uint32_t mip_levels = 0;
    HRESULT hr;

    ID3D11Resource_GetType(rsrc, &rsrc_dim);
    switch (rsrc_dim)
    {
    case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
    {
        D3D11_TEXTURE2D_DESC desc;
        ID3D11Texture2D *tex_2d;

        hr = ID3D11Resource_QueryInterface(rsrc, &IID_ID3D11Texture2D, (void **)&tex_2d);
        if (FAILED(hr))
            break;

        ID3D11Texture2D_GetDesc(tex_2d, &desc);
        ID3D11Texture2D_Release(tex_2d);
        mip_levels = desc.MipLevels;
        break;
    }

    case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
    {
        D3D11_TEXTURE3D_DESC desc;
        ID3D11Texture3D *tex_3d;

        hr = ID3D11Resource_QueryInterface(rsrc, &IID_ID3D11Texture3D, (void **)&tex_3d);
        if (FAILED(hr))
            break;

        ID3D11Texture3D_GetDesc(tex_3d, &desc);
        ID3D11Texture3D_Release(tex_3d);
        mip_levels = desc.MipLevels;
        break;
    }

    default:
        break;
    }

    return mip_levels;
}

HRESULT WINAPI D3DX11FilterTexture(ID3D11DeviceContext *context, ID3D11Resource *texture, UINT src_level, UINT filter)
{
    D3DX11_TEXTURE_LOAD_INFO load_info = { NULL, NULL, src_level, src_level + 1, 0, 0, 0, 0, filter, filter };

    TRACE("context %p, texture %p, src_level %u, filter %#x.\n", context, texture, src_level, filter);

    if (!context)
        return D3DERR_INVALIDCALL;

    if (d3d11_get_resource_mip_levels(texture) <= src_level)
        return S_OK;

    return D3DX11LoadTextureFromTexture(context, texture, &load_info, texture);
}
