/*
 * Copyright 2024 Connor McAdams for CodeWeavers
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
 *
 */

#include "d3dx9_private.h"

#include "ole2.h"
#include "wincodec.h"
#include "assert.h"
#include "initguid.h"

#define BCDEC_IMPLEMENTATION
#include "bcdec.h"
#define STB_DXT_IMPLEMENTATION
#include "stb_dxt.h"


WINE_DEFAULT_DEBUG_CHANNEL(d3dx);

HRESULT WINAPI WICCreateImagingFactory_Proxy(UINT, IWICImagingFactory**);

/* Wine-specific WIC GUIDs */
DEFINE_GUID(GUID_WineContainerFormatTga, 0x0c44fda1,0xa5c5,0x4298,0x96,0x85,0x47,0x3f,0xc1,0x7c,0xd3,0x22);

/************************************************************
 * pixel format table providing info about number of bytes per pixel,
 * number of bits per channel and format type.
 *
 * Call get_format_info to request information about a specific format.
 */
static const struct pixel_format_desc formats[] =
{
    /* format                                    bpc               shifts             bpp blocks     alpha type   rgb type     flags */
    {D3DX_PIXEL_FORMAT_B8G8R8_UNORM,             { 0,  8,  8,  8}, { 0, 16,  8,  0},  3, 1, 1,  3, { CTYPE_EMPTY, CTYPE_UNORM, FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_B8G8R8A8_UNORM,           { 8,  8,  8,  8}, {24, 16,  8,  0},  4, 1, 1,  4, { CTYPE_UNORM, CTYPE_UNORM, FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_B8G8R8A8_UNORM_SRGB,      { 8,  8,  8,  8}, {24, 16,  8,  0},  4, 1, 1,  4, { CTYPE_UNORM, CTYPE_UNORM, FMT_FLAG_SRGB_DXGI}},
    {D3DX_PIXEL_FORMAT_B8G8R8X8_UNORM,           { 0,  8,  8,  8}, { 0, 16,  8,  0},  4, 1, 1,  4, { CTYPE_EMPTY, CTYPE_UNORM, FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_R8G8B8A8_UNORM,           { 8,  8,  8,  8}, {24,  0,  8, 16},  4, 1, 1,  4, { CTYPE_UNORM, CTYPE_UNORM, FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_R8G8B8A8_UNORM_SRGB,      { 8,  8,  8,  8}, {24,  0,  8, 16},  4, 1, 1,  4, { CTYPE_UNORM, CTYPE_UNORM, FMT_FLAG_SRGB_DXGI}},
    {D3DX_PIXEL_FORMAT_R8G8B8X8_UNORM,           { 0,  8,  8,  8}, { 0,  0,  8, 16},  4, 1, 1,  4, { CTYPE_EMPTY, CTYPE_UNORM, FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_B5G6R5_UNORM,             { 0,  5,  6,  5}, { 0, 11,  5,  0},  2, 1, 1,  2, { CTYPE_EMPTY, CTYPE_UNORM, FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_B5G5R5X1_UNORM,           { 0,  5,  5,  5}, { 0, 10,  5,  0},  2, 1, 1,  2, { CTYPE_EMPTY, CTYPE_UNORM, FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_B5G5R5A1_UNORM,           { 1,  5,  5,  5}, {15, 10,  5,  0},  2, 1, 1,  2, { CTYPE_UNORM, CTYPE_UNORM, FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_B2G3R3_UNORM,             { 0,  3,  3,  2}, { 0,  5,  2,  0},  1, 1, 1,  1, { CTYPE_EMPTY, CTYPE_UNORM, FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_B2G3R3A8_UNORM,           { 8,  3,  3,  2}, { 8,  5,  2,  0},  2, 1, 1,  2, { CTYPE_UNORM, CTYPE_UNORM, FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_B4G4R4A4_UNORM,           { 4,  4,  4,  4}, {12,  8,  4,  0},  2, 1, 1,  2, { CTYPE_UNORM, CTYPE_UNORM, FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_B4G4R4X4_UNORM,           { 0,  4,  4,  4}, { 0,  8,  4,  0},  2, 1, 1,  2, { CTYPE_EMPTY, CTYPE_UNORM, FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_B10G10R10A2_UNORM,        { 2, 10, 10, 10}, {30, 20, 10,  0},  4, 1, 1,  4, { CTYPE_UNORM, CTYPE_UNORM, FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_R10G10B10A2_UNORM,        { 2, 10, 10, 10}, {30,  0, 10, 20},  4, 1, 1,  4, { CTYPE_UNORM, CTYPE_UNORM, FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_R16G16B16A16_UNORM,       {16, 16, 16, 16}, {48,  0, 16, 32},  8, 1, 1,  8, { CTYPE_UNORM, CTYPE_UNORM, FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_R8_UNORM,                 { 0,  8,  0,  0}, { 0,  0,  0,  0},  1, 1, 1,  1, { CTYPE_EMPTY, CTYPE_UNORM, FMT_FLAG_DXGI    }},
    {D3DX_PIXEL_FORMAT_R8_SNORM,                 { 0,  8,  0,  0}, { 0,  0,  0,  0},  1, 1, 1,  1, { CTYPE_EMPTY, CTYPE_SNORM, FMT_FLAG_DXGI    }},
    {D3DX_PIXEL_FORMAT_R8G8_UNORM,               { 0,  8,  8,  0}, { 0,  0,  8,  0},  2, 1, 1,  2, { CTYPE_EMPTY, CTYPE_UNORM, FMT_FLAG_DXGI    }},
    {D3DX_PIXEL_FORMAT_R16_UNORM,                { 0, 16,  0,  0}, { 0,  0,  0,  0},  2, 1, 1,  2, { CTYPE_EMPTY, CTYPE_UNORM, FMT_FLAG_DXGI    }},
    {D3DX_PIXEL_FORMAT_R16G16_UNORM,             { 0, 16, 16,  0}, { 0,  0, 16,  0},  4, 1, 1,  4, { CTYPE_EMPTY, CTYPE_UNORM, FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_A8_UNORM,                 { 8,  0,  0,  0}, { 0,  0,  0,  0},  1, 1, 1,  1, { CTYPE_UNORM, CTYPE_EMPTY, FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_DXT1_UNORM,               { 0,  0,  0,  0}, { 0,  0,  0,  0},  1, 4, 4,  8, { CTYPE_UNORM, CTYPE_UNORM, FMT_FLAG_DXT     }},
    {D3DX_PIXEL_FORMAT_BC1_UNORM_SRGB,           { 0,  0,  0,  0}, { 0,  0,  0,  0},  1, 4, 4,  8, { CTYPE_UNORM, CTYPE_UNORM, FMT_FLAG_SRGB_DXT}},
    {D3DX_PIXEL_FORMAT_DXT2_UNORM,               { 0,  0,  0,  0}, { 0,  0,  0,  0},  1, 4, 4, 16, { CTYPE_UNORM, CTYPE_UNORM, FMT_FLAG_PMA_DXT }},
    {D3DX_PIXEL_FORMAT_DXT3_UNORM,               { 0,  0,  0,  0}, { 0,  0,  0,  0},  1, 4, 4, 16, { CTYPE_UNORM, CTYPE_UNORM, FMT_FLAG_DXT     }},
    {D3DX_PIXEL_FORMAT_BC2_UNORM_SRGB,           { 0,  0,  0,  0}, { 0,  0,  0,  0},  1, 4, 4, 16, { CTYPE_UNORM, CTYPE_UNORM, FMT_FLAG_SRGB_DXT}},
    {D3DX_PIXEL_FORMAT_DXT4_UNORM,               { 0,  0,  0,  0}, { 0,  0,  0,  0},  1, 4, 4, 16, { CTYPE_UNORM, CTYPE_UNORM, FMT_FLAG_PMA_DXT }},
    {D3DX_PIXEL_FORMAT_DXT5_UNORM,               { 0,  0,  0,  0}, { 0,  0,  0,  0},  1, 4, 4, 16, { CTYPE_UNORM, CTYPE_UNORM, FMT_FLAG_DXT     }},
    {D3DX_PIXEL_FORMAT_BC3_UNORM_SRGB,           { 0,  0,  0,  0}, { 0,  0,  0,  0},  1, 4, 4, 16, { CTYPE_UNORM, CTYPE_UNORM, FMT_FLAG_SRGB_DXT}},
    {D3DX_PIXEL_FORMAT_BC4_UNORM,                { 0,  0,  0,  0}, { 0,  0,  0,  0},  1, 4, 4,  8, { CTYPE_EMPTY, CTYPE_UNORM, FMT_FLAG_DXGI_DXT}},
    {D3DX_PIXEL_FORMAT_BC4_SNORM,                { 0,  0,  0,  0}, { 0,  0,  0,  0},  1, 4, 4,  8, { CTYPE_EMPTY, CTYPE_SNORM, FMT_FLAG_DXGI_DXT}},
    {D3DX_PIXEL_FORMAT_BC5_UNORM,                { 0,  0,  0,  0}, { 0,  0,  0,  0},  1, 4, 4, 16, { CTYPE_EMPTY, CTYPE_UNORM, FMT_FLAG_DXGI_DXT}},
    {D3DX_PIXEL_FORMAT_BC5_SNORM,                { 0,  0,  0,  0}, { 0,  0,  0,  0},  1, 4, 4, 16, { CTYPE_EMPTY, CTYPE_SNORM, FMT_FLAG_DXGI_DXT}},
    {D3DX_PIXEL_FORMAT_R16_FLOAT,                { 0, 16,  0,  0}, { 0,  0,  0,  0},  2, 1, 1,  2, { CTYPE_EMPTY, CTYPE_FLOAT, FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_R16G16_FLOAT,             { 0, 16, 16,  0}, { 0,  0, 16,  0},  4, 1, 1,  4, { CTYPE_EMPTY, CTYPE_FLOAT, FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_R16G16B16A16_FLOAT,       {16, 16, 16, 16}, {48,  0, 16, 32},  8, 1, 1,  8, { CTYPE_FLOAT, CTYPE_FLOAT, FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_R32_FLOAT,                { 0, 32,  0,  0}, { 0,  0,  0,  0},  4, 1, 1,  4, { CTYPE_EMPTY, CTYPE_FLOAT, FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_R32G32_FLOAT,             { 0, 32, 32,  0}, { 0,  0, 32,  0},  8, 1, 1,  8, { CTYPE_EMPTY, CTYPE_FLOAT, FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_R32G32B32_FLOAT,          { 0, 32, 32, 32}, { 0,  0, 32, 64}, 12, 1, 1, 12, { CTYPE_EMPTY, CTYPE_FLOAT, FMT_FLAG_DXGI    }},
    {D3DX_PIXEL_FORMAT_R32G32B32A32_FLOAT,       {32, 32, 32, 32}, {96,  0, 32, 64}, 16, 1, 1, 16, { CTYPE_FLOAT, CTYPE_FLOAT, FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_L8A8_UNORM,               { 8,  8,  0,  0}, { 8,  0,  0,  0},  2, 1, 1,  2, { CTYPE_UNORM, CTYPE_LUMA,  FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_L4A4_UNORM,               { 4,  4,  0,  0}, { 4,  0,  0,  0},  1, 1, 1,  1, { CTYPE_UNORM, CTYPE_LUMA,  FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_L8_UNORM,                 { 0,  8,  0,  0}, { 0,  0,  0,  0},  1, 1, 1,  1, { CTYPE_EMPTY, CTYPE_LUMA,  FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_L16_UNORM,                { 0, 16,  0,  0}, { 0,  0,  0,  0},  2, 1, 1,  2, { CTYPE_EMPTY, CTYPE_LUMA,  FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_P8_UINT,                  { 8,  8,  8,  8}, { 0,  0,  0,  0},  1, 1, 1,  1, { CTYPE_INDEX, CTYPE_INDEX, FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_P8_UINT_A8_UNORM,         { 8,  8,  8,  8}, { 8,  0,  0,  0},  2, 1, 1,  2, { CTYPE_UNORM, CTYPE_INDEX, FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_U8V8W8Q8_SNORM,           { 8,  8,  8,  8}, {24,  0,  8, 16},  4, 1, 1,  4, { CTYPE_SNORM, CTYPE_SNORM, FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_U8V8_SNORM,               { 0,  8,  8,  0}, { 0,  0,  8,  0},  2, 1, 1,  2, { CTYPE_EMPTY, CTYPE_SNORM, FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_U16V16_SNORM,             { 0, 16, 16,  0}, { 0,  0, 16,  0},  4, 1, 1,  4, { CTYPE_EMPTY, CTYPE_SNORM, FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_U8V8_SNORM_L8X8_UNORM,    { 8,  8,  8,  0}, {16,  0,  8,  0},  4, 1, 1,  4, { CTYPE_UNORM, CTYPE_SNORM, FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_U10V10W10_SNORM_A2_UNORM, { 2, 10, 10, 10}, {30,  0, 10, 20},  4, 1, 1,  4, { CTYPE_UNORM, CTYPE_SNORM, FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_U16V16W16Q16_SNORM,       {16, 16, 16, 16}, {48,  0, 16, 32},  8, 1, 1,  8, { CTYPE_SNORM, CTYPE_SNORM, FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_R8G8_B8G8_UNORM,          { 0,  0,  0,  0}, { 0,  0,  0,  0},  1, 2, 1,  4, { CTYPE_EMPTY, CTYPE_UNORM, FMT_FLAG_PACKED  }},
    {D3DX_PIXEL_FORMAT_G8R8_G8B8_UNORM,          { 0,  0,  0,  0}, { 0,  0,  0,  0},  1, 2, 1,  4, { CTYPE_EMPTY, CTYPE_UNORM, FMT_FLAG_PACKED  }},
    {D3DX_PIXEL_FORMAT_UYVY,                     { 0,  0,  0,  0}, { 0,  0,  0,  0},  1, 2, 1,  4, { CTYPE_EMPTY, CTYPE_UNORM, FMT_FLAG_PACKED  }},
    {D3DX_PIXEL_FORMAT_YUY2,                     { 0,  0,  0,  0}, { 0,  0,  0,  0},  1, 2, 1,  4, { CTYPE_EMPTY, CTYPE_UNORM, FMT_FLAG_PACKED  }},
    /* marks last element */
    {D3DX_PIXEL_FORMAT_COUNT,                    { 0,  0,  0,  0}, { 0,  0,  0,  0},  0, 1, 1,  0, { CTYPE_EMPTY, CTYPE_EMPTY, FMT_FLAG_NONE }},
};

const struct pixel_format_desc *get_d3dx_pixel_format_info(enum d3dx_pixel_format_id format)
{
    return &formats[min(format, D3DX_PIXEL_FORMAT_COUNT)];
}

struct d3dx_wic_pixel_format
{
    const GUID *wic_guid;
    enum d3dx_pixel_format_id format_id;
};

/* Sorted by GUID. */
static const struct d3dx_wic_pixel_format wic_pixel_formats[] =
{
    { &GUID_WICPixelFormat1bppIndexed, D3DX_PIXEL_FORMAT_P8_UINT },
    { &GUID_WICPixelFormat4bppIndexed, D3DX_PIXEL_FORMAT_P8_UINT },
    { &GUID_WICPixelFormat8bppIndexed, D3DX_PIXEL_FORMAT_P8_UINT },
    { &GUID_WICPixelFormat8bppGray,    D3DX_PIXEL_FORMAT_L8_UNORM },
    { &GUID_WICPixelFormat16bppBGR555, D3DX_PIXEL_FORMAT_B5G5R5X1_UNORM },
    { &GUID_WICPixelFormat16bppBGR565, D3DX_PIXEL_FORMAT_B5G6R5_UNORM },
    { &GUID_WICPixelFormat24bppBGR,    D3DX_PIXEL_FORMAT_B8G8R8_UNORM },
    { &GUID_WICPixelFormat32bppBGR,    D3DX_PIXEL_FORMAT_B8G8R8X8_UNORM },
    { &GUID_WICPixelFormat32bppBGRA,   D3DX_PIXEL_FORMAT_B8G8R8A8_UNORM }
};

static int __cdecl d3dx_wic_pixel_format_guid_compare(const void *a, const void *b)
{
    const struct d3dx_wic_pixel_format *format = b;
    const GUID *guid = a;

    return memcmp(guid, format->wic_guid, sizeof(*guid));
}

enum d3dx_pixel_format_id wic_guid_to_d3dx_pixel_format_id(const GUID *guid)
{
    struct d3dx_wic_pixel_format *format;

    if ((format = bsearch(guid, wic_pixel_formats, ARRAY_SIZE(wic_pixel_formats), sizeof(*format),
            d3dx_wic_pixel_format_guid_compare)))
        return format->format_id;
    return D3DX_PIXEL_FORMAT_COUNT;
}

const GUID *d3dx_pixel_format_id_to_wic_guid(enum d3dx_pixel_format_id format)
{
    uint32_t i;

    /*
     * There are multiple indexed WIC pixel formats, but the only one that
     * makes sense for this case is 8bpp.
     */
    if (format == D3DX_PIXEL_FORMAT_P8_UINT)
        return &GUID_WICPixelFormat8bppIndexed;

    for (i = 0; i < ARRAY_SIZE(wic_pixel_formats); i++)
    {
        if (wic_pixel_formats[i].format_id == format)
            return wic_pixel_formats[i].wic_guid;
    }

    return NULL;
}

static enum d3dx_pixel_format_id dds_fourcc_to_d3dx_pixel_format(uint32_t fourcc)
{
    static const struct {
        uint32_t fourcc;
        enum d3dx_pixel_format_id format;
    } fourcc_formats[] = {
        { MAKEFOURCC('U','Y','V','Y'),     D3DX_PIXEL_FORMAT_UYVY },
        { MAKEFOURCC('Y','U','Y','2'),     D3DX_PIXEL_FORMAT_YUY2 },
        { MAKEFOURCC('R','G','B','G'),     D3DX_PIXEL_FORMAT_R8G8_B8G8_UNORM },
        { MAKEFOURCC('G','R','G','B'),     D3DX_PIXEL_FORMAT_G8R8_G8B8_UNORM },
        { MAKEFOURCC('D','X','T','1'),     D3DX_PIXEL_FORMAT_DXT1_UNORM },
        { MAKEFOURCC('D','X','T','2'),     D3DX_PIXEL_FORMAT_DXT2_UNORM },
        { MAKEFOURCC('D','X','T','3'),     D3DX_PIXEL_FORMAT_DXT3_UNORM },
        { MAKEFOURCC('D','X','T','4'),     D3DX_PIXEL_FORMAT_DXT4_UNORM },
        { MAKEFOURCC('D','X','T','5'),     D3DX_PIXEL_FORMAT_DXT5_UNORM },
        { MAKEFOURCC('B','C','4','U'),     D3DX_PIXEL_FORMAT_BC4_UNORM },
        { MAKEFOURCC('B','C','4','S'),     D3DX_PIXEL_FORMAT_BC4_SNORM },
        { MAKEFOURCC('B','C','5','U'),     D3DX_PIXEL_FORMAT_BC5_UNORM },
        { MAKEFOURCC('B','C','5','S'),     D3DX_PIXEL_FORMAT_BC5_SNORM },
        /* These aren't actually fourcc values, they're just D3DFMT values. */
        { 0x24, /* D3DFMT_A16B16G16R16 */  D3DX_PIXEL_FORMAT_R16G16B16A16_UNORM },
        { 0x6e, /* D3DFMT_Q16W16V16U16 */  D3DX_PIXEL_FORMAT_U16V16W16Q16_SNORM },
        { 0x6f, /* D3DFMT_R16F */          D3DX_PIXEL_FORMAT_R16_FLOAT },
        { 0x70, /* D3DFMT_G16R16F */       D3DX_PIXEL_FORMAT_R16G16_FLOAT },
        { 0x71, /* D3DFMT_A16B16G16R16F */ D3DX_PIXEL_FORMAT_R16G16B16A16_FLOAT },
        { 0x72, /* D3DFMT_R32F */          D3DX_PIXEL_FORMAT_R32_FLOAT },
        { 0x73, /* D3DFMT_G32R32F */       D3DX_PIXEL_FORMAT_R32G32_FLOAT },
        { 0x74, /* D3DFMT_A32B32G32R32F */ D3DX_PIXEL_FORMAT_R32G32B32A32_FLOAT },
    };
    uint32_t i;

    for (i = 0; i < ARRAY_SIZE(fourcc_formats); ++i)
    {
        if (fourcc_formats[i].fourcc == fourcc)
            return fourcc_formats[i].format;
    }

    WARN("Unknown FourCC %s.\n", debugstr_fourcc(fourcc));
    return D3DX_PIXEL_FORMAT_COUNT;
}

static const struct {
    uint32_t bpp;
    uint32_t rmask;
    uint32_t gmask;
    uint32_t bmask;
    uint32_t amask;
    enum d3dx_pixel_format_id format;
} rgb_pixel_formats[] = {
    {  8, 0xe0,       0x1c,       0x03,       0x00,       D3DX_PIXEL_FORMAT_B2G3R3_UNORM },
    { 16, 0xf800,     0x07e0,     0x001f,     0x0000,     D3DX_PIXEL_FORMAT_B5G6R5_UNORM },
    { 16, 0x7c00,     0x03e0,     0x001f,     0x8000,     D3DX_PIXEL_FORMAT_B5G5R5A1_UNORM },
    { 16, 0x7c00,     0x03e0,     0x001f,     0x0000,     D3DX_PIXEL_FORMAT_B5G5R5X1_UNORM },
    { 16, 0x0f00,     0x00f0,     0x000f,     0xf000,     D3DX_PIXEL_FORMAT_B4G4R4A4_UNORM },
    { 16, 0x0f00,     0x00f0,     0x000f,     0x0000,     D3DX_PIXEL_FORMAT_B4G4R4X4_UNORM },
    { 16, 0x00e0,     0x001c,     0x0003,     0xff00,     D3DX_PIXEL_FORMAT_B2G3R3A8_UNORM },
    { 24, 0xff0000,   0x00ff00,   0x0000ff,   0x000000,   D3DX_PIXEL_FORMAT_B8G8R8_UNORM },
    { 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000, D3DX_PIXEL_FORMAT_B8G8R8A8_UNORM },
    { 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0x00000000, D3DX_PIXEL_FORMAT_B8G8R8X8_UNORM },
    { 32, 0x3ff00000, 0x000ffc00, 0x000003ff, 0xc0000000, D3DX_PIXEL_FORMAT_R10G10B10A2_UNORM },
    { 32, 0x000003ff, 0x000ffc00, 0x3ff00000, 0xc0000000, D3DX_PIXEL_FORMAT_B10G10R10A2_UNORM },
    { 32, 0x0000ffff, 0xffff0000, 0x00000000, 0x00000000, D3DX_PIXEL_FORMAT_R16G16_UNORM },
    { 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000, D3DX_PIXEL_FORMAT_R8G8B8A8_UNORM },
    { 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0x00000000, D3DX_PIXEL_FORMAT_R8G8B8X8_UNORM },
};

static enum d3dx_pixel_format_id dds_rgb_to_d3dx_pixel_format(const struct dds_pixel_format *pixel_format)
{
    uint32_t i;

    for (i = 0; i < ARRAY_SIZE(rgb_pixel_formats); i++)
    {
        if (rgb_pixel_formats[i].bpp == pixel_format->bpp
            && rgb_pixel_formats[i].rmask == pixel_format->rmask
            && rgb_pixel_formats[i].gmask == pixel_format->gmask
            && rgb_pixel_formats[i].bmask == pixel_format->bmask)
        {
            if ((pixel_format->flags & DDS_PF_ALPHA) && rgb_pixel_formats[i].amask == pixel_format->amask)
                return rgb_pixel_formats[i].format;
            if (rgb_pixel_formats[i].amask == 0)
                return rgb_pixel_formats[i].format;
        }
    }

    WARN("Unknown RGB pixel format (r %#lx, g %#lx, b %#lx, a %#lx).\n",
            pixel_format->rmask, pixel_format->gmask, pixel_format->bmask, pixel_format->amask);
    return D3DX_PIXEL_FORMAT_COUNT;
}

static enum d3dx_pixel_format_id dds_luminance_to_d3dx_pixel_format(const struct dds_pixel_format *pixel_format)
{
    if (pixel_format->bpp == 8)
    {
        if (pixel_format->rmask == 0xff)
            return D3DX_PIXEL_FORMAT_L8_UNORM;
        if ((pixel_format->flags & DDS_PF_ALPHA) && pixel_format->rmask == 0x0f && pixel_format->amask == 0xf0)
            return D3DX_PIXEL_FORMAT_L4A4_UNORM;
    }
    if (pixel_format->bpp == 16)
    {
        if (pixel_format->rmask == 0xffff)
            return D3DX_PIXEL_FORMAT_L16_UNORM;
        if ((pixel_format->flags & DDS_PF_ALPHA) && pixel_format->rmask == 0x00ff && pixel_format->amask == 0xff00)
            return D3DX_PIXEL_FORMAT_L8A8_UNORM;
    }

    WARN("Unknown luminance pixel format (bpp %lu, l %#lx, a %#lx).\n",
            pixel_format->bpp, pixel_format->rmask, pixel_format->amask);
    return D3DX_PIXEL_FORMAT_COUNT;
}

static enum d3dx_pixel_format_id dds_alpha_to_d3dx_pixel_format(const struct dds_pixel_format *pixel_format)
{
    if (pixel_format->bpp == 8 && pixel_format->amask == 0xff)
        return D3DX_PIXEL_FORMAT_A8_UNORM;

    WARN("Unknown alpha pixel format (bpp %lu, a %#lx).\n", pixel_format->bpp, pixel_format->rmask);
    return D3DX_PIXEL_FORMAT_COUNT;
}

static enum d3dx_pixel_format_id dds_indexed_to_d3dx_pixel_format(const struct dds_pixel_format *pixel_format)
{
    if (pixel_format->bpp == 8)
        return D3DX_PIXEL_FORMAT_P8_UINT;
    if (pixel_format->bpp == 16 && pixel_format->amask == 0xff00)
        return D3DX_PIXEL_FORMAT_P8_UINT_A8_UNORM;

    WARN("Unknown indexed pixel format (bpp %lu).\n", pixel_format->bpp);
    return D3DX_PIXEL_FORMAT_COUNT;
}

static enum d3dx_pixel_format_id dds_bump_to_d3dx_pixel_format(const struct dds_pixel_format *pixel_format)
{
    if (pixel_format->bpp == 16 && pixel_format->rmask == 0x00ff && pixel_format->gmask == 0xff00)
        return D3DX_PIXEL_FORMAT_U8V8_SNORM;
    if (pixel_format->bpp == 32 && pixel_format->rmask == 0x0000ffff && pixel_format->gmask == 0xffff0000)
        return D3DX_PIXEL_FORMAT_U16V16_SNORM;
    if (pixel_format->bpp == 32 && pixel_format->rmask == 0x000000ff && pixel_format->gmask == 0x0000ff00
            && pixel_format->bmask == 0x00ff0000 && pixel_format->amask == 0xff000000)
        return D3DX_PIXEL_FORMAT_U8V8W8Q8_SNORM;

    WARN("Unknown bump pixel format (bpp %lu, r %#lx, g %#lx, b %#lx, a %#lx).\n", pixel_format->bpp,
            pixel_format->rmask, pixel_format->gmask, pixel_format->bmask, pixel_format->amask);
    return D3DX_PIXEL_FORMAT_COUNT;
}

static enum d3dx_pixel_format_id dds_bump_luminance_to_d3dx_pixel_format(const struct dds_pixel_format *pixel_format)
{
    if (pixel_format->bpp == 32 && pixel_format->rmask == 0x000000ff && pixel_format->gmask == 0x0000ff00
            && pixel_format->bmask == 0x00ff0000)
        return D3DX_PIXEL_FORMAT_U8V8_SNORM_L8X8_UNORM;

    WARN("Unknown bump pixel format (bpp %lu, r %#lx, g %#lx, b %#lx, a %#lx).\n", pixel_format->bpp,
            pixel_format->rmask, pixel_format->gmask, pixel_format->bmask, pixel_format->amask);
    return D3DX_PIXEL_FORMAT_COUNT;
}

static enum d3dx_pixel_format_id dds_pixel_format_to_d3dx_pixel_format(const struct dds_pixel_format *pixel_format)
{
    TRACE("pixel_format: size %lu, flags %#lx, fourcc %#lx, bpp %lu.\n", pixel_format->size,
            pixel_format->flags, pixel_format->fourcc, pixel_format->bpp);
    TRACE("rmask %#lx, gmask %#lx, bmask %#lx, amask %#lx.\n", pixel_format->rmask, pixel_format->gmask,
            pixel_format->bmask, pixel_format->amask);

    if (pixel_format->flags & DDS_PF_FOURCC)
        return dds_fourcc_to_d3dx_pixel_format(pixel_format->fourcc);
    if (pixel_format->flags & DDS_PF_INDEXED)
        return dds_indexed_to_d3dx_pixel_format(pixel_format);
    if (pixel_format->flags & DDS_PF_RGB)
        return dds_rgb_to_d3dx_pixel_format(pixel_format);
    if (pixel_format->flags & DDS_PF_LUMINANCE)
        return dds_luminance_to_d3dx_pixel_format(pixel_format);
    if (pixel_format->flags & DDS_PF_ALPHA_ONLY)
        return dds_alpha_to_d3dx_pixel_format(pixel_format);
    if (pixel_format->flags & DDS_PF_BUMPDUDV)
        return dds_bump_to_d3dx_pixel_format(pixel_format);
    if (pixel_format->flags & DDS_PF_BUMPLUMINANCE)
        return dds_bump_luminance_to_d3dx_pixel_format(pixel_format);

    WARN("Unknown pixel format (flags %#lx, fourcc %#lx, bpp %lu, r %#lx, g %#lx, b %#lx, a %#lx).\n",
        pixel_format->flags, pixel_format->fourcc, pixel_format->bpp,
        pixel_format->rmask, pixel_format->gmask, pixel_format->bmask, pixel_format->amask);
    return D3DX_PIXEL_FORMAT_COUNT;
}

HRESULT d3dx_pixel_format_to_dds_pixel_format(struct dds_pixel_format *pixel_format,
        enum d3dx_pixel_format_id format)
{
    uint32_t i;

    memset(pixel_format, 0, sizeof(*pixel_format));
    pixel_format->size = sizeof(*pixel_format);
    for (i = 0; i < ARRAY_SIZE(rgb_pixel_formats); i++)
    {
        if (rgb_pixel_formats[i].format == format)
        {
            pixel_format->flags |= DDS_PF_RGB;
            pixel_format->bpp = rgb_pixel_formats[i].bpp;
            pixel_format->rmask = rgb_pixel_formats[i].rmask;
            pixel_format->gmask = rgb_pixel_formats[i].gmask;
            pixel_format->bmask = rgb_pixel_formats[i].bmask;
            pixel_format->amask = rgb_pixel_formats[i].amask;
            if (pixel_format->amask) pixel_format->flags |= DDS_PF_ALPHA;
            return D3D_OK;
        }
    }

    WARN("Unknown pixel format %#x.\n", format);
    return E_NOTIMPL;

}

enum d3dx_pixel_format_id d3dx_pixel_format_id_from_dxgi_format(uint32_t format)
{
    switch (format)
    {
    case DXGI_FORMAT_R8G8B8A8_UNORM:           return D3DX_PIXEL_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:      return D3DX_PIXEL_FORMAT_R8G8B8A8_UNORM_SRGB;
    case DXGI_FORMAT_B8G8R8A8_UNORM:           return D3DX_PIXEL_FORMAT_B8G8R8A8_UNORM;
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:      return D3DX_PIXEL_FORMAT_B8G8R8A8_UNORM_SRGB;
    case DXGI_FORMAT_B8G8R8X8_UNORM:           return D3DX_PIXEL_FORMAT_B8G8R8X8_UNORM;
    case DXGI_FORMAT_B5G6R5_UNORM:             return D3DX_PIXEL_FORMAT_B5G6R5_UNORM;
    case DXGI_FORMAT_B5G5R5A1_UNORM:           return D3DX_PIXEL_FORMAT_B5G5R5A1_UNORM;
    case DXGI_FORMAT_B4G4R4A4_UNORM:           return D3DX_PIXEL_FORMAT_B4G4R4A4_UNORM;
    case DXGI_FORMAT_R10G10B10A2_UNORM:        return D3DX_PIXEL_FORMAT_R10G10B10A2_UNORM;
    case DXGI_FORMAT_R16G16B16A16_UNORM:       return D3DX_PIXEL_FORMAT_R16G16B16A16_UNORM;
    case DXGI_FORMAT_R8_UNORM:                 return D3DX_PIXEL_FORMAT_R8_UNORM;
    case DXGI_FORMAT_R8_SNORM:                 return D3DX_PIXEL_FORMAT_R8_SNORM;
    case DXGI_FORMAT_R8G8_UNORM:               return D3DX_PIXEL_FORMAT_R8G8_UNORM;
    case DXGI_FORMAT_R16_UNORM:                return D3DX_PIXEL_FORMAT_R16_UNORM;
    case DXGI_FORMAT_R16G16_UNORM:             return D3DX_PIXEL_FORMAT_R16G16_UNORM;
    case DXGI_FORMAT_A8_UNORM:                 return D3DX_PIXEL_FORMAT_A8_UNORM;
    case DXGI_FORMAT_R16_FLOAT:                return D3DX_PIXEL_FORMAT_R16_FLOAT;
    case DXGI_FORMAT_R16G16_FLOAT:             return D3DX_PIXEL_FORMAT_R16G16_FLOAT;
    case DXGI_FORMAT_R16G16B16A16_FLOAT:       return D3DX_PIXEL_FORMAT_R16G16B16A16_FLOAT;
    case DXGI_FORMAT_R32_FLOAT:                return D3DX_PIXEL_FORMAT_R32_FLOAT;
    case DXGI_FORMAT_R32G32_FLOAT:             return D3DX_PIXEL_FORMAT_R32G32_FLOAT;
    case DXGI_FORMAT_R32G32B32_FLOAT:          return D3DX_PIXEL_FORMAT_R32G32B32_FLOAT;
    case DXGI_FORMAT_R32G32B32A32_FLOAT:       return D3DX_PIXEL_FORMAT_R32G32B32A32_FLOAT;
    case DXGI_FORMAT_G8R8_G8B8_UNORM:          return D3DX_PIXEL_FORMAT_G8R8_G8B8_UNORM;
    case DXGI_FORMAT_R8G8_B8G8_UNORM:          return D3DX_PIXEL_FORMAT_R8G8_B8G8_UNORM;
    case DXGI_FORMAT_BC1_UNORM:                return D3DX_PIXEL_FORMAT_DXT1_UNORM;
    case DXGI_FORMAT_BC1_UNORM_SRGB:           return D3DX_PIXEL_FORMAT_BC1_UNORM_SRGB;
    case DXGI_FORMAT_BC2_UNORM:                return D3DX_PIXEL_FORMAT_DXT3_UNORM;
    case DXGI_FORMAT_BC2_UNORM_SRGB:           return D3DX_PIXEL_FORMAT_BC2_UNORM_SRGB;
    case DXGI_FORMAT_BC3_UNORM:                return D3DX_PIXEL_FORMAT_DXT5_UNORM;
    case DXGI_FORMAT_BC3_UNORM_SRGB:           return D3DX_PIXEL_FORMAT_BC3_UNORM_SRGB;
    case DXGI_FORMAT_BC4_UNORM:                return D3DX_PIXEL_FORMAT_BC4_UNORM;
    case DXGI_FORMAT_BC4_SNORM:                return D3DX_PIXEL_FORMAT_BC4_SNORM;
    case DXGI_FORMAT_BC5_UNORM:                return D3DX_PIXEL_FORMAT_BC5_UNORM;
    case DXGI_FORMAT_BC5_SNORM:                return D3DX_PIXEL_FORMAT_BC5_SNORM;
    case DXGI_FORMAT_R8G8B8A8_SNORM:           return D3DX_PIXEL_FORMAT_R8G8B8A8_SNORM;
    case DXGI_FORMAT_R8G8_SNORM:               return D3DX_PIXEL_FORMAT_R8G8_SNORM;
    case DXGI_FORMAT_R16G16_SNORM:             return D3DX_PIXEL_FORMAT_R16G16_SNORM;
    case DXGI_FORMAT_R16G16B16A16_SNORM:       return D3DX_PIXEL_FORMAT_R16G16B16A16_SNORM;

    default:
        FIXME("Unhandled DXGI format %#x.\n", format);
        return D3DX_PIXEL_FORMAT_COUNT;
    }
}

/*
 * These are mappings from legacy DDS header formats to DXGI formats. Some
 * don't map at all, and some only map to the default format.
 */
DXGI_FORMAT dxgi_format_from_dds_d3dx_pixel_format_id(enum d3dx_pixel_format_id format)
{
    switch (format)
    {
    case D3DX_PIXEL_FORMAT_P8_UINT:                  return DXGI_FORMAT_R8G8B8A8_UNORM;
    case D3DX_PIXEL_FORMAT_P8_UINT_A8_UNORM:         return DXGI_FORMAT_R8G8B8A8_UNORM;
    case D3DX_PIXEL_FORMAT_R8G8B8A8_UNORM:           return DXGI_FORMAT_R8G8B8A8_UNORM;
    case D3DX_PIXEL_FORMAT_R8G8B8X8_UNORM:           return DXGI_FORMAT_R8G8B8A8_UNORM;
    case D3DX_PIXEL_FORMAT_B8G8R8_UNORM:             return DXGI_FORMAT_R8G8B8A8_UNORM;
    case D3DX_PIXEL_FORMAT_B8G8R8A8_UNORM:           return DXGI_FORMAT_R8G8B8A8_UNORM;
    case D3DX_PIXEL_FORMAT_B8G8R8X8_UNORM:           return DXGI_FORMAT_R8G8B8A8_UNORM;
    case D3DX_PIXEL_FORMAT_B5G6R5_UNORM:             return DXGI_FORMAT_R8G8B8A8_UNORM;
    case D3DX_PIXEL_FORMAT_B5G5R5X1_UNORM:           return DXGI_FORMAT_R8G8B8A8_UNORM;
    case D3DX_PIXEL_FORMAT_B5G5R5A1_UNORM:           return DXGI_FORMAT_R8G8B8A8_UNORM;
    case D3DX_PIXEL_FORMAT_B2G3R3_UNORM:             return DXGI_FORMAT_R8G8B8A8_UNORM;
    case D3DX_PIXEL_FORMAT_B2G3R3A8_UNORM:           return DXGI_FORMAT_R8G8B8A8_UNORM;
    case D3DX_PIXEL_FORMAT_B4G4R4A4_UNORM:           return DXGI_FORMAT_R8G8B8A8_UNORM;
    case D3DX_PIXEL_FORMAT_B4G4R4X4_UNORM:           return DXGI_FORMAT_R8G8B8A8_UNORM;
    case D3DX_PIXEL_FORMAT_L8A8_UNORM:               return DXGI_FORMAT_R8G8B8A8_UNORM;
    case D3DX_PIXEL_FORMAT_L4A4_UNORM:               return DXGI_FORMAT_R8G8B8A8_UNORM;
    case D3DX_PIXEL_FORMAT_L8_UNORM:                 return DXGI_FORMAT_R8G8B8A8_UNORM;

    /* B10G10R10A2 doesn't exist in DXGI, both map to R10G10B10A2. */
    case D3DX_PIXEL_FORMAT_B10G10R10A2_UNORM:
    case D3DX_PIXEL_FORMAT_R10G10B10A2_UNORM:        return DXGI_FORMAT_R10G10B10A2_UNORM;

    case D3DX_PIXEL_FORMAT_U16V16W16Q16_SNORM:       return DXGI_FORMAT_R16G16B16A16_SNORM;
    case D3DX_PIXEL_FORMAT_L16_UNORM:                return DXGI_FORMAT_R16G16B16A16_UNORM;
    case D3DX_PIXEL_FORMAT_R16G16B16A16_UNORM:       return DXGI_FORMAT_R16G16B16A16_UNORM;
    case D3DX_PIXEL_FORMAT_R16G16_UNORM:             return DXGI_FORMAT_R16G16_UNORM;
    case D3DX_PIXEL_FORMAT_A8_UNORM:                 return DXGI_FORMAT_A8_UNORM;
    case D3DX_PIXEL_FORMAT_R16_FLOAT:                return DXGI_FORMAT_R16_FLOAT;
    case D3DX_PIXEL_FORMAT_R16G16_FLOAT:             return DXGI_FORMAT_R16G16_FLOAT;
    case D3DX_PIXEL_FORMAT_R16G16B16A16_FLOAT:       return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case D3DX_PIXEL_FORMAT_R32_FLOAT:                return DXGI_FORMAT_R32_FLOAT;
    case D3DX_PIXEL_FORMAT_R32G32_FLOAT:             return DXGI_FORMAT_R32G32_FLOAT;
    case D3DX_PIXEL_FORMAT_R32G32B32A32_FLOAT:       return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case D3DX_PIXEL_FORMAT_G8R8_G8B8_UNORM:          return DXGI_FORMAT_G8R8_G8B8_UNORM;
    case D3DX_PIXEL_FORMAT_R8G8_B8G8_UNORM:          return DXGI_FORMAT_R8G8_B8G8_UNORM;

    case D3DX_PIXEL_FORMAT_DXT1_UNORM:               return DXGI_FORMAT_BC1_UNORM;
    case D3DX_PIXEL_FORMAT_DXT2_UNORM:               return DXGI_FORMAT_BC2_UNORM;
    case D3DX_PIXEL_FORMAT_DXT3_UNORM:               return DXGI_FORMAT_BC2_UNORM;
    case D3DX_PIXEL_FORMAT_DXT4_UNORM:               return DXGI_FORMAT_BC3_UNORM;
    case D3DX_PIXEL_FORMAT_DXT5_UNORM:               return DXGI_FORMAT_BC3_UNORM;
    case D3DX_PIXEL_FORMAT_BC4_UNORM:                return DXGI_FORMAT_BC4_UNORM;
    case D3DX_PIXEL_FORMAT_BC4_SNORM:                return DXGI_FORMAT_BC4_SNORM;
    case D3DX_PIXEL_FORMAT_BC5_UNORM:                return DXGI_FORMAT_BC5_UNORM;
    case D3DX_PIXEL_FORMAT_BC5_SNORM:                return DXGI_FORMAT_BC5_SNORM;

    /* These formats are known and explicitly unsupported. */
    case D3DX_PIXEL_FORMAT_U8V8W8Q8_SNORM:
    case D3DX_PIXEL_FORMAT_U8V8_SNORM:
    case D3DX_PIXEL_FORMAT_U16V16_SNORM:
    case D3DX_PIXEL_FORMAT_U8V8_SNORM_L8X8_UNORM:
    case D3DX_PIXEL_FORMAT_U10V10W10_SNORM_A2_UNORM:
    case D3DX_PIXEL_FORMAT_UYVY:
    case D3DX_PIXEL_FORMAT_YUY2:
        return DXGI_FORMAT_UNKNOWN;

    default:
        FIXME("Unknown d3dx_pixel_format_id %#x.\n", format);
        return DXGI_FORMAT_UNKNOWN;
    }
}

DXGI_FORMAT dxgi_format_from_d3dx_pixel_format_id(enum d3dx_pixel_format_id format)
{
    switch (format)
    {
    case D3DX_PIXEL_FORMAT_R8G8B8A8_UNORM:          return DXGI_FORMAT_R8G8B8A8_UNORM;
    case D3DX_PIXEL_FORMAT_R8G8B8A8_UNORM_SRGB:     return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case D3DX_PIXEL_FORMAT_B8G8R8A8_UNORM:          return DXGI_FORMAT_B8G8R8A8_UNORM;
    case D3DX_PIXEL_FORMAT_B8G8R8A8_UNORM_SRGB:     return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    case D3DX_PIXEL_FORMAT_B8G8R8X8_UNORM:          return DXGI_FORMAT_B8G8R8X8_UNORM;
    case D3DX_PIXEL_FORMAT_R10G10B10A2_UNORM:       return DXGI_FORMAT_R10G10B10A2_UNORM;
    case D3DX_PIXEL_FORMAT_R16G16B16A16_UNORM:      return DXGI_FORMAT_R16G16B16A16_UNORM;
    case D3DX_PIXEL_FORMAT_R8_UNORM:                return DXGI_FORMAT_R8_UNORM;
    case D3DX_PIXEL_FORMAT_R8_SNORM:                return DXGI_FORMAT_R8_SNORM;
    case D3DX_PIXEL_FORMAT_R8G8_UNORM:              return DXGI_FORMAT_R8G8_UNORM;
    case D3DX_PIXEL_FORMAT_R16_UNORM:               return DXGI_FORMAT_R16_UNORM;
    case D3DX_PIXEL_FORMAT_R16G16_UNORM:            return DXGI_FORMAT_R16G16_UNORM;
    case D3DX_PIXEL_FORMAT_A8_UNORM:                return DXGI_FORMAT_A8_UNORM;
    case D3DX_PIXEL_FORMAT_R16_FLOAT:               return DXGI_FORMAT_R16_FLOAT;
    case D3DX_PIXEL_FORMAT_R16G16_FLOAT:            return DXGI_FORMAT_R16G16_FLOAT;
    case D3DX_PIXEL_FORMAT_R16G16B16A16_FLOAT:      return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case D3DX_PIXEL_FORMAT_R32_FLOAT:               return DXGI_FORMAT_R32_FLOAT;
    case D3DX_PIXEL_FORMAT_R32G32_FLOAT:            return DXGI_FORMAT_R32G32_FLOAT;
    case D3DX_PIXEL_FORMAT_R32G32B32_FLOAT:         return DXGI_FORMAT_R32G32B32_FLOAT;
    case D3DX_PIXEL_FORMAT_R32G32B32A32_FLOAT:      return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case D3DX_PIXEL_FORMAT_G8R8_G8B8_UNORM:         return DXGI_FORMAT_G8R8_G8B8_UNORM;
    case D3DX_PIXEL_FORMAT_R8G8_B8G8_UNORM:         return DXGI_FORMAT_R8G8_B8G8_UNORM;
    case D3DX_PIXEL_FORMAT_DXT1_UNORM:              return DXGI_FORMAT_BC1_UNORM;
    case D3DX_PIXEL_FORMAT_BC1_UNORM_SRGB:          return DXGI_FORMAT_BC1_UNORM_SRGB;
    case D3DX_PIXEL_FORMAT_DXT3_UNORM:              return DXGI_FORMAT_BC2_UNORM;
    case D3DX_PIXEL_FORMAT_BC2_UNORM_SRGB:          return DXGI_FORMAT_BC2_UNORM_SRGB;
    case D3DX_PIXEL_FORMAT_DXT5_UNORM:              return DXGI_FORMAT_BC3_UNORM;
    case D3DX_PIXEL_FORMAT_BC3_UNORM_SRGB:          return DXGI_FORMAT_BC3_UNORM_SRGB;
    case D3DX_PIXEL_FORMAT_BC4_UNORM:               return DXGI_FORMAT_BC4_UNORM;
    case D3DX_PIXEL_FORMAT_BC4_SNORM:               return DXGI_FORMAT_BC4_SNORM;
    case D3DX_PIXEL_FORMAT_BC5_UNORM:               return DXGI_FORMAT_BC5_UNORM;
    case D3DX_PIXEL_FORMAT_BC5_SNORM:               return DXGI_FORMAT_BC5_SNORM;
    case D3DX_PIXEL_FORMAT_R16G16B16A16_SNORM:      return DXGI_FORMAT_R16G16B16A16_SNORM;
    case D3DX_PIXEL_FORMAT_R8G8B8A8_SNORM:          return DXGI_FORMAT_R8G8B8A8_SNORM;
    case D3DX_PIXEL_FORMAT_R8G8_SNORM:              return DXGI_FORMAT_R8G8_SNORM;
    case D3DX_PIXEL_FORMAT_R16G16_SNORM:            return DXGI_FORMAT_R16G16_SNORM;

    /*
     * These have DXGI_FORMAT equivalents, but are explicitly unsupported on
     * d3dx10/d3dx11.
     */
    case D3DX_PIXEL_FORMAT_B5G6R5_UNORM:
    case D3DX_PIXEL_FORMAT_B5G5R5A1_UNORM:
    case D3DX_PIXEL_FORMAT_B4G4R4A4_UNORM:
        return DXGI_FORMAT_UNKNOWN;

    default:
        FIXME("Unhandled d3dx_pixel_format_id %#x.\n", format);
        return DXGI_FORMAT_UNKNOWN;
    }
}

void d3dx_get_next_mip_level_size(struct volume *size)
{
    size->width  = max(size->width  / 2, 1);
    size->height = max(size->height / 2, 1);
    size->depth  = max(size->depth  / 2, 1);
}

void d3dx_get_mip_level_size(struct volume *size, uint32_t level)
{
    uint32_t i;

    for (i = 0; i < level; ++i)
        d3dx_get_next_mip_level_size(size);
}

uint32_t d3dx_get_max_mip_levels_for_size(uint32_t width, uint32_t height, uint32_t depth)
{
    struct volume tmp = { width, height, depth };
    uint32_t mip_levels = 1;

    while (!(tmp.width == 1 && tmp.height == 1 && tmp.depth == 1))
    {
        d3dx_get_next_mip_level_size(&tmp);
        mip_levels++;
    }

    return mip_levels;
}

static const char *debug_volume(const struct volume *volume)
{
    if (!volume)
        return "(null)";
    return wine_dbg_sprintf("(%ux%ux%u)", volume->width, volume->height, volume->depth);
}

HRESULT d3dx_calculate_pixels_size(enum d3dx_pixel_format_id format, uint32_t width, uint32_t height,
    uint32_t *pitch, uint32_t *size)
{
    const struct pixel_format_desc *format_desc = get_d3dx_pixel_format_info(format);

    *pitch = *size = 0;
    if (is_unknown_format(format_desc))
        return E_NOTIMPL;

    if (format_desc->block_width != 1 || format_desc->block_height != 1)
    {
        *pitch = format_desc->block_byte_count
            * max(1, (width + format_desc->block_width - 1) / format_desc->block_width);
        *size = *pitch
            * max(1, (height + format_desc->block_height - 1) / format_desc->block_height);
    }
    else
    {
        *pitch = width * format_desc->bytes_per_pixel;
        *size = *pitch * height;
    }

    return D3D_OK;
}

uint32_t d3dx_calculate_layer_pixels_size(enum d3dx_pixel_format_id format, uint32_t width, uint32_t height, uint32_t depth,
        uint32_t mip_levels)
{
    uint32_t layer_size, row_pitch, slice_pitch, i;
    struct volume dims = { width, height, depth };

    layer_size = 0;
    for (i = 0; i < mip_levels; ++i)
    {
        if (FAILED(d3dx_calculate_pixels_size(format, dims.width, dims.height, &row_pitch, &slice_pitch)))
            return 0;
        layer_size += slice_pitch * dims.depth;
        d3dx_get_next_mip_level_size(&dims);
    }

    return layer_size;
}

/* These defines match D3D10/D3D11 values. */
#define DDS_RESOURCE_MISC_TEXTURECUBE 0x04
#define DDS_RESOURCE_DIMENSION_TEXTURE1D 2
#define DDS_RESOURCE_DIMENSION_TEXTURE2D 3
#define DDS_RESOURCE_DIMENSION_TEXTURE3D 4
struct dds_header_dxt10
{
    uint32_t dxgi_format;
    uint32_t resource_dimension;
    uint32_t misc_flags;
    uint32_t array_size;
    uint32_t misc_flags2;
};

static enum d3dx_resource_type dxt10_resource_dimension_to_d3dx_resource_type(uint32_t resource_dimension)
{
    switch (resource_dimension)
    {
    case DDS_RESOURCE_DIMENSION_TEXTURE1D: return D3DX_RESOURCE_TYPE_TEXTURE_1D;
    case DDS_RESOURCE_DIMENSION_TEXTURE2D: return D3DX_RESOURCE_TYPE_TEXTURE_2D;
    case DDS_RESOURCE_DIMENSION_TEXTURE3D: return D3DX_RESOURCE_TYPE_TEXTURE_3D;
    default:
        break;
    }

    FIXME("Unhandled DXT10 resource dimension value %u.\n", resource_dimension);
    return D3DX_RESOURCE_TYPE_UNKNOWN;
}

static BOOL has_extended_header(const struct dds_header *header)
{
    return (header->pixel_format.flags & DDS_PF_FOURCC) &&
           (header->pixel_format.fourcc == MAKEFOURCC('D', 'X', '1', '0'));
}

#define DDS_PALETTE_SIZE (sizeof(PALETTEENTRY) * 256)
static HRESULT d3dx_initialize_image_from_dds(const void *src_data, uint32_t src_data_size,
        struct d3dx_image *image, uint32_t starting_mip_level, uint32_t flags)
{
    uint32_t expected_src_data_size, header_size;
    const struct dds_header *header = src_data;
    BOOL is_indexed_fmt;
    HRESULT hr;

    if (src_data_size < sizeof(*header) || header->pixel_format.size != sizeof(header->pixel_format))
        return D3DXERR_INVALIDDATA;

    TRACE("File type is DDS.\n");
    is_indexed_fmt = !!(header->pixel_format.flags & DDS_PF_INDEXED);
    header_size = is_indexed_fmt ? sizeof(*header) + DDS_PALETTE_SIZE : sizeof(*header);

    set_volume_struct(&image->size, header->width, header->height, 1);
    image->mip_levels = header->miplevels ? header->miplevels : 1;
    image->layer_count = 1;

    if (has_extended_header(header) && (flags & D3DX_IMAGE_SUPPORT_DXT10))
    {
        const struct dds_header_dxt10 *dxt10 = (const struct dds_header_dxt10 *)(((BYTE *)src_data) + header_size);

        header_size += sizeof(*dxt10);
        if (src_data_size < header_size)
            return D3DXERR_INVALIDDATA;

        if ((image->format = d3dx_pixel_format_id_from_dxgi_format(dxt10->dxgi_format)) == D3DX_PIXEL_FORMAT_COUNT)
            return D3DXERR_INVALIDDATA;

        if (dxt10->misc_flags2)
        {
            ERR("Invalid misc_flags2 field %#x.\n", dxt10->misc_flags2);
            return D3DXERR_INVALIDDATA;
        }

        image->image_file_format = D3DX_IMAGE_FILE_FORMAT_DDS_DXT10;
        image->size.depth = (header->flags & DDS_DEPTH) ? max(header->depth, 1) : 1;
        image->layer_count = max(1, dxt10->array_size);
        image->resource_type = dxt10_resource_dimension_to_d3dx_resource_type(dxt10->resource_dimension);
        if (dxt10->misc_flags & DDS_RESOURCE_MISC_TEXTURECUBE)
        {
            if (image->resource_type != D3DX_RESOURCE_TYPE_TEXTURE_2D)
                return D3DXERR_INVALIDDATA;
            image->resource_type = D3DX_RESOURCE_TYPE_CUBE_TEXTURE;
            image->layer_count *= 6;
        }
    }
    else
    {
        if ((image->format = dds_pixel_format_to_d3dx_pixel_format(&header->pixel_format)) == D3DX_PIXEL_FORMAT_COUNT)
            return D3DXERR_INVALIDDATA;

        image->image_file_format = D3DX_IMAGE_FILE_FORMAT_DDS;
        if (header->flags & DDS_DEPTH)
        {
            image->size.depth = max(header->depth, 1);
            image->resource_type = D3DX_RESOURCE_TYPE_TEXTURE_3D;
        }
        else if (header->caps2 & DDS_CAPS2_CUBEMAP)
        {
            if ((header->caps2 & DDS_CAPS2_CUBEMAP_ALL_FACES) != DDS_CAPS2_CUBEMAP_ALL_FACES)
            {
                WARN("Tried to load a partial cubemap DDS file.\n");
                return D3DXERR_INVALIDDATA;
            }

            image->layer_count = 6;
            image->resource_type = D3DX_RESOURCE_TYPE_CUBE_TEXTURE;
        }
        else
            image->resource_type = D3DX_RESOURCE_TYPE_TEXTURE_2D;
    }

    TRACE("Pixel format is %#x.\n", image->format);
    image->layer_pitch = d3dx_calculate_layer_pixels_size(image->format, image->size.width, image->size.height,
            image->size.depth, image->mip_levels);
    if (!image->layer_pitch)
        return D3DXERR_INVALIDDATA;

    expected_src_data_size = (image->layer_pitch * image->layer_count) + header_size;
    if (src_data_size < expected_src_data_size)
    {
        WARN("File is too short %u, expected at least %u bytes.\n", src_data_size, expected_src_data_size);
        /* d3dx10/d3dx11 do not validate the size of the pixels. */
        if (!(flags & D3DX_IMAGE_SUPPORT_DXT10))
            return D3DXERR_INVALIDDATA;
    }

    image->palette = (is_indexed_fmt) ? (PALETTEENTRY *)(((BYTE *)src_data) + sizeof(*header)) : NULL;
    image->pixels = ((BYTE *)src_data) + header_size;

    if (starting_mip_level && (image->mip_levels > 1))
    {
        uint32_t i, row_pitch, slice_pitch, initial_mip_levels;
        const struct volume initial_size = image->size;

        initial_mip_levels = image->mip_levels;
        for (i = 0; i < starting_mip_level; i++)
        {
            hr = d3dx_calculate_pixels_size(image->format, image->size.width, image->size.height, &row_pitch, &slice_pitch);
            if (FAILED(hr))
                return hr;

            image->pixels += slice_pitch * image->size.depth;
            d3dx_get_next_mip_level_size(&image->size);
            if (--image->mip_levels == 1)
                break;
        }

        TRACE("Requested starting mip level %u, actual starting mip level is %u (of %u total in image).\n",
                starting_mip_level, (initial_mip_levels - image->mip_levels), initial_mip_levels);
        TRACE("Original dimensions %s, new dimensions %s.\n", debug_volume(&initial_size), debug_volume(&image->size));
    }

    return D3D_OK;
}

static BOOL convert_dib_to_bmp(const void **data, unsigned int *size)
{
    ULONG header_size;
    ULONG count = 0;
    ULONG offset;
    BITMAPFILEHEADER *header;
    BYTE *new_data;
    UINT new_size;

    if ((*size < 4) || (*size < (header_size = *(ULONG*)*data)))
        return FALSE;

    if ((header_size == sizeof(BITMAPINFOHEADER)) ||
        (header_size == sizeof(BITMAPV4HEADER)) ||
        (header_size == sizeof(BITMAPV5HEADER)) ||
        (header_size == 64 /* sizeof(BITMAPCOREHEADER2) */))
    {
        /* All structures begin with the same memory layout as BITMAPINFOHEADER */
        BITMAPINFOHEADER *info_header = (BITMAPINFOHEADER*)*data;
        count = info_header->biClrUsed;

        if (!count && info_header->biBitCount <= 8)
            count = 1 << info_header->biBitCount;

        offset = sizeof(BITMAPFILEHEADER) + header_size + sizeof(RGBQUAD) * count;

        /* For BITMAPINFOHEADER with BI_BITFIELDS compression, there are 3 additional color masks after header */
        if ((info_header->biSize == sizeof(BITMAPINFOHEADER)) && (info_header->biCompression == BI_BITFIELDS))
            offset += 3 * sizeof(DWORD);
    }
    else if (header_size == sizeof(BITMAPCOREHEADER))
    {
        BITMAPCOREHEADER *core_header = (BITMAPCOREHEADER*)*data;

        if (core_header->bcBitCount <= 8)
            count = 1 << core_header->bcBitCount;

        offset = sizeof(BITMAPFILEHEADER) + header_size + sizeof(RGBTRIPLE) * count;
    }
    else
    {
        return FALSE;
    }

    TRACE("Converting DIB file to BMP\n");

    new_size = *size + sizeof(BITMAPFILEHEADER);
    new_data = malloc(new_size);
    CopyMemory(new_data + sizeof(BITMAPFILEHEADER), *data, *size);

    /* Add BMP header */
    header = (BITMAPFILEHEADER*)new_data;
    header->bfType = 0x4d42; /* BM */
    header->bfSize = new_size;
    header->bfReserved1 = 0;
    header->bfReserved2 = 0;
    header->bfOffBits = offset;

    /* Update input data */
    *data = new_data;
    *size = new_size;

    return TRUE;
}

/* windowscodecs always returns xRGB, but we should return ARGB if and only if
 * at least one pixel has a non-zero alpha component. */
static BOOL image_is_argb(IWICBitmapFrameDecode *frame, struct d3dx_image *image)
{
    unsigned int size, i;
    BYTE *buffer;
    HRESULT hr;

    if (image->format != D3DX_PIXEL_FORMAT_B8G8R8X8_UNORM || (image->image_file_format != D3DX_IMAGE_FILE_FORMAT_BMP
            && image->image_file_format != D3DX_IMAGE_FILE_FORMAT_TGA))
        return FALSE;

    size = image->size.width * image->size.height * 4;
    if (!(buffer = malloc(size)))
        return FALSE;

    if (FAILED(hr = IWICBitmapFrameDecode_CopyPixels(frame, NULL, image->size.width * 4, size, buffer)))
    {
        ERR("Failed to copy pixels, hr %#lx.\n", hr);
        free(buffer);
        return FALSE;
    }

    for (i = 0; i < image->size.width * image->size.height; ++i)
    {
        if (buffer[i * 4 + 3])
        {
            free(buffer);
            return TRUE;
        }
    }

    free(buffer);
    return FALSE;
}

#define FILE_FORMAT_FLAG_DX9 1
#define FILE_FORMAT_FLAG_DX10 2
struct d3dx_wic_file_format
{
    const GUID *wic_container_guid;
    enum d3dx_image_file_format d3dx_file_format;
    uint32_t file_format_flag;
};

/* Sorted by GUID. */
static const struct d3dx_wic_file_format file_formats[] =
{
    { &GUID_ContainerFormatGif,     D3DX_IMAGE_FILE_FORMAT_GIF,  (FILE_FORMAT_FLAG_DX10) },
    { &GUID_ContainerFormatTiff,    D3DX_IMAGE_FILE_FORMAT_TIFF, (FILE_FORMAT_FLAG_DX10) },
    { &GUID_ContainerFormatBmp,     D3DX_IMAGE_FILE_FORMAT_BMP,  (FILE_FORMAT_FLAG_DX9 | FILE_FORMAT_FLAG_DX10) },
    { &GUID_WineContainerFormatTga, D3DX_IMAGE_FILE_FORMAT_TGA,  (FILE_FORMAT_FLAG_DX9)  },
    { &GUID_ContainerFormatWmp,     D3DX_IMAGE_FILE_FORMAT_WMP,  (FILE_FORMAT_FLAG_DX10) },
    { &GUID_ContainerFormatJpeg,    D3DX_IMAGE_FILE_FORMAT_JPG,  (FILE_FORMAT_FLAG_DX9 | FILE_FORMAT_FLAG_DX10) },
    { &GUID_ContainerFormatPng,     D3DX_IMAGE_FILE_FORMAT_PNG,  (FILE_FORMAT_FLAG_DX9 | FILE_FORMAT_FLAG_DX10) },
};

static int __cdecl d3dx_wic_file_format_guid_compare(const void *a, const void *b)
{
    const struct d3dx_wic_file_format *format = b;
    const GUID *guid = a;

    return memcmp(guid, format->wic_container_guid, sizeof(*guid));
}

static enum d3dx_image_file_format wic_container_guid_to_d3dx_image_file_format(GUID *container_format, uint32_t flags)
{
    const uint32_t fmt_support_flag = (flags & D3DX_IMAGE_SUPPORT_DXT10) ? FILE_FORMAT_FLAG_DX10 : FILE_FORMAT_FLAG_DX9;
    struct d3dx_wic_file_format *format;

    if ((format = bsearch(container_format, file_formats, ARRAY_SIZE(file_formats), sizeof(*format),
            d3dx_wic_file_format_guid_compare)) && (format->file_format_flag & fmt_support_flag))
        return format->d3dx_file_format;
    return D3DX_IMAGE_FILE_FORMAT_FORCE_DWORD;
}

static const char *debug_d3dx_image_file_format(enum d3dx_image_file_format format)
{
    switch (format)
    {
#define FMT_TO_STR(format) case format: return #format
        FMT_TO_STR(D3DX_IMAGE_FILE_FORMAT_BMP);
        FMT_TO_STR(D3DX_IMAGE_FILE_FORMAT_JPG);
        FMT_TO_STR(D3DX_IMAGE_FILE_FORMAT_TGA);
        FMT_TO_STR(D3DX_IMAGE_FILE_FORMAT_PNG);
        FMT_TO_STR(D3DX_IMAGE_FILE_FORMAT_DDS);
        FMT_TO_STR(D3DX_IMAGE_FILE_FORMAT_PPM);
        FMT_TO_STR(D3DX_IMAGE_FILE_FORMAT_DIB);
        FMT_TO_STR(D3DX_IMAGE_FILE_FORMAT_HDR);
        FMT_TO_STR(D3DX_IMAGE_FILE_FORMAT_PFM);
        FMT_TO_STR(D3DX_IMAGE_FILE_FORMAT_TIFF);
        FMT_TO_STR(D3DX_IMAGE_FILE_FORMAT_GIF);
        FMT_TO_STR(D3DX_IMAGE_FILE_FORMAT_WMP);
#undef FMT_TO_STR
        default:
            return "unrecognized";
    }
}

static HRESULT d3dx_image_wic_frame_decode(struct d3dx_image *image,
        IWICImagingFactory *wic_factory, IWICBitmapFrameDecode *bitmap_frame)
{
    const struct pixel_format_desc *fmt_desc;
    uint32_t row_pitch, slice_pitch;
    IWICPalette *wic_palette = NULL;
    PALETTEENTRY *palette = NULL;
    WICColor *colors = NULL;
    BYTE *buffer = NULL;
    HRESULT hr;

    fmt_desc = get_d3dx_pixel_format_info(image->format);
    hr = d3dx_calculate_pixels_size(image->format, image->size.width, image->size.height, &row_pitch, &slice_pitch);
    if (FAILED(hr))
        return hr;

    /* Allocate a buffer for our image. */
    if (!(buffer = malloc(slice_pitch)))
        return E_OUTOFMEMORY;

    hr = IWICBitmapFrameDecode_CopyPixels(bitmap_frame, NULL, row_pitch, slice_pitch, buffer);
    if (FAILED(hr))
    {
        free(buffer);
        return hr;
    }

    if (is_index_format(fmt_desc))
    {
        uint32_t nb_colors, i;

        hr = IWICImagingFactory_CreatePalette(wic_factory, &wic_palette);
        if (FAILED(hr))
            goto exit;

        hr = IWICBitmapFrameDecode_CopyPalette(bitmap_frame, wic_palette);
        if (FAILED(hr))
            goto exit;

        hr = IWICPalette_GetColorCount(wic_palette, &nb_colors);
        if (FAILED(hr))
            goto exit;

        colors = malloc(nb_colors * sizeof(colors[0]));
        palette = malloc(nb_colors * sizeof(palette[0]));
        if (!colors || !palette)
        {
            hr = E_OUTOFMEMORY;
            goto exit;
        }

        hr = IWICPalette_GetColors(wic_palette, nb_colors, colors, &nb_colors);
        if (FAILED(hr))
            goto exit;

        /* Convert colors from WICColor (ARGB) to PALETTEENTRY (ABGR) */
        for (i = 0; i < nb_colors; i++)
        {
            palette[i].peRed   = (colors[i] >> 16) & 0xff;
            palette[i].peGreen = (colors[i] >> 8) & 0xff;
            palette[i].peBlue  = colors[i] & 0xff;
            palette[i].peFlags = (colors[i] >> 24) & 0xff; /* peFlags is the alpha component in DX8 and higher */
        }
    }

    image->image_buf = image->pixels = buffer;
    image->image_palette = image->palette = palette;

exit:
    free(colors);
    if (image->image_buf != buffer)
        free(buffer);
    if (image->image_palette != palette)
        free(palette);
    if (wic_palette)
        IWICPalette_Release(wic_palette);

    return hr;
}

static HRESULT d3dx_initialize_image_from_wic(const void *src_data, uint32_t src_data_size,
        struct d3dx_image *image, uint32_t flags)
{
    IWICBitmapFrameDecode *bitmap_frame = NULL;
    IWICBitmapDecoder *bitmap_decoder = NULL;
    uint32_t src_image_size = src_data_size;
    IWICImagingFactory *wic_factory;
    const void *src_image = src_data;
    WICPixelFormatGUID pixel_format;
    IWICStream *wic_stream = NULL;
    uint32_t frame_count = 0;
    GUID container_format;
    BOOL is_dib = FALSE;
    HRESULT hr;

    hr = WICCreateImagingFactory_Proxy(WINCODEC_SDK_VERSION, &wic_factory);
    if (FAILED(hr))
        return hr;

    is_dib = convert_dib_to_bmp(&src_image, &src_image_size);
    hr = IWICImagingFactory_CreateStream(wic_factory, &wic_stream);
    if (FAILED(hr))
        goto exit;

    hr = IWICStream_InitializeFromMemory(wic_stream, (BYTE *)src_image, src_image_size);
    if (FAILED(hr))
        goto exit;

    hr = IWICImagingFactory_CreateDecoderFromStream(wic_factory, (IStream *)wic_stream, NULL, 0, &bitmap_decoder);
    if (FAILED(hr))
    {
        if ((src_image_size >= 2) && (!memcmp(src_image, "P3", 2) || !memcmp(src_image, "P6", 2)))
            FIXME("File type PPM is not supported yet.\n");
        else if ((src_image_size >= 10) && !memcmp(src_image, "#?RADIANCE", 10))
            FIXME("File type HDR is not supported yet.\n");
        else if ((src_image_size >= 2) && (!memcmp(src_image, "PF", 2) || !memcmp(src_image, "Pf", 2)))
            FIXME("File type PFM is not supported yet.\n");
        goto exit;
    }

    hr = IWICBitmapDecoder_GetContainerFormat(bitmap_decoder, &container_format);
    if (FAILED(hr))
        goto exit;

    image->image_file_format = wic_container_guid_to_d3dx_image_file_format(&container_format, flags);
    if (is_dib && image->image_file_format == D3DX_IMAGE_FILE_FORMAT_BMP)
    {
        image->image_file_format = D3DX_IMAGE_FILE_FORMAT_DIB;
    }
    else if (image->image_file_format == D3DX_IMAGE_FILE_FORMAT_FORCE_DWORD)
    {
        WARN("Unsupported image file format %s.\n", debugstr_guid(&container_format));
        hr = D3DXERR_INVALIDDATA;
        goto exit;
    }

    TRACE("File type is %s.\n", debug_d3dx_image_file_format(image->image_file_format));
    hr = IWICBitmapDecoder_GetFrameCount(bitmap_decoder, &frame_count);
    if (FAILED(hr) || (SUCCEEDED(hr) && !frame_count))
    {
        hr = D3DXERR_INVALIDDATA;
        goto exit;
    }

    hr = IWICBitmapDecoder_GetFrame(bitmap_decoder, 0, &bitmap_frame);
    if (FAILED(hr))
        goto exit;

    hr = IWICBitmapFrameDecode_GetSize(bitmap_frame, &image->size.width, &image->size.height);
    if (FAILED(hr))
        goto exit;

    hr = IWICBitmapFrameDecode_GetPixelFormat(bitmap_frame, &pixel_format);
    if (FAILED(hr))
        goto exit;

    if ((image->format = wic_guid_to_d3dx_pixel_format_id(&pixel_format)) == D3DX_PIXEL_FORMAT_COUNT)
    {
        WARN("Unsupported pixel format %s.\n", debugstr_guid(&pixel_format));
        hr = D3DXERR_INVALIDDATA;
        goto exit;
    }

    /* D3DX10/D3DX11 ignore alpha channels in bitmaps. */
    if (!(flags & D3DX_IMAGE_SUPPORT_DXT10) && image_is_argb(bitmap_frame, image))
        image->format = D3DX_PIXEL_FORMAT_B8G8R8A8_UNORM;

    if (!(flags & D3DX_IMAGE_INFO_ONLY))
    {
        hr = d3dx_image_wic_frame_decode(image, wic_factory, bitmap_frame);
        if (FAILED(hr))
            goto exit;
    }

    image->size.depth = 1;
    image->mip_levels = 1;
    image->layer_count = 1;
    image->resource_type = D3DX_RESOURCE_TYPE_TEXTURE_2D;

exit:
    if (is_dib)
        free((void *)src_image);
    if (bitmap_frame)
        IWICBitmapFrameDecode_Release(bitmap_frame);
    if (bitmap_decoder)
        IWICBitmapDecoder_Release(bitmap_decoder);
    if (wic_stream)
        IWICStream_Release(wic_stream);
    IWICImagingFactory_Release(wic_factory);

    return hr;
}

HRESULT d3dx_image_init(const void *src_data, uint32_t src_data_size, struct d3dx_image *image,
        uint32_t starting_mip_level, uint32_t flags)
{
    if (!src_data || !src_data_size || !image)
        return D3DERR_INVALIDCALL;

    memset(image, 0, sizeof(*image));
    if ((src_data_size >= 4) && !memcmp(src_data, "DDS ", 4))
        return d3dx_initialize_image_from_dds(src_data, src_data_size, image, starting_mip_level, flags);

    return d3dx_initialize_image_from_wic(src_data, src_data_size, image, flags);
}

void d3dx_image_cleanup(struct d3dx_image *image)
{
    free(image->image_buf);
    free(image->image_palette);
}

HRESULT d3dx_image_get_pixels(struct d3dx_image *image, uint32_t layer, uint32_t mip_level,
        struct d3dx_pixels *pixels)
{
    struct volume mip_level_size = image->size;
    const BYTE *pixels_ptr = image->pixels;
    uint32_t row_pitch, slice_pitch, i;
    RECT unaligned_rect;
    HRESULT hr = S_OK;

    if (mip_level >= image->mip_levels)
    {
        ERR("Tried to retrieve mip level %u, but image only has %u mip levels.\n", mip_level, image->mip_levels);
        return E_FAIL;
    }

    if (layer >= image->layer_count)
    {
        ERR("Tried to retrieve layer %u, but image only has %u layers.\n", layer, image->layer_count);
        return E_FAIL;
    }

    slice_pitch = row_pitch = 0;
    for (i = 0; i < image->mip_levels; i++)
    {
        hr = d3dx_calculate_pixels_size(image->format, mip_level_size.width, mip_level_size.height, &row_pitch, &slice_pitch);
        if (FAILED(hr))
            return hr;

        if (i == mip_level)
            break;

        pixels_ptr += slice_pitch * mip_level_size.depth;
        d3dx_get_next_mip_level_size(&mip_level_size);
    }

    pixels_ptr += (layer * image->layer_pitch);
    SetRect(&unaligned_rect, 0, 0, mip_level_size.width, mip_level_size.height);
    set_d3dx_pixels(pixels, pixels_ptr, row_pitch, slice_pitch, image->palette, mip_level_size.width,
            mip_level_size.height, mip_level_size.depth, &unaligned_rect);

    return D3D_OK;
}

/*
 * Pixel filtering/format conversion functions.
 */
unsigned short float_32_to_16(const float in)
{
    int exp = 0, origexp;
    float tmp = fabsf(in);
    int sign = (copysignf(1, in) < 0);
    unsigned int mantissa;
    unsigned short ret;

    /* Deal with special numbers */
    if (isinf(in)) return (sign ? 0xffff : 0x7fff);
    if (isnan(in)) return (sign ? 0xffff : 0x7fff);
    if (in == 0.0f) return (sign ? 0x8000 : 0x0000);

    if (tmp < (float)(1u << 10))
    {
        do
        {
            tmp *= 2.0f;
            exp--;
        } while (tmp < (float)(1u << 10));
    }
    else if (tmp >= (float)(1u << 11))
    {
        do
        {
            tmp /= 2.0f;
            exp++;
        } while (tmp >= (float)(1u << 11));
    }

    exp += 10;  /* Normalize the mantissa */
    exp += 15;  /* Exponent is encoded with excess 15 */

    origexp = exp;

    mantissa = (unsigned int) tmp;
    if ((tmp - mantissa == 0.5f && mantissa % 2 == 1) || /* round half to even */
        (tmp - mantissa > 0.5f))
    {
        mantissa++; /* round to nearest, away from zero */
    }
    if (mantissa == 2048)
    {
        mantissa = 1024;
        exp++;
    }

    if (exp > 31)
    {
        /* too big */
        ret = 0x7fff; /* INF */
    }
    else if (exp <= 0)
    {
        unsigned int rounding = 0;

        /* Denormalized half float */

        /* return 0x0000 (=0.0) for numbers too small to represent in half floats */
        if (exp < -11)
            return (sign ? 0x8000 : 0x0000);

        exp = origexp;

        /* the 13 extra bits from single precision are used for rounding */
        mantissa = (unsigned int)(tmp * (1u << 13));
        mantissa >>= 1 - exp; /* denormalize */

        mantissa -= ~(mantissa >> 13) & 1; /* round half to even */
        /* remove 13 least significant bits to get half float precision */
        mantissa >>= 12;
        rounding = mantissa & 1;
        mantissa >>= 1;

        ret = mantissa + rounding;
    }
    else
    {
        ret = (exp << 10) | (mantissa & 0x3ff);
    }

    ret |= ((sign ? 1 : 0) << 15); /* Add the sign */
    return ret;
}

/* Native d3dx9's D3DXFloat16to32Array lacks support for NaN and Inf. Specifically, e = 16 is treated as a
 * regular number - e.g., 0x7fff is converted to 131008.0 and 0xffff to -131008.0. */
float float_16_to_32(const unsigned short in)
{
    const unsigned short s = (in & 0x8000);
    const unsigned short e = (in & 0x7C00) >> 10;
    const unsigned short m = in & 0x3FF;
    const float sgn = (s ? -1.0f : 1.0f);

    if (e == 0)
    {
        if (m == 0) return sgn * 0.0f; /* +0.0 or -0.0 */
        else return sgn * powf(2, -14.0f) * (m / 1024.0f);
    }
    else
    {
        return sgn * powf(2, e - 15.0f) * (1.0f + (m / 1024.0f));
    }
}

struct argb_conversion_info
{
    const struct pixel_format_desc *srcformat;
    const struct pixel_format_desc *destformat;
    DWORD srcshift[4], destshift[4];
    DWORD srcmask[4], destmask[4];
    BOOL process_channel[4];
    DWORD channelmask;
};

static void init_argb_conversion_info(const struct pixel_format_desc *srcformat, const struct pixel_format_desc *destformat, struct argb_conversion_info *info)
{
    UINT i;
    ZeroMemory(info->process_channel, 4 * sizeof(BOOL));
    info->channelmask = 0;

    info->srcformat  =  srcformat;
    info->destformat = destformat;

    for(i = 0;i < 4;i++) {
        /* srcshift is used to extract the _relevant_ components */
        info->srcshift[i]  =  srcformat->shift[i] + max( srcformat->bits[i] - destformat->bits[i], 0);

        /* destshift is used to move the components to the correct position */
        info->destshift[i] = destformat->shift[i] + max(destformat->bits[i] -  srcformat->bits[i], 0);

        info->srcmask[i]  = ((1 <<  srcformat->bits[i]) - 1) <<  srcformat->shift[i];
        info->destmask[i] = ((1 << destformat->bits[i]) - 1) << destformat->shift[i];

        /* channelmask specifies bits which aren't used in the source format but in the destination one */
        if(destformat->bits[i]) {
            if(srcformat->bits[i]) info->process_channel[i] = TRUE;
            else info->channelmask |= info->destmask[i];
        }
    }
}

/************************************************************
 * get_relevant_argb_components
 *
 * Extracts the relevant components from the source color and
 * drops the less significant bits if they aren't used by the destination format.
 */
static void get_relevant_argb_components(const struct argb_conversion_info *info, const BYTE *col, DWORD *out)
{
    unsigned int i, j;
    unsigned int component, mask;

    for (i = 0; i < 4; ++i)
    {
        if (!info->process_channel[i])
            continue;

        component = 0;
        mask = info->srcmask[i];
        for (j = 0; j < 4 && mask; ++j)
        {
            if (info->srcshift[i] < j * 8)
                component |= (col[j] & mask) << (j * 8 - info->srcshift[i]);
            else
                component |= (col[j] & mask) >> (info->srcshift[i] - j * 8);
            mask >>= 8;
        }
        out[i] = component;
    }
}

static float d3dx_clamp(float value, float min_value, float max_value)
{
    return value < min_value ? min_value : value > max_value ? max_value : value;
}

/************************************************************
 * make_argb_color
 *
 * Recombines the output of get_relevant_argb_components and converts
 * it to the destination format.
 */
static DWORD make_argb_color(const struct argb_conversion_info *info, const DWORD *in)
{
    UINT i;
    DWORD val = 0;

    for(i = 0;i < 4;i++) {
        if(info->process_channel[i]) {
            /* necessary to make sure that e.g. an X4R4G4B4 white maps to an R8G8B8 white instead of 0xf0f0f0 */
            signed int shift;
            for(shift = info->destshift[i]; shift > info->destformat->shift[i]; shift -= info->srcformat->bits[i]) val |= in[i] << shift;
            val |= (in[i] >> (info->destformat->shift[i] - shift)) << info->destformat->shift[i];
        }
    }
    val |= info->channelmask;   /* new channels are set to their maximal value */
    return val;
}

static BYTE get_channel_from_palette_entry(const PALETTEENTRY *entry, uint32_t channel)
{
    switch (channel)
    {
    case 0: /* Alpha. */
        return entry->peFlags;
    case 1: /* Red. */
        return entry->peRed;
    case 2: /* Green. */
        return entry->peGreen;
    case 3: /* Blue. */
        return entry->peBlue;
    default:
        assert(0);
        return 0;
    }
}

static void premultiply_alpha(struct vec4 *vec)
{
    vec->x *= vec->w;
    vec->y *= vec->w;
    vec->z *= vec->w;
}

static void demultiply_alpha(struct vec4 *vec)
{
    vec->x = (vec->w == 0.0f) ? 0.0f : vec->x / vec->w;
    vec->y = (vec->w == 0.0f) ? 0.0f : vec->y / vec->w;
    vec->z = (vec->w == 0.0f) ? 0.0f : vec->z / vec->w;
}

static void vec4_to_sRGB(struct vec4 *vec)
{
    vec->x = (vec->x <= 0.0031308f) ? (12.92f * vec->x) : (1.055f * powf(vec->x, 1.0f / 2.4f) - 0.055f);
    vec->y = (vec->y <= 0.0031308f) ? (12.92f * vec->y) : (1.055f * powf(vec->y, 1.0f / 2.4f) - 0.055f);
    vec->z = (vec->z <= 0.0031308f) ? (12.92f * vec->z) : (1.055f * powf(vec->z, 1.0f / 2.4f) - 0.055f);
}

static void vec4_from_sRGB(struct vec4 *vec)
{
    vec->x = (vec->x <= 0.04045f) ? vec->x / 12.92f : powf((vec->x + 0.055f) / 1.055f, 2.4f);
    vec->y = (vec->y <= 0.04045f) ? vec->y / 12.92f : powf((vec->y + 0.055f) / 1.055f, 2.4f);
    vec->z = (vec->z <= 0.04045f) ? vec->z / 12.92f : powf((vec->z + 0.055f) / 1.055f, 2.4f);
}

/* It doesn't work for components bigger than 32 bits (or somewhat smaller but unaligned). */
void format_to_vec4(const struct pixel_format_desc *format, const BYTE *src, const PALETTEENTRY *palette,
        struct vec4 *dst)
{
    const struct pixel_format_type_desc *type = &format->fmt_type_desc;
    DWORD mask, tmp;
    unsigned int c;

    for (c = 0; c < 4; ++c)
    {
        const enum component_type dst_ctype = !c ? type->a_type : type->rgb_type;
        static const unsigned int component_offsets[4] = {3, 0, 1, 2};
        float *dst_component = (float *)dst + component_offsets[c];

        if (format->bits[c])
        {
            mask = ~0u >> (32 - format->bits[c]);

            memcpy(&tmp, src + format->shift[c] / 8,
                    min(sizeof(DWORD), (format->shift[c] % 8 + format->bits[c] + 7) / 8));

            switch (dst_ctype)
            {
            case CTYPE_FLOAT:
                if (format->bits[c] == 16)
                    *dst_component = float_16_to_32(tmp);
                else
                    *dst_component = *(float *)&tmp;
                break;

            case CTYPE_INDEX:
                *dst_component = get_channel_from_palette_entry(&palette[(tmp >> format->shift[c] % 8) & mask], c);
                *dst_component /= 255.0f;
                break;

            case CTYPE_LUMA:
            case CTYPE_UNORM:
                *dst_component = (float)((tmp >> format->shift[c] % 8) & mask) / mask;
                break;

            case CTYPE_SNORM:
            {
                const uint32_t sign_bit = (1 << (format->bits[c] - 1));
                uint32_t tmp_extended, tmp_masked = (tmp >> format->shift[c] % 8) & mask;

                tmp_extended = tmp_masked;
                if (tmp_masked & sign_bit)
                {
                    tmp_extended |= ~(sign_bit - 1);

                    /*
                     * In order to clamp to an even range, we need to ignore
                     * the maximum negative value.
                     */
                    if (tmp_masked == sign_bit)
                        tmp_extended |= 1;
                }

                *dst_component = (float)(((int32_t)tmp_extended)) / (sign_bit - 1);
                break;
            }

            default:
                break;
            }
        }
        else if (dst_ctype == CTYPE_LUMA)
        {
            *dst_component = dst->x;
        }
        else
        {
            *dst_component = 1.0f;
        }
    }
}

/* It doesn't work for components bigger than 32 bits. */
void format_from_vec4(const struct pixel_format_desc *format, const struct vec4 *src,
        const struct pixel_format_type_desc *src_type, BYTE *dst)
{
    const struct pixel_format_type_desc *dst_type = &format->fmt_type_desc;
    DWORD v, mask32;
    unsigned int c, i;

    memset(dst, 0, format->bytes_per_pixel);

    for (c = 0; c < 4; ++c)
    {
        const enum component_type src_ctype = !c ? src_type->a_type : src_type->rgb_type;
        const enum component_type dst_ctype = !c ? dst_type->a_type : dst_type->rgb_type;
        static const unsigned int component_offsets[4] = {3, 0, 1, 2};
        const float src_component = *((const float *)src + component_offsets[c]);

        if (!format->bits[c])
            continue;

        mask32 = ~0u >> (32 - format->bits[c]);

        switch (dst_ctype)
        {
        case CTYPE_FLOAT:
            if (format->bits[c] == 16)
                v = float_32_to_16(src_component);
            else
                v = *(DWORD *)&src_component;
            break;

        case CTYPE_LUMA:
        {
            float val = src->x * 0.2125f + src->y * 0.7154f + src->z * 0.0721f;

            if (src_ctype == CTYPE_SNORM)
                val = (val + 1.0f) / 2.0f;

            v = d3dx_clamp(val, 0.0f, 1.0f) * ((1 << format->bits[c]) - 1) + 0.5f;
            break;
        }

        case CTYPE_UNORM:
        {
            float val = src_component;

            if (src_ctype == CTYPE_SNORM)
                val = (val + 1.0f) / 2.0f;

            v = d3dx_clamp(val, 0.0f, 1.0f) * ((1 << format->bits[c]) - 1) + 0.5f;
            break;
        }

        case CTYPE_SNORM:
        {
            const uint32_t max_value = (1 << (format->bits[c] - 1)) - 1;
            float val = src_component;

            if (src_ctype == CTYPE_UNORM || src_ctype == CTYPE_LUMA || src_ctype == CTYPE_INDEX)
                val = (val * 2.0f) - 1.0f;

            v = d3dx_clamp(val, -1.0f, 1.0f) * max_value + 0.5f;
            break;
        }

        /* We shouldn't be trying to convert to CTYPE_INDEX. */
        case CTYPE_INDEX:
            assert(0);
            break;

        default:
            v = 0;
            break;
        }

        for (i = format->shift[c] / 8 * 8; i < format->shift[c] + format->bits[c]; i += 8)
        {
            BYTE mask, byte;

            if (format->shift[c] > i)
            {
                mask = mask32 << (format->shift[c] - i);
                byte = (v << (format->shift[c] - i)) & mask;
            }
            else
            {
                mask = mask32 >> (i - format->shift[c]);
                byte = (v >> (i - format->shift[c])) & mask;
            }
            dst[i / 8] |= byte;
        }
    }
}

/************************************************************
 * copy_pixels
 *
 * Copies the source buffer to the destination buffer.
 * Works for any pixel format.
 * The source and the destination must be block-aligned.
 */
void copy_pixels(const BYTE *src, UINT src_row_pitch, UINT src_slice_pitch,
        BYTE *dst, UINT dst_row_pitch, UINT dst_slice_pitch, const struct volume *size,
        const struct pixel_format_desc *format)
{
    UINT row, slice;
    BYTE *dst_addr;
    const BYTE *src_addr;
    UINT row_block_count = (size->width + format->block_width - 1) / format->block_width;
    UINT row_count = (size->height + format->block_height - 1) / format->block_height;

    for (slice = 0; slice < size->depth; slice++)
    {
        src_addr = src + slice * src_slice_pitch;
        dst_addr = dst + slice * dst_slice_pitch;

        for (row = 0; row < row_count; row++)
        {
            memcpy(dst_addr, src_addr, row_block_count * format->block_byte_count);
            src_addr += src_row_pitch;
            dst_addr += dst_row_pitch;
        }
    }
}

/************************************************************
 * convert_argb_pixels
 *
 * Copies the source buffer to the destination buffer, performing
 * any necessary format conversion and color keying.
 * Pixels outsize the source rect are blacked out.
 */
void convert_argb_pixels(const BYTE *src, UINT src_row_pitch, UINT src_slice_pitch, const struct volume *src_size,
        const struct pixel_format_desc *src_format, BYTE *dst, UINT dst_row_pitch, UINT dst_slice_pitch,
        const struct volume *dst_size, const struct pixel_format_desc *dst_format, D3DCOLOR color_key,
        const PALETTEENTRY *palette, uint32_t filter_flags)
{
    struct argb_conversion_info conv_info, ck_conv_info;
    const struct pixel_format_desc *ck_format = NULL;
    DWORD channels[4];
    BOOL src_pma, dst_pma, src_srgb, dst_srgb;
    UINT min_width, min_height, min_depth;
    UINT x, y, z;

    TRACE("src %p, src_row_pitch %u, src_slice_pitch %u, src_size %p, src_format %p, dst %p, "
            "dst_row_pitch %u, dst_slice_pitch %u, dst_size %p, dst_format %p, color_key 0x%08lx, palette %p.\n",
            src, src_row_pitch, src_slice_pitch, src_size, src_format, dst, dst_row_pitch, dst_slice_pitch, dst_size,
            dst_format, color_key, palette);

    ZeroMemory(channels, sizeof(channels));
    init_argb_conversion_info(src_format, dst_format, &conv_info);

    src_pma = !!(filter_flags & D3DX_FILTER_PMA_IN);
    dst_pma = !!(filter_flags & D3DX_FILTER_PMA_OUT);
    src_srgb = !!(filter_flags & D3DX_FILTER_SRGB_IN);
    dst_srgb = !!(filter_flags & D3DX_FILTER_SRGB_OUT);
    min_width = min(src_size->width, dst_size->width);
    min_height = min(src_size->height, dst_size->height);
    min_depth = min(src_size->depth, dst_size->depth);

    if (color_key)
    {
        /* Color keys are always represented in D3DX_PIXEL_FORMAT_B8G8R8A8_UNORM format. */
        ck_format = get_d3dx_pixel_format_info(D3DX_PIXEL_FORMAT_B8G8R8A8_UNORM);
        init_argb_conversion_info(src_format, ck_format, &ck_conv_info);
    }

    for (z = 0; z < min_depth; z++) {
        const BYTE *src_slice_ptr = src + z * src_slice_pitch;
        BYTE *dst_slice_ptr = dst + z * dst_slice_pitch;

        for (y = 0; y < min_height; y++) {
            const BYTE *src_ptr = src_slice_ptr + y * src_row_pitch;
            BYTE *dst_ptr = dst_slice_ptr + y * dst_row_pitch;

            for (x = 0; x < min_width; x++) {
                if (format_types_match(src_format, dst_format) && filter_flags_match(filter_flags)
                        && src_format->bytes_per_pixel <= 4 && dst_format->bytes_per_pixel <= 4)
                {
                    DWORD val;

                    get_relevant_argb_components(&conv_info, src_ptr, channels);
                    val = make_argb_color(&conv_info, channels);

                    if (color_key)
                    {
                        DWORD ck_pixel;

                        get_relevant_argb_components(&ck_conv_info, src_ptr, channels);
                        ck_pixel = make_argb_color(&ck_conv_info, channels);
                        if (ck_pixel == color_key)
                            val &= ~conv_info.destmask[0];
                    }
                    memcpy(dst_ptr, &val, dst_format->bytes_per_pixel);
                }
                else
                {
                    struct vec4 color, tmp;

                    format_to_vec4(src_format, src_ptr, palette, &color);
                    if (src_pma && src_pma != dst_pma)
                        demultiply_alpha(&color);
                    if (src_srgb && src_srgb != dst_srgb)
                        vec4_from_sRGB(&color);
                    tmp = color;

                    if (ck_format)
                    {
                        DWORD ck_pixel;

                        format_from_vec4(ck_format, &tmp, &src_format->fmt_type_desc, (BYTE *)&ck_pixel);
                        if (ck_pixel == color_key)
                            tmp.w = 0.0f;
                    }

                    color = tmp;
                    if (dst_srgb && src_srgb != dst_srgb)
                        vec4_to_sRGB(&color);
                    if (dst_pma && src_pma != dst_pma)
                        premultiply_alpha(&color);
                    format_from_vec4(dst_format, &color, &src_format->fmt_type_desc, dst_ptr);
                }

                src_ptr += src_format->bytes_per_pixel;
                dst_ptr += dst_format->bytes_per_pixel;
            }

            if (src_size->width < dst_size->width) /* black out remaining pixels */
                memset(dst_ptr, 0, dst_format->bytes_per_pixel * (dst_size->width - src_size->width));
        }

        if (src_size->height < dst_size->height) /* black out remaining pixels */
            memset(dst + src_size->height * dst_row_pitch, 0, dst_row_pitch * (dst_size->height - src_size->height));
    }
    if (src_size->depth < dst_size->depth) /* black out remaining pixels */
        memset(dst + src_size->depth * dst_slice_pitch, 0, dst_slice_pitch * (dst_size->depth - src_size->depth));
}

/************************************************************
 * point_filter_argb_pixels
 *
 * Copies the source buffer to the destination buffer, performing
 * any necessary format conversion, color keying and stretching
 * using a point filter.
 */
static void point_filter_argb_pixels(const BYTE *src, UINT src_row_pitch, UINT src_slice_pitch, const struct volume *src_size,
        const struct pixel_format_desc *src_format, BYTE *dst, UINT dst_row_pitch, UINT dst_slice_pitch,
        const struct volume *dst_size, const struct pixel_format_desc *dst_format, D3DCOLOR color_key,
        const PALETTEENTRY *palette, uint32_t filter_flags)
{
    struct argb_conversion_info conv_info, ck_conv_info;
    const struct pixel_format_desc *ck_format = NULL;
    BOOL src_pma, dst_pma, src_srgb, dst_srgb;
    DWORD channels[4];
    UINT x, y, z;

    TRACE("src %p, src_row_pitch %u, src_slice_pitch %u, src_size %p, src_format %p, dst %p, "
            "dst_row_pitch %u, dst_slice_pitch %u, dst_size %p, dst_format %p, color_key 0x%08lx, palette %p.\n",
            src, src_row_pitch, src_slice_pitch, src_size, src_format, dst, dst_row_pitch, dst_slice_pitch, dst_size,
            dst_format, color_key, palette);

    src_srgb = !!(filter_flags & D3DX_FILTER_SRGB_IN);
    dst_srgb = !!(filter_flags & D3DX_FILTER_SRGB_OUT);
    src_pma = !!(filter_flags & D3DX_FILTER_PMA_IN);
    dst_pma = !!(filter_flags & D3DX_FILTER_PMA_OUT);
    ZeroMemory(channels, sizeof(channels));
    init_argb_conversion_info(src_format, dst_format, &conv_info);

    if (color_key)
    {
        /* Color keys are always represented in D3DX_PIXEL_FORMAT_B8G8R8A8_UNORM format. */
        ck_format = get_d3dx_pixel_format_info(D3DX_PIXEL_FORMAT_B8G8R8A8_UNORM);
        init_argb_conversion_info(src_format, ck_format, &ck_conv_info);
    }

    for (z = 0; z < dst_size->depth; z++)
    {
        BYTE *dst_slice_ptr = dst + z * dst_slice_pitch;
        const BYTE *src_slice_ptr = src + src_slice_pitch * (z * src_size->depth / dst_size->depth);

        for (y = 0; y < dst_size->height; y++)
        {
            BYTE *dst_ptr = dst_slice_ptr + y * dst_row_pitch;
            const BYTE *src_row_ptr = src_slice_ptr + src_row_pitch * (y * src_size->height / dst_size->height);

            for (x = 0; x < dst_size->width; x++)
            {
                const BYTE *src_ptr = src_row_ptr + (x * src_size->width / dst_size->width) * src_format->bytes_per_pixel;

                if (format_types_match(src_format, dst_format) && filter_flags_match(filter_flags)
                        && src_format->bytes_per_pixel <= 4 && dst_format->bytes_per_pixel <= 4)
                {
                    DWORD val;

                    get_relevant_argb_components(&conv_info, src_ptr, channels);
                    val = make_argb_color(&conv_info, channels);

                    if (color_key)
                    {
                        DWORD ck_pixel;

                        get_relevant_argb_components(&ck_conv_info, src_ptr, channels);
                        ck_pixel = make_argb_color(&ck_conv_info, channels);
                        if (ck_pixel == color_key)
                            val &= ~conv_info.destmask[0];
                    }
                    memcpy(dst_ptr, &val, dst_format->bytes_per_pixel);
                }
                else
                {
                    struct vec4 color, tmp;

                    format_to_vec4(src_format, src_ptr, palette, &color);
                    if (src_pma && src_pma != dst_pma)
                        demultiply_alpha(&color);
                    if (src_srgb && src_srgb != dst_srgb)
                        vec4_from_sRGB(&color);
                    tmp = color;

                    if (ck_format)
                    {
                        DWORD ck_pixel;

                        format_from_vec4(ck_format, &tmp, &src_format->fmt_type_desc, (BYTE *)&ck_pixel);
                        if (ck_pixel == color_key)
                            tmp.w = 0.0f;
                    }

                    color = tmp;
                    if (dst_srgb && src_srgb != dst_srgb)
                        vec4_to_sRGB(&color);
                    if (dst_pma && src_pma != dst_pma)
                        premultiply_alpha(&color);
                    format_from_vec4(dst_format, &color, &src_format->fmt_type_desc, dst_ptr);
                }

                dst_ptr += dst_format->bytes_per_pixel;
            }
        }
    }
}

struct d3dx_bcn_decompression_context
{
    void (*decompress_bcn_block)(const void *, void *, int);
    const struct pixel_format_desc *compressed_format_desc;
    struct d3dx_pixels *compressed_pixels;

    const struct pixel_format_desc *decompressed_format_desc;
    uint8_t cur_block_decompressed[192];
    uint32_t cur_block_row_pitch;
    int32_t cur_block_x;
    int32_t cur_block_y;
    int32_t cur_block_z;
};

static void d3dx_init_bcn_decompression_context(struct d3dx_bcn_decompression_context *context,
        struct d3dx_pixels *pixels, const struct pixel_format_desc *desc, const struct pixel_format_desc *dst_desc)
{
    memset(context, 0, sizeof(*context));
    switch (desc->format)
    {
    case D3DX_PIXEL_FORMAT_DXT1_UNORM:
    case D3DX_PIXEL_FORMAT_BC1_UNORM_SRGB:
        context->decompress_bcn_block = bcdec_bc1;
        break;

    case D3DX_PIXEL_FORMAT_DXT2_UNORM:
    case D3DX_PIXEL_FORMAT_DXT3_UNORM:
    case D3DX_PIXEL_FORMAT_BC2_UNORM_SRGB:
        context->decompress_bcn_block = bcdec_bc2;
        break;

    case D3DX_PIXEL_FORMAT_DXT4_UNORM:
    case D3DX_PIXEL_FORMAT_DXT5_UNORM:
    case D3DX_PIXEL_FORMAT_BC3_UNORM_SRGB:
        context->decompress_bcn_block = bcdec_bc3;
        break;

    case D3DX_PIXEL_FORMAT_BC4_UNORM:
    case D3DX_PIXEL_FORMAT_BC4_SNORM:
        context->decompress_bcn_block = bcdec_bc4;
        break;

    case D3DX_PIXEL_FORMAT_BC5_UNORM:
    case D3DX_PIXEL_FORMAT_BC5_SNORM:
        context->decompress_bcn_block = bcdec_bc5;
        break;

    default:
        assert(0);
        break;
    }

    context->compressed_format_desc = desc;
    context->compressed_pixels = pixels;

    context->decompressed_format_desc = dst_desc;
    context->cur_block_row_pitch = dst_desc->bytes_per_pixel * desc->block_width;
    context->cur_block_x = context->cur_block_y = context->cur_block_z = -1;
}

static void d3dx_fetch_bcn_texel(struct d3dx_bcn_decompression_context *context, int32_t x, int32_t y, int32_t z, void *texel)
{
    const struct pixel_format_desc *decomp_fmt_desc = context->decompressed_format_desc;
    const struct pixel_format_desc *comp_fmt_desc = context->compressed_format_desc;
    const int32_t y_aligned = (y & ~(comp_fmt_desc->block_height - 1));
    const int32_t x_aligned = (x & ~(comp_fmt_desc->block_width - 1));
    uint32_t pixel_offset;

    if (z != context->cur_block_z || (x_aligned != context->cur_block_x) || (y_aligned != context->cur_block_y))
    {
        const BYTE *block_ptr = context->compressed_pixels->data;

        block_ptr += z * context->compressed_pixels->slice_pitch;
        block_ptr += (y / comp_fmt_desc->block_height) * context->compressed_pixels->row_pitch;
        block_ptr += (x / comp_fmt_desc->block_width) * comp_fmt_desc->block_byte_count;
        context->decompress_bcn_block(block_ptr, context->cur_block_decompressed, context->cur_block_row_pitch);
        context->cur_block_x = (x & (comp_fmt_desc->block_width));
        context->cur_block_y = (y & (comp_fmt_desc->block_height));
        context->cur_block_z = z;
    }

    pixel_offset = (y & (comp_fmt_desc->block_height - 1)) * context->cur_block_row_pitch;
    pixel_offset += (x & (comp_fmt_desc->block_width - 1)) * decomp_fmt_desc->bytes_per_pixel;
    memcpy(texel, context->cur_block_decompressed + pixel_offset, decomp_fmt_desc->bytes_per_pixel);
}

static HRESULT d3dx_pixels_decompress(struct d3dx_pixels *pixels, const struct pixel_format_desc *desc,
        BOOL is_dst, void **out_memory, uint32_t *out_row_pitch, uint32_t *out_slice_pitch,
        const struct pixel_format_desc **out_desc)
{
    uint32_t x, y, z, uncompressed_slice_pitch, uncompressed_row_pitch;
    const struct pixel_format_desc *uncompressed_desc = NULL;
    struct d3dx_bcn_decompression_context context;
    const struct volume *size = &pixels->size;
    BYTE *uncompressed_mem;

    switch (desc->format)
    {
        case D3DX_PIXEL_FORMAT_DXT1_UNORM:
            uncompressed_desc = get_d3dx_pixel_format_info(D3DX_PIXEL_FORMAT_R8G8B8A8_UNORM);
            break;
        case D3DX_PIXEL_FORMAT_BC1_UNORM_SRGB:
            uncompressed_desc = get_d3dx_pixel_format_info(D3DX_PIXEL_FORMAT_R8G8B8A8_UNORM_SRGB);
            break;
        case D3DX_PIXEL_FORMAT_DXT2_UNORM:
        case D3DX_PIXEL_FORMAT_DXT3_UNORM:
            uncompressed_desc = get_d3dx_pixel_format_info(D3DX_PIXEL_FORMAT_R8G8B8A8_UNORM);
            break;
        case D3DX_PIXEL_FORMAT_BC2_UNORM_SRGB:
            uncompressed_desc = get_d3dx_pixel_format_info(D3DX_PIXEL_FORMAT_R8G8B8A8_UNORM_SRGB);
            break;
        case D3DX_PIXEL_FORMAT_DXT4_UNORM:
        case D3DX_PIXEL_FORMAT_DXT5_UNORM:
            uncompressed_desc = get_d3dx_pixel_format_info(D3DX_PIXEL_FORMAT_R8G8B8A8_UNORM);
            break;
        case D3DX_PIXEL_FORMAT_BC3_UNORM_SRGB:
            uncompressed_desc = get_d3dx_pixel_format_info(D3DX_PIXEL_FORMAT_R8G8B8A8_UNORM_SRGB);
            break;
        case D3DX_PIXEL_FORMAT_BC4_UNORM:
            uncompressed_desc = get_d3dx_pixel_format_info(D3DX_PIXEL_FORMAT_R8_UNORM);
            break;
        case D3DX_PIXEL_FORMAT_BC4_SNORM:
            uncompressed_desc = get_d3dx_pixel_format_info(D3DX_PIXEL_FORMAT_R8_SNORM);
            break;
        case D3DX_PIXEL_FORMAT_BC5_UNORM:
            uncompressed_desc = get_d3dx_pixel_format_info(D3DX_PIXEL_FORMAT_R8G8_UNORM);
            break;
        case D3DX_PIXEL_FORMAT_BC5_SNORM:
            uncompressed_desc = get_d3dx_pixel_format_info(D3DX_PIXEL_FORMAT_R8G8_SNORM);
            break;
        default:
            FIXME("Unexpected compressed texture format %u.\n", desc->format);
            return E_NOTIMPL;
    }

    uncompressed_row_pitch = size->width * uncompressed_desc->bytes_per_pixel;
    uncompressed_slice_pitch = uncompressed_row_pitch * size->height;
    if (!(uncompressed_mem = malloc(size->depth * uncompressed_slice_pitch)))
        return E_OUTOFMEMORY;

    /*
     * For compressed destination pixels, width/height will represent
     * the entire set of compressed blocks our destination rectangle touches.
     * If we're only updating a sub-area of any blocks, we need to decompress
     * the pixels outside of the sub-area.
     */
    if (is_dst)
    {
        const RECT aligned_rect = { 0, 0, size->width, size->height };

        /*
         * If our destination covers the entire set of blocks, no
         * decompression needs to be done, just return the allocated memory.
         */
        if (EqualRect(&aligned_rect, &pixels->unaligned_rect))
            goto exit;
    }

    TRACE("Decompressing pixels.\n");
    d3dx_init_bcn_decompression_context(&context, pixels, desc, uncompressed_desc);
    for (z = 0; z < size->depth; ++z)
    {
        for (y = 0; y < size->height; ++y)
        {
            BYTE *ptr = &uncompressed_mem[(z * uncompressed_slice_pitch) + (y * uncompressed_row_pitch)];
            for (x = 0; x < size->width; ++x)
            {
                const POINT pt = { x, y };

                if (!is_dst)
                    d3dx_fetch_bcn_texel(&context, x + pixels->unaligned_rect.left, y + pixels->unaligned_rect.top, z, ptr);
                else if (!PtInRect(&pixels->unaligned_rect, pt))
                    d3dx_fetch_bcn_texel(&context, x, y, z, ptr);
                ptr += uncompressed_desc->bytes_per_pixel;
            }
        }
    }

exit:
    *out_memory = uncompressed_mem;
    *out_row_pitch = uncompressed_row_pitch;
    *out_slice_pitch = uncompressed_slice_pitch;
    *out_desc = uncompressed_desc;

    return S_OK;
}

static void d3dx_init_bcn_block_buffer(const void *src_data, uint32_t src_row_pitch, uint8_t src_width, uint8_t src_height,
        const struct pixel_format_desc *src_desc, uint8_t *block_buf)
{
    uint8_t x, y;

    for (y = 0; y < 4; ++y)
    {
        const uint8_t *src_row = ((const uint8_t *)src_data) + ((y % src_height) * src_row_pitch);

        for (x = 0; x < 4; ++x)
        {
            uint8_t *dst_pixel = &block_buf[(y * 4 * src_desc->bytes_per_pixel) + (x * src_desc->bytes_per_pixel)];
            const uint8_t *src_pixel = src_row + (((x % src_width)) * src_desc->bytes_per_pixel);

            memcpy(dst_pixel, src_pixel, src_desc->bytes_per_pixel);
        }
    }
}

static void d3dx_compress_bc1_block(const void *src_data, uint32_t src_row_pitch, uint8_t src_width, uint8_t src_height,
        void *dst_data)
{
    const struct pixel_format_desc *fmt_desc = get_d3dx_pixel_format_info(D3DX_PIXEL_FORMAT_R8G8B8A8_UNORM);
    uint8_t tmp_buf[4 * 4 * 4];

    d3dx_init_bcn_block_buffer(src_data, src_row_pitch, src_width, src_height, fmt_desc, tmp_buf);

    stb_compress_dxt_block(dst_data, (const uint8_t *)tmp_buf, FALSE, 0);
}

static void d3dx_compress_bc2_block(const void *src_data, uint32_t src_row_pitch, uint8_t src_width, uint8_t src_height,
        void *dst_data)
{
    const struct pixel_format_desc *fmt_desc = get_d3dx_pixel_format_info(D3DX_PIXEL_FORMAT_R8G8B8A8_UNORM);
    uint8_t *dst_data_offset = dst_data;
    uint8_t tmp_buf[4 * 4 * 4], y;

    d3dx_init_bcn_block_buffer(src_data, src_row_pitch, src_width, src_height, fmt_desc, tmp_buf);
    for (y = 0; y < 4; ++y)
    {
        uint8_t *tmp_row = &tmp_buf[y * 4 * fmt_desc->bytes_per_pixel];

        dst_data_offset[0]  = (tmp_row[7] & 0xf0);
        dst_data_offset[0] |= (tmp_row[3] >> 4);
        dst_data_offset[1]  = (tmp_row[15] & 0xf0);
        dst_data_offset[1] |= (tmp_row[11] >> 4);

        /*
         * Set all alpha values to 0xff so they aren't considered during
         * compression.
         */
        tmp_row[3] = tmp_row[7] = tmp_row[11] = tmp_row[15] = 0xff;
        dst_data_offset += 2;
    }

    stb_compress_dxt_block(dst_data_offset, (const unsigned char *)tmp_buf, FALSE, 0);
}

static void d3dx_compress_bc3_block(const void *src_data, uint32_t src_row_pitch, uint8_t src_width, uint8_t src_height,
        void *dst_data)
{
    const struct pixel_format_desc *fmt_desc = get_d3dx_pixel_format_info(D3DX_PIXEL_FORMAT_R8G8B8A8_UNORM);
    uint8_t tmp_buf[4 * 4 * 4];

    d3dx_init_bcn_block_buffer(src_data, src_row_pitch, src_width, src_height, fmt_desc, tmp_buf);

    stb_compress_dxt_block(dst_data, (const uint8_t *)tmp_buf, TRUE, 0);
}

static void d3dx_compress_bc4_block(const void *src_data, uint32_t src_row_pitch, uint8_t src_width, uint8_t src_height,
        void *dst_data)
{
    const struct pixel_format_desc *fmt_desc = get_d3dx_pixel_format_info(D3DX_PIXEL_FORMAT_R8_UNORM);
    uint8_t tmp_buf[4 * 4];

    d3dx_init_bcn_block_buffer(src_data, src_row_pitch, src_width, src_height, fmt_desc, tmp_buf);

    stb_compress_bc4_block(dst_data, (const unsigned char *)tmp_buf);
}

static void d3dx_compress_bc5_block(const void *src_data, uint32_t src_row_pitch, uint8_t src_width, uint8_t src_height,
        void *dst_data)
{
    const struct pixel_format_desc *fmt_desc = get_d3dx_pixel_format_info(D3DX_PIXEL_FORMAT_R8G8_UNORM);
    uint8_t tmp_buf[4 * 4 * 2];

    d3dx_init_bcn_block_buffer(src_data, src_row_pitch, src_width, src_height, fmt_desc, tmp_buf);

    stb_compress_bc5_block(dst_data, (const unsigned char *)tmp_buf);
}

static HRESULT d3dx_pixels_compress(struct d3dx_pixels *src_pixels,
        const struct pixel_format_desc *src_desc, struct d3dx_pixels *dst_pixels,
        const struct pixel_format_desc *dst_desc)
{
    void (*compress_bcn_block)(const void *, uint32_t, uint8_t, uint8_t, void *) = NULL;
    uint32_t x, y, z;

    /* Pick a compression function. */
    switch (dst_desc->format)
    {
        case D3DX_PIXEL_FORMAT_DXT1_UNORM:
        case D3DX_PIXEL_FORMAT_BC1_UNORM_SRGB:
            compress_bcn_block = d3dx_compress_bc1_block;
            break;

        case D3DX_PIXEL_FORMAT_DXT2_UNORM:
        case D3DX_PIXEL_FORMAT_DXT3_UNORM:
        case D3DX_PIXEL_FORMAT_BC2_UNORM_SRGB:
            compress_bcn_block = d3dx_compress_bc2_block;
            break;

        case D3DX_PIXEL_FORMAT_DXT4_UNORM:
        case D3DX_PIXEL_FORMAT_DXT5_UNORM:
        case D3DX_PIXEL_FORMAT_BC3_UNORM_SRGB:
            compress_bcn_block = d3dx_compress_bc3_block;
            break;

        case D3DX_PIXEL_FORMAT_BC4_UNORM:
        case D3DX_PIXEL_FORMAT_BC4_SNORM:
            compress_bcn_block = d3dx_compress_bc4_block;
            break;

        case D3DX_PIXEL_FORMAT_BC5_UNORM:
        case D3DX_PIXEL_FORMAT_BC5_SNORM:
            compress_bcn_block = d3dx_compress_bc5_block;
            break;

        default:
            FIXME("Unexpected compressed texture format %u.\n", dst_desc->format);
            return E_NOTIMPL;
    }

    assert(compress_bcn_block);

    TRACE("Compressing pixels.\n");
    for (z = 0; z < src_pixels->size.depth; ++z)
    {
        const BYTE *src_slice = ((const BYTE *)src_pixels->data) + (z * src_pixels->slice_pitch);
        BYTE *dst_slice = ((BYTE *)dst_pixels->data) + (z * dst_pixels->slice_pitch);

        for (y = 0; y < src_pixels->size.height; y += dst_desc->block_height)
        {
            BYTE *dst_ptr = &dst_slice[(y / dst_desc->block_height) * dst_pixels->row_pitch];
            uint8_t tmp_src_height = min(dst_desc->block_height, src_pixels->size.height - y);
            const BYTE *src_ptr = &src_slice[y * src_pixels->row_pitch];

            for (x = 0; x < src_pixels->size.width; x += dst_desc->block_width)
            {
                uint8_t tmp_src_width = min(dst_desc->block_width, src_pixels->size.width - x);

                compress_bcn_block(src_ptr, src_pixels->row_pitch, tmp_src_width, tmp_src_height, dst_ptr);
                src_ptr += (src_desc->bytes_per_pixel * dst_desc->block_width);
                dst_ptr += dst_desc->block_byte_count;
            }
        }
    }

    return S_OK;
}

HRESULT d3dx_pixels_init(const void *data, uint32_t row_pitch, uint32_t slice_pitch,
        const PALETTEENTRY *palette, enum d3dx_pixel_format_id format, uint32_t left, uint32_t top, uint32_t right,
        uint32_t bottom, uint32_t front, uint32_t back, struct d3dx_pixels *pixels)
{
    const struct pixel_format_desc *fmt_desc = get_d3dx_pixel_format_info(format);
    const BYTE *ptr = data;
    RECT unaligned_rect;

    memset(pixels, 0, sizeof(*pixels));
    if (is_unknown_format(fmt_desc))
    {
        FIXME("Unsupported format %#x.\n", format);
        return E_NOTIMPL;
    }

    ptr += front * slice_pitch;
    ptr += (top / fmt_desc->block_height) * row_pitch;
    ptr += (left / fmt_desc->block_width) * fmt_desc->block_byte_count;

    if (is_compressed_format(fmt_desc))
    {
        uint32_t left_aligned, top_aligned;

        top_aligned = top & ~(fmt_desc->block_height - 1);
        left_aligned = left & ~(fmt_desc->block_width - 1);
        SetRect(&unaligned_rect, left, top, right, bottom);
        OffsetRect(&unaligned_rect, -left_aligned, -top_aligned);
    }
    else
    {
        SetRect(&unaligned_rect, 0, 0, (right - left), (bottom - top));
    }

    set_d3dx_pixels(pixels, ptr, row_pitch, slice_pitch, palette, (right - left), (bottom - top), (back - front),
            &unaligned_rect);

    return S_OK;
}

static const char *debug_d3dx_pixels(struct d3dx_pixels *pixels)
{
    if (!pixels)
        return "(null)";
    return wine_dbg_sprintf("(data %p, row_pitch %d, slice_pitch %d, palette %p, width %d, height %d, depth %d, "
            "unaligned_rect %s)", pixels->data, pixels->row_pitch, pixels->slice_pitch, pixels->palette,
            pixels->size.width, pixels->size.height, pixels->size.depth, wine_dbgstr_rect(&pixels->unaligned_rect));
}

HRESULT d3dx_load_pixels_from_pixels(struct d3dx_pixels *dst_pixels,
       const struct pixel_format_desc *dst_desc, struct d3dx_pixels *src_pixels,
       const struct pixel_format_desc *src_desc, uint32_t filter_flags, uint32_t color_key)
{
    struct volume src_size, dst_size, dst_size_aligned;
    BOOL src_pma, dst_pma, src_srgb, dst_srgb;
    HRESULT hr = S_OK;

    TRACE("dst_pixels %s, dst_desc %p, src_pixels %s, src_desc %p, filter_flags %#x, color_key %#x.\n",
            debug_d3dx_pixels(dst_pixels), dst_desc, debug_d3dx_pixels(src_pixels), src_desc,
            filter_flags, color_key);

    if (src_desc->fmt_type_desc.fmt_flags & FMT_FLAG_SRGB)
        filter_flags |= D3DX_FILTER_SRGB_IN;
    if (src_desc->fmt_type_desc.fmt_flags & FMT_FLAG_PM_ALPHA)
        filter_flags |= D3DX_FILTER_PMA_IN;
    if (dst_desc->fmt_type_desc.fmt_flags & FMT_FLAG_SRGB)
        filter_flags |= D3DX_FILTER_SRGB_OUT;
    if (dst_desc->fmt_type_desc.fmt_flags & FMT_FLAG_PM_ALPHA)
        filter_flags |= D3DX_FILTER_PMA_OUT;

    src_pma = !!(filter_flags & D3DX_FILTER_PMA_IN);
    dst_pma = !!(filter_flags & D3DX_FILTER_PMA_OUT);
    src_srgb = !!(filter_flags & D3DX_FILTER_SRGB_IN);
    dst_srgb = !!(filter_flags & D3DX_FILTER_SRGB_OUT);

    if (is_compressed_format(src_desc))
        set_volume_struct(&src_size, (src_pixels->unaligned_rect.right - src_pixels->unaligned_rect.left),
                (src_pixels->unaligned_rect.bottom - src_pixels->unaligned_rect.top), src_pixels->size.depth);
    else
        src_size = src_pixels->size;

    dst_size_aligned = dst_pixels->size;
    if (is_compressed_format(dst_desc))
        set_volume_struct(&dst_size, (dst_pixels->unaligned_rect.right - dst_pixels->unaligned_rect.left),
                (dst_pixels->unaligned_rect.bottom - dst_pixels->unaligned_rect.top), dst_pixels->size.depth);
    else
        dst_size = dst_size_aligned;

    /* Everything matches, simply copy the pixels. */
    if (src_desc->format == dst_desc->format
            && (dst_size.width == src_size.width && !(dst_size.width % dst_desc->block_width))
            && (dst_size.height == src_size.height && !(dst_size.height % dst_desc->block_height))
            && (dst_size.depth == src_size.depth)
            && (src_pma == dst_pma)
            && (src_srgb == dst_srgb)
            && color_key == 0
            && !(src_pixels->unaligned_rect.left & (src_desc->block_width - 1))
            && !(src_pixels->unaligned_rect.top & (src_desc->block_height - 1))
            && !(dst_pixels->unaligned_rect.left & (dst_desc->block_width - 1))
            && !(dst_pixels->unaligned_rect.top & (dst_desc->block_height - 1)))
    {
        TRACE("Simple copy.\n");
        copy_pixels(src_pixels->data, src_pixels->row_pitch, src_pixels->slice_pitch, (void *)dst_pixels->data,
                dst_pixels->row_pitch, dst_pixels->slice_pitch, &src_size, src_desc);
        return S_OK;
    }

    /* Stretching or format conversion. */
    if (!is_conversion_from_supported(src_desc)
            || !is_conversion_to_supported(dst_desc))
    {
        FIXME("Unsupported format conversion %#x -> %#x.\n", src_desc->format, dst_desc->format);
        return E_NOTIMPL;
    }

    /*
     * If the source is a compressed image, we need to decompress it first
     * before doing any modifications.
     */
    if (is_compressed_format(src_desc))
    {
        uint32_t uncompressed_row_pitch, uncompressed_slice_pitch;
        const struct pixel_format_desc *uncompressed_desc;
        void *uncompressed_mem = NULL;

        hr = d3dx_pixels_decompress(src_pixels, src_desc, FALSE, &uncompressed_mem, &uncompressed_row_pitch,
                &uncompressed_slice_pitch, &uncompressed_desc);
        if (SUCCEEDED(hr))
        {
            struct d3dx_pixels uncompressed_pixels;

            d3dx_pixels_init(uncompressed_mem, uncompressed_row_pitch, uncompressed_slice_pitch, NULL,
                    uncompressed_desc->format, 0, 0, src_pixels->size.width, src_pixels->size.height,
                    0, src_pixels->size.depth, &uncompressed_pixels);

            hr = d3dx_load_pixels_from_pixels(dst_pixels, dst_desc, &uncompressed_pixels, uncompressed_desc,
                    filter_flags, color_key);
        }
        free(uncompressed_mem);
        goto exit;
    }

    /* Same as the above, need to decompress the destination prior to modifying. */
    if (is_compressed_format(dst_desc))
    {
        uint32_t uncompressed_row_pitch, uncompressed_slice_pitch;
        const struct pixel_format_desc *uncompressed_desc;
        struct d3dx_pixels uncompressed_pixels;
        void *uncompressed_mem = NULL;

        hr = d3dx_pixels_decompress(dst_pixels, dst_desc, TRUE, &uncompressed_mem, &uncompressed_row_pitch,
                &uncompressed_slice_pitch, &uncompressed_desc);
        if (FAILED(hr))
            goto exit;

        d3dx_pixels_init(uncompressed_mem, uncompressed_row_pitch, uncompressed_slice_pitch, NULL,
                uncompressed_desc->format, dst_pixels->unaligned_rect.left, dst_pixels->unaligned_rect.top,
                dst_pixels->unaligned_rect.right, dst_pixels->unaligned_rect.bottom, 0, dst_pixels->size.depth,
                &uncompressed_pixels);

        hr = d3dx_load_pixels_from_pixels(&uncompressed_pixels, uncompressed_desc, src_pixels, src_desc, filter_flags,
                color_key);
        if (SUCCEEDED(hr))
        {
            d3dx_pixels_init(uncompressed_mem, uncompressed_row_pitch, uncompressed_slice_pitch, NULL,
                    uncompressed_desc->format, 0, 0, dst_size_aligned.width, dst_size_aligned.height, 0,
                    dst_pixels->size.depth, &uncompressed_pixels);

            hr = d3dx_pixels_compress(&uncompressed_pixels, uncompressed_desc, dst_pixels, dst_desc);
            if (FAILED(hr))
                WARN("Failed to compress pixels, hr %#lx.\n", hr);
        }
        free(uncompressed_mem);
        goto exit;
    }

    if ((filter_flags & 0xf) == D3DX_FILTER_NONE)
    {
        convert_argb_pixels(src_pixels->data, src_pixels->row_pitch, src_pixels->slice_pitch, &src_size, src_desc,
                (BYTE *)dst_pixels->data, dst_pixels->row_pitch, dst_pixels->slice_pitch, &dst_size, dst_desc,
                color_key, src_pixels->palette, filter_flags);
    }
    else /* if ((filter & 0xf) == D3DX_FILTER_POINT) */
    {
        if ((filter_flags & 0xf) != D3DX_FILTER_POINT)
            FIXME("Unhandled filter %#x.\n", filter_flags);

        /* Always apply a point filter until D3DX_FILTER_LINEAR,
         * D3DX_FILTER_TRIANGLE and D3DX_FILTER_BOX are implemented. */
        point_filter_argb_pixels(src_pixels->data, src_pixels->row_pitch, src_pixels->slice_pitch, &src_size,
                src_desc, (BYTE *)dst_pixels->data, dst_pixels->row_pitch, dst_pixels->slice_pitch, &dst_size,
                dst_desc, color_key, src_pixels->palette, filter_flags);
    }

exit:
    if (FAILED(hr))
        WARN("Failed to load pixels, hr %#lx.\n", hr);
    return hr;
}

void get_aligned_rect(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom, uint32_t width, uint32_t height,
        const struct pixel_format_desc *fmt_desc, RECT *aligned_rect)
{
    SetRect(aligned_rect, left, top, right, bottom);
    if (aligned_rect->left & (fmt_desc->block_width - 1))
        aligned_rect->left = aligned_rect->left & ~(fmt_desc->block_width - 1);
    if (aligned_rect->top & (fmt_desc->block_height - 1))
        aligned_rect->top = aligned_rect->top & ~(fmt_desc->block_height - 1);
    if (aligned_rect->right & (fmt_desc->block_width - 1) && aligned_rect->right != width)
        aligned_rect->right = min((aligned_rect->right + fmt_desc->block_width - 1)
                & ~(fmt_desc->block_width - 1), width);
    if (aligned_rect->bottom & (fmt_desc->block_height - 1) && aligned_rect->bottom != height)
        aligned_rect->bottom = min((aligned_rect->bottom + fmt_desc->block_height - 1)
                & ~(fmt_desc->block_height - 1), height);
}

/* File/resource loading functions shared amongst d3dx10/d3dx11. */
HRESULT d3dx_load_file(const WCHAR *path, void **data, DWORD *size)
{
    DWORD read_len;
    HANDLE file;
    BOOL ret;

    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return D3DX_HELPER_ERR_FILE_NOT_FOUND;

    *size = GetFileSize(file, NULL);
    *data = malloc(*size);
    if (!*data)
    {
        CloseHandle(file);
        return E_OUTOFMEMORY;
    }

    ret = ReadFile(file, *data, *size, &read_len, NULL);
    CloseHandle(file);
    if (!ret || read_len != *size)
    {
        WARN("Failed to read file contents.\n");
        free(*data);
        return E_FAIL;
    }
    return S_OK;
}

HRESULT d3dx_load_resource_initA(HMODULE module, const char *resource, HRSRC *rsrc)
{
    if (!(*rsrc = FindResourceA(module, resource, (const char *)RT_RCDATA)))
        *rsrc = FindResourceA(module, resource, (const char *)RT_BITMAP);
    if (!*rsrc)
    {
        WARN("Failed to find resource.\n");
        return D3DX_HELPER_ERR_INVALID_DATA;
    }
    return S_OK;
}

HRESULT d3dx_load_resource_initW(HMODULE module, const WCHAR *resource, HRSRC *rsrc)
{
    if (!(*rsrc = FindResourceW(module, resource, (const WCHAR *)RT_RCDATA)))
        *rsrc = FindResourceW(module, resource, (const WCHAR *)RT_BITMAP);
    if (!*rsrc)
    {
        WARN("Failed to find resource.\n");
        return D3DX_HELPER_ERR_INVALID_DATA;
    }
    return S_OK;
}

HRESULT d3dx_load_resource(HMODULE module, HRSRC rsrc, void **data, DWORD *size)
{
    HGLOBAL hglobal;

    if (!(*size = SizeofResource(module, rsrc)))
        return D3DX_HELPER_ERR_INVALID_DATA;
    if (!(hglobal = LoadResource(module, rsrc)))
        return D3DX_HELPER_ERR_INVALID_DATA;
    if (!(*data = LockResource(hglobal)))
        return D3DX_HELPER_ERR_INVALID_DATA;
    return S_OK;
}

HRESULT d3dx_load_resourceA(HMODULE module, const char *resource, void **data, DWORD *size)
{
    HRESULT hr;
    HRSRC rsrc;

    if (FAILED((hr = d3dx_load_resource_initA(module, resource, &rsrc))))
        return hr;
    return d3dx_load_resource(module, rsrc, data, size);
}

HRESULT d3dx_load_resourceW(HMODULE module, const WCHAR *resource, void **data, DWORD *size)
{
    HRESULT hr;
    HRSRC rsrc;

    if ((FAILED(hr = d3dx_load_resource_initW(module, resource, &rsrc))))
        return hr;
    return d3dx_load_resource(module, rsrc, data, size);
}
