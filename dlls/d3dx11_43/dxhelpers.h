/*
 * Copyright 2025 Connor McAdams for CodeWeavers
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

#include "d3dx_helpers.h"

#define D3DERR_INVALIDCALL  0x8876086c

HRESULT load_file(const WCHAR *path, void **data, DWORD *size);

HRESULT get_image_info(const void *data, SIZE_T size, D3DX11_IMAGE_INFO *img_info);

void init_load_info(const D3DX11_IMAGE_LOAD_INFO *load_info,
        D3DX11_IMAGE_LOAD_INFO *out);
/* Returns array of D3D11_SUBRESOURCE_DATA structures followed by textures data. */
HRESULT load_texture_data(const void *data, SIZE_T size, D3DX11_IMAGE_LOAD_INFO *load_info,
        D3D11_SUBRESOURCE_DATA **resource_data);
HRESULT create_d3d_texture(ID3D11Device *device, D3DX11_IMAGE_LOAD_INFO *load_info,
        D3D11_SUBRESOURCE_DATA *resource_data, ID3D11Resource **texture);
