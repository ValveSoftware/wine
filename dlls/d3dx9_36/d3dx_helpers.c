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
    {D3DX_PIXEL_FORMAT_B8G8R8X8_UNORM,           { 0,  8,  8,  8}, { 0, 16,  8,  0},  4, 1, 1,  4, { CTYPE_EMPTY, CTYPE_UNORM, FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_R8G8B8A8_UNORM,           { 8,  8,  8,  8}, {24,  0,  8, 16},  4, 1, 1,  4, { CTYPE_UNORM, CTYPE_UNORM, FMT_FLAG_NONE    }},
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
    {D3DX_PIXEL_FORMAT_R16G16_UNORM,             { 0, 16, 16,  0}, { 0,  0, 16,  0},  4, 1, 1,  4, { CTYPE_EMPTY, CTYPE_UNORM, FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_A8_UNORM,                 { 8,  0,  0,  0}, { 0,  0,  0,  0},  1, 1, 1,  1, { CTYPE_UNORM, CTYPE_EMPTY, FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_DXT1_UNORM,               { 0,  0,  0,  0}, { 0,  0,  0,  0},  1, 4, 4,  8, { CTYPE_UNORM, CTYPE_UNORM, FMT_FLAG_DXT     }},
    {D3DX_PIXEL_FORMAT_DXT2_UNORM,               { 0,  0,  0,  0}, { 0,  0,  0,  0},  1, 4, 4, 16, { CTYPE_UNORM, CTYPE_UNORM, FMT_FLAG_PMA_DXT }},
    {D3DX_PIXEL_FORMAT_DXT3_UNORM,               { 0,  0,  0,  0}, { 0,  0,  0,  0},  1, 4, 4, 16, { CTYPE_UNORM, CTYPE_UNORM, FMT_FLAG_DXT     }},
    {D3DX_PIXEL_FORMAT_DXT4_UNORM,               { 0,  0,  0,  0}, { 0,  0,  0,  0},  1, 4, 4, 16, { CTYPE_UNORM, CTYPE_UNORM, FMT_FLAG_PMA_DXT }},
    {D3DX_PIXEL_FORMAT_DXT5_UNORM,               { 0,  0,  0,  0}, { 0,  0,  0,  0},  1, 4, 4, 16, { CTYPE_UNORM, CTYPE_UNORM, FMT_FLAG_DXT     }},
    {D3DX_PIXEL_FORMAT_R16_FLOAT,                { 0, 16,  0,  0}, { 0,  0,  0,  0},  2, 1, 1,  2, { CTYPE_EMPTY, CTYPE_FLOAT, FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_R16G16_FLOAT,             { 0, 16, 16,  0}, { 0,  0, 16,  0},  4, 1, 1,  4, { CTYPE_EMPTY, CTYPE_FLOAT, FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_R16G16B16A16_FLOAT,       {16, 16, 16, 16}, {48,  0, 16, 32},  8, 1, 1,  8, { CTYPE_FLOAT, CTYPE_FLOAT, FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_R32_FLOAT,                { 0, 32,  0,  0}, { 0,  0,  0,  0},  4, 1, 1,  4, { CTYPE_EMPTY, CTYPE_FLOAT, FMT_FLAG_NONE    }},
    {D3DX_PIXEL_FORMAT_R32G32_FLOAT,             { 0, 32, 32,  0}, { 0,  0, 32,  0},  8, 1, 1,  8, { CTYPE_EMPTY, CTYPE_FLOAT, FMT_FLAG_NONE    }},
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

static enum d3dx_pixel_format_id d3dx_pixel_format_id_from_dxgi_format(DXGI_FORMAT format)
{
    switch (format)
    {
    case DXGI_FORMAT_R8G8B8A8_UNORM:           return D3DX_PIXEL_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_B8G8R8A8_UNORM:           return D3DX_PIXEL_FORMAT_B8G8R8A8_UNORM;
    case DXGI_FORMAT_B8G8R8X8_UNORM:           return D3DX_PIXEL_FORMAT_B8G8R8X8_UNORM;
    case DXGI_FORMAT_B5G6R5_UNORM:             return D3DX_PIXEL_FORMAT_B5G6R5_UNORM;
    case DXGI_FORMAT_B5G5R5A1_UNORM:           return D3DX_PIXEL_FORMAT_B5G5R5A1_UNORM;
    case DXGI_FORMAT_B4G4R4A4_UNORM:           return D3DX_PIXEL_FORMAT_B4G4R4A4_UNORM;
    case DXGI_FORMAT_R10G10B10A2_UNORM:        return D3DX_PIXEL_FORMAT_R10G10B10A2_UNORM;
    case DXGI_FORMAT_R16G16B16A16_UNORM:       return D3DX_PIXEL_FORMAT_R16G16B16A16_UNORM;
    case DXGI_FORMAT_R16G16_UNORM:             return D3DX_PIXEL_FORMAT_R16G16_UNORM;
    case DXGI_FORMAT_A8_UNORM:                 return D3DX_PIXEL_FORMAT_A8_UNORM;
    case DXGI_FORMAT_R16_FLOAT:                return D3DX_PIXEL_FORMAT_R16_FLOAT;
    case DXGI_FORMAT_R16G16_FLOAT:             return D3DX_PIXEL_FORMAT_R16G16_FLOAT;
    case DXGI_FORMAT_R16G16B16A16_FLOAT:       return D3DX_PIXEL_FORMAT_R16G16B16A16_FLOAT;
    case DXGI_FORMAT_R32_FLOAT:                return D3DX_PIXEL_FORMAT_R32_FLOAT;
    case DXGI_FORMAT_R32G32_FLOAT:             return D3DX_PIXEL_FORMAT_R32G32_FLOAT;
    case DXGI_FORMAT_R32G32B32A32_FLOAT:       return D3DX_PIXEL_FORMAT_R32G32B32A32_FLOAT;
    case DXGI_FORMAT_G8R8_G8B8_UNORM:          return D3DX_PIXEL_FORMAT_G8R8_G8B8_UNORM;
    case DXGI_FORMAT_R8G8_B8G8_UNORM:          return D3DX_PIXEL_FORMAT_R8G8_B8G8_UNORM;
    case DXGI_FORMAT_BC1_UNORM:                return D3DX_PIXEL_FORMAT_DXT1_UNORM;
    case DXGI_FORMAT_BC2_UNORM:                return D3DX_PIXEL_FORMAT_DXT3_UNORM;
    case DXGI_FORMAT_BC3_UNORM:                return D3DX_PIXEL_FORMAT_DXT5_UNORM;
    case DXGI_FORMAT_R8G8B8A8_SNORM:           return D3DX_PIXEL_FORMAT_R8G8B8A8_SNORM;
    case DXGI_FORMAT_R8G8_SNORM:               return D3DX_PIXEL_FORMAT_R8G8_SNORM;
    case DXGI_FORMAT_R16G16_SNORM:             return D3DX_PIXEL_FORMAT_R16G16_SNORM;
    case DXGI_FORMAT_R16G16B16A16_SNORM:       return D3DX_PIXEL_FORMAT_R16G16B16A16_SNORM;

    default:
        FIXME("Unhandled DXGI format %#x.\n", format);
        return D3DX_PIXEL_FORMAT_COUNT;
    }
}

static void d3dx_get_next_mip_level_size(struct volume *size)
{
    size->width  = max(size->width  / 2, 1);
    size->height = max(size->height / 2, 1);
    size->depth  = max(size->depth  / 2, 1);
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

static uint32_t d3dx_calculate_layer_pixels_size(enum d3dx_pixel_format_id format, uint32_t width, uint32_t height, uint32_t depth,
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

struct d3dx_wic_file_format
{
    const GUID *wic_container_guid;
    enum d3dx_image_file_format d3dx_file_format;
};

/* Sorted by GUID. */
static const struct d3dx_wic_file_format file_formats[] =
{
    { &GUID_ContainerFormatBmp,     D3DX_IMAGE_FILE_FORMAT_BMP },
    { &GUID_WineContainerFormatTga, D3DX_IMAGE_FILE_FORMAT_TGA },
    { &GUID_ContainerFormatJpeg,    D3DX_IMAGE_FILE_FORMAT_JPG },
    { &GUID_ContainerFormatPng,     D3DX_IMAGE_FILE_FORMAT_PNG },
};

static int __cdecl d3dx_wic_file_format_guid_compare(const void *a, const void *b)
{
    const struct d3dx_wic_file_format *format = b;
    const GUID *guid = a;

    return memcmp(guid, format->wic_container_guid, sizeof(*guid));
}

static enum d3dx_image_file_format wic_container_guid_to_d3dx_image_file_format(GUID *container_format)
{
    struct d3dx_wic_file_format *format;

    if ((format = bsearch(container_format, file_formats, ARRAY_SIZE(file_formats), sizeof(*format),
            d3dx_wic_file_format_guid_compare)))
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

    image->image_file_format = wic_container_guid_to_d3dx_image_file_format(&container_format);
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

    if (image_is_argb(bitmap_frame, image))
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
