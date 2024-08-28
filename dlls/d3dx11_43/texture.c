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

#include "d3dx11.h"
#include "d3dcompiler.h"
#include "dxhelpers.h"

WINE_DEFAULT_DEBUG_CHANNEL(d3dx);

HRESULT WINAPI D3DX11CreateShaderResourceViewFromMemory(ID3D11Device *device, const void *data,
        SIZE_T data_size, D3DX11_IMAGE_LOAD_INFO *load_info, ID3DX11ThreadPump *pump,
        ID3D11ShaderResourceView **view, HRESULT *hresult)
{
    FIXME("device %p, data %p, data_size %Iu, load_info %p, pump %p, view %p, hresult %p stub!\n",
            device, data, data_size, load_info, pump, view, hresult);

    return E_NOTIMPL;
}

HRESULT WINAPI D3DX11CreateTextureFromFileA(ID3D11Device *device, const char *filename,
        D3DX11_IMAGE_LOAD_INFO *load_info, ID3DX11ThreadPump *pump, ID3D11Resource **texture,
        HRESULT *hresult)
{
    FIXME("device %p, filename %s, load_info %p, pump %p, texture %p, hresult %p stub.\n",
            device, debugstr_a(filename), load_info, pump, texture, hresult);

    return E_NOTIMPL;
}

HRESULT WINAPI D3DX11CreateTextureFromFileW(ID3D11Device *device, const WCHAR *filename,
        D3DX11_IMAGE_LOAD_INFO *load_info, ID3DX11ThreadPump *pump, ID3D11Resource **texture,
        HRESULT *hresult)
{
    FIXME("device %p, filename %s, load_info %p, pump %p, texture %p, hresult %p stub.\n",
            device, debugstr_w(filename), load_info, pump, texture, hresult);

    return E_NOTIMPL;
}

HRESULT WINAPI D3DX11CreateTextureFromMemory(ID3D11Device *device, const void *data,
        SIZE_T data_size, D3DX11_IMAGE_LOAD_INFO *load_info, ID3DX11ThreadPump *pump,
        ID3D11Resource **texture, HRESULT *hresult)
{
    FIXME("device %p, data %p, data_size %Iu, load_info %p, pump %p, texture %p, hresult %p stub.\n",
            device, data, data_size, load_info, pump, texture, hresult);

    return E_NOTIMPL;
}

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

HRESULT WINAPI D3DX11LoadTextureFromTexture(ID3D11DeviceContext *context, ID3D11Resource *src_texture,
        D3DX11_TEXTURE_LOAD_INFO *info, ID3D11Resource *dst_texture)
{
    FIXME("context %p, src_texture %p, info %p, dst_texture %p stub!\n",
            context, src_texture, info, dst_texture);

    return E_NOTIMPL;
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
