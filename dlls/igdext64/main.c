/*
 * igdext64.dll implementation
 * 
 * Copyright 2024 Etaash Mathamsetty
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

#include <stdarg.h>
#include <stdlib.h>

#include "windef.h"
#include "winbase.h"
#include "wine/debug.h"

#include "d3d12.h"
#include "d3d11.h"
#include "intel.h"

WINE_DEFAULT_DEBUG_CHANNEL(igdext64);


HRESULT WINAPI D3D11EnumInternalExtensions(void *unk, UINT *unk2)
{
    FIXME("%p %p stub!\n", unk, unk2);

    if(*unk2 == 0)
    {
        return S_OK;
    }

    if(!unk)
    {
        return E_OUTOFMEMORY;
    }

    return S_OK;
}

HRESULT WINAPI D3D12EnumInternalExtensions(void *unk, UINT *unk2)
{
    FIXME("%p %p stub!\n", unk, unk2);

    if(*unk2 == 0)
    {
        return S_OK;
    }

    if(!unk)
    {
        return E_OUTOFMEMORY;
    }

    return S_OK;
}

void WINAPI D3D12GetSupportedVersions2(void *unk, INTCExtensionVersion *unk2, int *unk3)
{
    FIXME("%p %p %p stub!\n", unk, unk2, unk3);

    if(!unk2)
    {
        *unk3 = 1;
        return;
    }

    /* random numbers */
    unk2->APIVersion = 64000000;
    unk2->HWFeatureLevel = 64000000;
    unk2->Revision = 10000000;
}

HRESULT WINAPI _INTC_D3D12_CreateDeviceExtensionContext(ID3D12Device *device, INTCExtensionContext** context, INTCExtensionInfo* info, INTCExtensionAppInfo* app_info)
{
    FIXME("%p %p %p %p stub!\n", device, context, info, app_info);

    *context = malloc(sizeof(INTCExtensionContext));

    if(info)
        (*context)->info = *info;
    if(app_info)
        (*context)->app_info = *app_info;

    return S_OK;
}

HRESULT WINAPI _INTC_D3D11_CreateDeviceExtensionContext(ID3D11Device *device, INTCExtensionContext** context, INTCExtensionInfo* info, INTCExtensionAppInfo* app_info)
{
    FIXME("%p %p %p %p stub!\n", device, context, info, app_info);

    *context = malloc(sizeof(INTCExtensionContext));

    if(info)
        (*context)->info = *info;
    if(app_info)
        (*context)->app_info = *app_info;

    return S_OK;
}
