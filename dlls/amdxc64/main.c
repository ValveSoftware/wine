/*
 * amdxc implementation
 *
 * Copyright 2023 Etaash Mathamsetty
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

#include "ntstatus.h"
#include "winerror.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "wine/debug.h"

#define COBJMACROS
#include "initguid.h"
#include "d3d12.h"
#include "dxgi1_4.h"

#include "amdxc_interfaces.h"

WINE_DEFAULT_DEBUG_CHANNEL(amdxc);

struct AMDFSR4FFX
{
    IAmdExtFfxApi IAmdExtFfxApi_iface;
    LONG ref;
    LUID luid;
};

static struct AMDFSR4FFX* impl_from_IAmdExtFfxApi(IAmdExtFfxApi* iface)
{
    return CONTAINING_RECORD(iface, struct AMDFSR4FFX, IAmdExtFfxApi_iface);
}

ULONG STDMETHODCALLTYPE AMDFSR4FFX_AddRef(IAmdExtFfxApi *iface)
{
    struct AMDFSR4FFX* data = impl_from_IAmdExtFfxApi(iface);
    return InterlockedIncrement(&data->ref);
}

ULONG STDMETHODCALLTYPE AMDFSR4FFX_Release(IAmdExtFfxApi *iface)
{
    struct AMDFSR4FFX* data = impl_from_IAmdExtFfxApi(iface);
    ULONG ret = InterlockedDecrement(&data->ref);
    if (!ret) free(data);
    return ret;
}

HRESULT STDMETHODCALLTYPE AMDFSR4FFX_QueryInterface(IAmdExtFfxApi *iface, REFIID iid, void **obj)
{
    FIXME("%p %s %p", iface, debugstr_guid(iid), obj);

    return E_NOINTERFACE;
}

static BOOL is_device_supported(LUID luid)
{
    IDXGIFactory4 *factory;
    IDXGIAdapter *adapter;
    DXGI_ADAPTER_DESC desc;
    BOOL ret = FALSE;
    HRESULT hr;

    hr = CreateDXGIFactory2(0, &IID_IDXGIFactory4, (void **)&factory);
    if (hr != S_OK)
    {
        ERR("Failed to create DXGI Factory: %#lx!\n", hr);
        return FALSE;
    }

    hr = IDXGIFactory4_EnumAdapterByLuid(factory, luid, &IID_IDXGIAdapter, (void**) &adapter);
    if (hr != S_OK)
    {
        ERR("Failed to create find adapter by LUID %08lx:%08lx: %#lx!\n", luid.HighPart, luid.LowPart, hr);
        IDXGIFactory4_Release(factory);
        return FALSE;
    }

    IDXGIAdapter_GetDesc(adapter, &desc);

    IDXGIAdapter_Release(adapter);
    IDXGIFactory4_Release(factory);

    TRACE("The device vid:pid is %#x:%#x.\n", desc.VendorId, desc.DeviceId);

    /* supported discrete NAVI3s */
    if (desc.VendorId == 0x1002 &&
            (desc.DeviceId == 0x73f0 || (desc.DeviceId >= 0x7440 && desc.DeviceId <= 0x749f))
        ) {
        TRACE("Device supported.\n");
        return TRUE;
    }

    TRACE("Device may be unsupported.\n");

    return ret;
}

typedef HRESULT (__stdcall *updateffxapi_pfn)(void*, unsigned int);

HRESULT STDMETHODCALLTYPE AMDFSR4FFX_UpdateFfxApiProvider(IAmdExtFfxApi *iface, void* data, unsigned int size)
{
    static int once;
    const char *env;
    struct AMDFSR4FFX* amdfs4ffx_data = impl_from_IAmdExtFfxApi(iface);
    updateffxapi_pfn pfn;
    HMODULE amdffx;

    TRACE("%p %p %u\n", iface, data, size);

    env = getenv("FSR4_UPGRADE");

    /* either FSR4_UPGRADE=1 or unset but on a supported device */
    if ((!env && is_device_supported(amdfs4ffx_data->luid)) ||
        (env && !strcmp(env, "1")))
    {
        amdffx = LoadLibraryA("amdxcffx64");
        if (!amdffx)
        {
            ERR("Failed to load FSR4 dll (amdxcffx)!\n");
            return E_NOINTERFACE;
        }

        pfn = (updateffxapi_pfn)GetProcAddress(amdffx, "UpdateFfxApiProvider");

        if (pfn)
        {
            if (!once++) WARN("Replaced FSR3 with FSR4!\n");
            return pfn(data, size);
        }
    }

    return E_NOINTERFACE;
}

static const struct IAmdExtFfxApiVtbl AMDFSR4FFX_vtable = {
    AMDFSR4FFX_QueryInterface,
    AMDFSR4FFX_AddRef,
    AMDFSR4FFX_Release,
    AMDFSR4FFX_UpdateFfxApiProvider
};

struct AMDExtStub2
{
    IAmdExtStub2 IAmdExtStub2_iface;
    LONG ref;
};

struct AMDExtStub2* impl_from_IAMDExtStub2(IAmdExtStub2 *iface)
{
    return CONTAINING_RECORD(iface, struct AMDExtStub2, IAmdExtStub2_iface);
}

ULONG STDMETHODCALLTYPE AMDExtStub2_AddRef(IAmdExtStub2 *iface)
{
    struct AMDExtStub2 *this = impl_from_IAMDExtStub2(iface);
    return InterlockedIncrement(&this->ref);
}

