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

#include <stdint.h>
#include "windef.h" /* For RECT. */
#include "wingdi.h" /* For PALETTEENTRY. */

struct volume
{
    UINT width;
    UINT height;
    UINT depth;
};

static inline void set_volume_struct(struct volume *volume, uint32_t width, uint32_t height, uint32_t depth)
{
    volume->width = width;
    volume->height = height;
    volume->depth = depth;
}

/* These are custom Wine only filter flags. */
#define D3DX_FILTER_PMA_IN  0x00800000
#define D3DX_FILTER_PMA_OUT 0x01000000
#define D3DX_FILTER_PMA     0x01800000

/* These values act as indexes into the pixel_format_desc table. */
enum d3dx_pixel_format_id {
    D3DX_PIXEL_FORMAT_B8G8R8_UNORM,
    D3DX_PIXEL_FORMAT_B8G8R8A8_UNORM,
    D3DX_PIXEL_FORMAT_B8G8R8X8_UNORM,
    D3DX_PIXEL_FORMAT_R8G8B8A8_UNORM,
    D3DX_PIXEL_FORMAT_R8G8B8X8_UNORM,
    D3DX_PIXEL_FORMAT_B5G6R5_UNORM,
    D3DX_PIXEL_FORMAT_B5G5R5X1_UNORM,
    D3DX_PIXEL_FORMAT_B5G5R5A1_UNORM,
    D3DX_PIXEL_FORMAT_B2G3R3_UNORM,
    D3DX_PIXEL_FORMAT_B2G3R3A8_UNORM,
    D3DX_PIXEL_FORMAT_B4G4R4A4_UNORM,
    D3DX_PIXEL_FORMAT_B4G4R4X4_UNORM,
    D3DX_PIXEL_FORMAT_B10G10R10A2_UNORM,
    D3DX_PIXEL_FORMAT_R10G10B10A2_UNORM,
    D3DX_PIXEL_FORMAT_R16G16B16A16_UNORM,
    D3DX_PIXEL_FORMAT_R8_UNORM,
    D3DX_PIXEL_FORMAT_R8G8_UNORM,
    D3DX_PIXEL_FORMAT_R16_UNORM,
    D3DX_PIXEL_FORMAT_R16G16_UNORM,
    D3DX_PIXEL_FORMAT_A8_UNORM,
    D3DX_PIXEL_FORMAT_DXT1_UNORM,
    D3DX_PIXEL_FORMAT_DXT2_UNORM,
    D3DX_PIXEL_FORMAT_DXT3_UNORM,
    D3DX_PIXEL_FORMAT_DXT4_UNORM,
    D3DX_PIXEL_FORMAT_DXT5_UNORM,
    D3DX_PIXEL_FORMAT_BC4_UNORM,
    D3DX_PIXEL_FORMAT_BC4_SNORM,
    D3DX_PIXEL_FORMAT_BC5_UNORM,
    D3DX_PIXEL_FORMAT_BC5_SNORM,
    D3DX_PIXEL_FORMAT_R16_FLOAT,
    D3DX_PIXEL_FORMAT_R16G16_FLOAT,
    D3DX_PIXEL_FORMAT_R16G16B16A16_FLOAT,
    D3DX_PIXEL_FORMAT_R32_FLOAT,
    D3DX_PIXEL_FORMAT_R32G32_FLOAT,
    D3DX_PIXEL_FORMAT_R32G32B32_FLOAT,
    D3DX_PIXEL_FORMAT_R32G32B32A32_FLOAT,
    D3DX_PIXEL_FORMAT_L8A8_UNORM,
    D3DX_PIXEL_FORMAT_L4A4_UNORM,
    D3DX_PIXEL_FORMAT_L8_UNORM,
    D3DX_PIXEL_FORMAT_L16_UNORM,
    D3DX_PIXEL_FORMAT_P8_UINT,
    D3DX_PIXEL_FORMAT_P8_UINT_A8_UNORM,
    D3DX_PIXEL_FORMAT_U8V8W8Q8_SNORM,
    D3DX_PIXEL_FORMAT_U8V8_SNORM,
    D3DX_PIXEL_FORMAT_U16V16_SNORM,
    D3DX_PIXEL_FORMAT_U8V8_SNORM_L8X8_UNORM,
    D3DX_PIXEL_FORMAT_U10V10W10_SNORM_A2_UNORM,
    D3DX_PIXEL_FORMAT_U16V16W16Q16_SNORM,
    D3DX_PIXEL_FORMAT_R8G8_B8G8_UNORM,
    D3DX_PIXEL_FORMAT_G8R8_G8B8_UNORM,
    D3DX_PIXEL_FORMAT_UYVY,
    D3DX_PIXEL_FORMAT_YUY2,
    D3DX_PIXEL_FORMAT_COUNT,
};

/* These are aliases. */
#define D3DX_PIXEL_FORMAT_R16G16B16A16_SNORM D3DX_PIXEL_FORMAT_U16V16W16Q16_SNORM
#define D3DX_PIXEL_FORMAT_R16G16_SNORM       D3DX_PIXEL_FORMAT_U16V16_SNORM
#define D3DX_PIXEL_FORMAT_R8G8B8A8_SNORM     D3DX_PIXEL_FORMAT_U8V8W8Q8_SNORM
#define D3DX_PIXEL_FORMAT_R8G8_SNORM         D3DX_PIXEL_FORMAT_U8V8_SNORM


enum d3dx_resource_type {
    D3DX_RESOURCE_TYPE_UNKNOWN,
    D3DX_RESOURCE_TYPE_TEXTURE_1D,
    D3DX_RESOURCE_TYPE_TEXTURE_2D,
    D3DX_RESOURCE_TYPE_TEXTURE_3D,
    D3DX_RESOURCE_TYPE_CUBE_TEXTURE,
};

enum d3dx_image_file_format
{
    D3DX_IMAGE_FILE_FORMAT_BMP  = 0,
    D3DX_IMAGE_FILE_FORMAT_JPG  = 1,
    D3DX_IMAGE_FILE_FORMAT_TGA  = 2,
    D3DX_IMAGE_FILE_FORMAT_PNG  = 3,
    D3DX_IMAGE_FILE_FORMAT_DDS  = 4,
    D3DX_IMAGE_FILE_FORMAT_PPM  = 5,
    D3DX_IMAGE_FILE_FORMAT_DIB  = 6,
    D3DX_IMAGE_FILE_FORMAT_HDR  = 7,
    D3DX_IMAGE_FILE_FORMAT_PFM  = 8,
    D3DX_IMAGE_FILE_FORMAT_TIFF = 10,
    D3DX_IMAGE_FILE_FORMAT_GIF  = 11,
    D3DX_IMAGE_FILE_FORMAT_WMP  = 12,
    /* This is a Wine only file format value. */
    D3DX_IMAGE_FILE_FORMAT_DDS_DXT10 = 100,
    D3DX_IMAGE_FILE_FORMAT_FORCE_DWORD = 0x7fffffff
};

enum component_type {
    CTYPE_EMPTY = 0x00,
    CTYPE_UNORM = 0x01,
    CTYPE_SNORM = 0x02,
    CTYPE_FLOAT = 0x03,
    CTYPE_LUMA  = 0x04,
    CTYPE_INDEX = 0x05,
};

enum format_flag {
    FMT_FLAG_NONE = 0x00,
    FMT_FLAG_DXT  = 0x01,
    FMT_FLAG_PACKED = 0x02,
    FMT_FLAG_PM_ALPHA = 0x04,
    /* For formats that only have a DXGI_FORMAT mapping, no D3DFORMAT equivalent. */
    FMT_FLAG_DXGI     = 0x08,
};

#define FMT_FLAG_PMA_DXT (FMT_FLAG_DXT | FMT_FLAG_PM_ALPHA)
#define FMT_FLAG_DXGI_DXT (FMT_FLAG_DXGI | FMT_FLAG_DXT)

struct pixel_format_type_desc {
    enum component_type a_type;
    enum component_type rgb_type;
    uint32_t fmt_flags;
};

struct pixel_format_desc {
    enum d3dx_pixel_format_id format;
    BYTE bits[4];
    BYTE shift[4];
    UINT bytes_per_pixel;
    UINT block_width;
    UINT block_height;
    UINT block_byte_count;
    struct pixel_format_type_desc fmt_type_desc;
};

struct d3dx_pixels
{
    const void *data;
    uint32_t row_pitch;
    uint32_t slice_pitch;
    const PALETTEENTRY *palette;

    struct volume size;
    RECT unaligned_rect;
};

static inline void set_d3dx_pixels(struct d3dx_pixels *pixels, const void *data, uint32_t row_pitch,
        uint32_t slice_pitch, const PALETTEENTRY *palette, uint32_t width, uint32_t height, uint32_t depth,
        const RECT *unaligned_rect)
{
    pixels->data = data;
    pixels->row_pitch = row_pitch;
    pixels->slice_pitch = slice_pitch;
    pixels->palette = palette;
    set_volume_struct(&pixels->size, width, height, depth);
    pixels->unaligned_rect = *unaligned_rect;
}

#define D3DX_IMAGE_INFO_ONLY 1
#define D3DX_IMAGE_SUPPORT_DXT10 2
struct d3dx_image
{
    enum d3dx_resource_type resource_type;
    enum d3dx_pixel_format_id format;

    struct volume size;
    uint32_t mip_levels;
    uint32_t layer_count;

    BYTE *pixels;
    PALETTEENTRY *palette;
    uint32_t layer_pitch;

    /*
     * image_buf and image_palette are pointers to allocated memory used to store
     * image data. If they are non-NULL, they need to be freed when no longer
     * in use.
     */
    void *image_buf;
    PALETTEENTRY *image_palette;

    enum d3dx_image_file_format image_file_format;
};

HRESULT d3dx_image_init(const void *src_data, uint32_t src_data_size, struct d3dx_image *image,
        uint32_t starting_mip_level, uint32_t flags);
void d3dx_image_cleanup(struct d3dx_image *image);
HRESULT d3dx_image_get_pixels(struct d3dx_image *image, uint32_t element, uint32_t mip_level,
        struct d3dx_pixels *pixels);