ULONG STDMETHODCALLTYPE AMDExtStub2_Release(IAmdExtStub2 *iface)
{
    struct AMDExtStub2 *this = impl_from_IAMDExtStub2(iface);
    ULONG ret = InterlockedDecrement(&this->ref);
    if (!ret) free(this);
    return ret;
}

HRESULT STDMETHODCALLTYPE AMDExtStub2_QueryInterface(IAmdExtStub2 *iface, REFIID iid, void **out)
{
    FIXME("%p %s %p stub!\n", iface, debugstr_guid(iid), out);
    return E_NOINTERFACE;
}

void STDMETHODCALLTYPE AMDExtStub2_stub1(IAmdExtStub2 *iface)
{
    FIXME("%p stub!\n", iface);
}

void STDMETHODCALLTYPE AMDExtStub2_stub2(IAmdExtStub2 *iface, unsigned int unk)
{
    FIXME("%p %u stub!\n", iface, unk);
}

void STDMETHODCALLTYPE AMDExtStub2_stub3(IAmdExtStub2 *iface)
{
    FIXME("%p stub!\n", iface);
}

const static struct IAmdExtStub2Vtbl AMDSTUB2_vtable = {
    AMDExtStub2_QueryInterface,
    AMDExtStub2_AddRef,
    AMDExtStub2_Release,
    AMDExtStub2_stub1,
    AMDExtStub2_stub2,
    AMDExtStub2_stub3
};

struct AMDExtStub1
{
    IAmdExtStub1 IAmdExtStub1_iface;
};

struct AMDExtStub1* impl_from_IAMDExtStub1(IAmdExtStub1 *iface)
{
    return CONTAINING_RECORD(iface, struct AMDExtStub1, IAmdExtStub1_iface);
}

ULONG STDMETHODCALLTYPE AMDExtStub1_AddRef(IAmdExtStub1 *iface)
{
    return 2;
}

ULONG STDMETHODCALLTYPE AMDExtStub1_Release(IAmdExtStub1 *iface)
{
    return 1;
}

HRESULT STDMETHODCALLTYPE AmdExtStub1_QueryInterface2(IAmdExtStub1 *iface, void* unk, REFIID iid, void **out)
{
    TRACE("%p %p %s %p\n", iface, unk, debugstr_guid(iid), out);

    *out = NULL;
    if(IsEqualGUID(iid, &IID_IAmdExtStub2))
    {
        struct AMDExtStub2 *this = calloc(1, sizeof(struct AMDExtStub2));

        this->IAmdExtStub2_iface.lpVtbl = &AMDSTUB2_vtable;
        this->ref = 1;
        *out = &this->IAmdExtStub2_iface;
        return S_OK;
    } else {
        FIXME("unknown guid %s\n", debugstr_guid(iid));
    }

    return E_NOINTERFACE;
}

HRESULT STDMETHODCALLTYPE AmdExtStub1_QueryInterface(IAmdExtStub1 *iface, REFIID iid, void **out)
{
    return AmdExtStub1_QueryInterface2(iface, NULL, iid, out);
}

static const struct IAmdExtStub1Vtbl AMDSTUB1_vtable = {
    AmdExtStub1_QueryInterface,
    AMDExtStub1_AddRef,
    AMDExtStub1_Release,
    AmdExtStub1_QueryInterface2
};

static struct AMDExtStub1 amd_ext_stub1 =
{
    .IAmdExtStub1_iface = { &AMDSTUB1_vtable },
};

LUID get_luid(IUnknown *outer)
{
    LUID luid = {};
    ID3D12Device *device;
    HRESULT hr;

    /* XXX: can it be something else like d3d11? */
    hr = IUnknown_QueryInterface(outer, &IID_ID3D12Device, (void**)&device);
    if (hr != S_OK)
    {
        ERR("Failed to get ID3D12Device from outer: %#lx!\n", hr);
        return luid;
    }
    luid = ID3D12Device_GetAdapterLuid(device);
    TRACE("LUID = %08lx:%08lx\n", luid.HighPart, luid.LowPart);
    ID3D12Device_Release(device);

    return luid;
}

HRESULT CDECL AmdExtD3DCreateInterface(IUnknown *outer, REFIID iid, void **obj)
{
    TRACE("outer %p, iid %s, obj %p\n", outer, debugstr_guid(iid), obj);

    if (IsEqualGUID(iid, &IID_IAmdExtFfxApi))
    {
        struct AMDFSR4FFX* ffx = calloc(1, sizeof(struct AMDFSR4FFX));
        ffx->IAmdExtFfxApi_iface.lpVtbl = &AMDFSR4FFX_vtable;
        ffx->ref = 1;
        ffx->luid = get_luid(outer);
        *obj = &ffx->IAmdExtFfxApi_iface;
        return S_OK;
    } else if (IsEqualGUID(iid, &IID_IAmdExtAntiLagApi)) {
        return ID3D12Device_QueryInterface((ID3D12Device *)outer, &IID_IAmdExtAntiLagApi, obj);
    } else if(IsEqualGUID(iid, &IID_IAmdExtStub1)) {
        *obj = &amd_ext_stub1;
        return S_OK;
    } else {
        FIXME("unknown guid: %s\n", debugstr_guid(iid));
    }

    return E_NOINTERFACE;
}

HMODULE WINAPI AmdGetDxcModuleHandle(void)
{
    return GetModuleHandleA(NULL);
}
